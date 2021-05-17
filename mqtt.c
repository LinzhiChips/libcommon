/*
 * mqtt.c - Wrappers for MQTT operations (using Mosquitto)
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define	_GNU_SOURCE	/* for asprintf, vasprintf */
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <pthread.h>
#include <assert.h>

#include <mosquitto.h>

#include "linzhi/alloc.h"

#include "thread.h"
#include "mqtt.h"


struct sub {
	const char	*topic;
	enum mqtt_qos	qos;
	void		(*cb)(void *user, const char *topic, const char *msg);
	void		*user;
	struct sub	*next;
};


int mqtt_verbose = 0;

static bool initialized = 0;
static struct mosquitto *mosq;
static struct mosquitto *mosq;
static pthread_mutex_t mutex;	/* protects "subs" and "connected" */
struct sub *subs = NULL;
static bool is_connected = 0;
static bool is_threaded = 0;
static bool shutting_down = 0;
static unsigned pub_enq = 0;
static volatile unsigned pub_ack = 0;
static char *will_topic = NULL;
static char *will_msg = NULL;
static enum mqtt_qos will_qos;
static bool will_retain;


/* ----- Synchronization --------------------------------------------------- */


static void published(struct mosquitto *m, void *obj, int mid)
{
	if (mqtt_verbose > 2)
		fprintf(stderr, "MQTT ACK\n");
	pub_ack++;
}


/* ----- Transmission ------------------------------------------------------ */


void mqtt_vprintf(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, va_list ap)
{
	char *s;
	int res;

	assert(initialized);
	if (vasprintf(&s, fmt, ap) < 0) {
		perror("vasprintf");
		exit(1);
	}
	if (mqtt_verbose > 1)
		fprintf(stderr, "MQTT \"%s\" -> \"%s\"\n", topic, s);
	pub_enq++;
	res = mosquitto_publish(mosq, NULL, topic, strlen(s), s, qos, retain);
	if (res != MOSQ_ERR_SUCCESS)
		fprintf(stderr, "warning: mosquitto_publish (%s): %s\n",
		    topic, mosquitto_strerror(res));
	free(s);
}


void mqtt_printf(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, ...)
{
	va_list ap;

	assert(!strchr(topic, '%'));
	va_start(ap, fmt);
	mqtt_vprintf(topic, qos, retain, fmt, ap);
	va_end(ap);
}


void mqtt_printf_arg(const char *topic, enum mqtt_qos qos, bool retain,
    const char *arg, const char *fmt, ...)
{
	va_list ap;
	char *t;

	assert(strchr(topic, '%'));
	if (asprintf(&t, topic, arg) < 0) {
		perror("asprintf");
		exit(1);
	}
	va_start(ap, fmt);
	mqtt_vprintf(t, qos, retain, fmt, ap);
	va_end(ap);
	free(t);
}


/* ----- Last will --------------------------------------------------------- */


void mqtt_last_will(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, ...)
{
	va_list ap;

	assert(!initialized);

	free(will_topic);
	free(will_msg);
	will_topic = will_msg = NULL;

	if (!topic)
		return;

	will_topic = stralloc(topic);
	va_start(ap, fmt);
	if (asprintf(&will_msg, fmt, ap) < 0) {
		perror("asprintf");
		exit(1);
	}
	va_end(ap);
	will_qos = qos;
	will_retain = retain;
}


/* ----- Subscriptions and reception --------------------------------------- */


static void message(struct mosquitto *m, void *user,
    const struct mosquitto_message *msg)
{
	const struct sub *sub;
	char *buf;

	assert(initialized);
	if (shutting_down)
		return;
	if (mqtt_verbose > 1)
		fprintf(stderr, "MQTT \"%s\": \"%.*s\"\n",
		    msg->topic, msg->payloadlen, (const char *) msg->payload);

	buf = alloc_size(msg->payloadlen + 1);
	memcpy(buf, msg->payload, msg->payloadlen);
	buf[msg->payloadlen] = 0;

	lock(&mutex);
	for (sub = subs; sub; sub = sub->next)
		if (!strcmp(sub->topic, msg->topic))
			sub->cb(sub->user, sub->topic, buf);
	unlock(&mutex);
	free(buf);
}


static void subscribe_one(const char *topic, enum mqtt_qos qos)
{
	int res;

	assert(initialized);
	res = mosquitto_subscribe(mosq, NULL, topic, qos);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "mosquitto_subscribe %s: %s\n",
		    topic, mosquitto_strerror(res));
		exit(1);
	}
}


void mqtt_subscribe(const char *topic, enum mqtt_qos qos,
    void (*cb)(void *user, const char *topic, const char *msg), void *user,
    ...)
{
	struct sub *sub;
	va_list ap;
	char *s;

	va_start(ap, user);
	if (vasprintf(&s, topic, ap) < 0) {
		perror("vasprintf");
		exit(1);
	}
	va_end(ap);

	sub = alloc_type(struct sub);
	sub->topic = s;
	sub->qos = qos;
	sub->cb = cb;
	sub->user = user;

	lock(&mutex);
	if (is_connected)
		subscribe_one(topic, qos);
	sub->next = subs;
	subs = sub;
	unlock(&mutex);
}


/* ----- Connect and disconnect -------------------------------------------- */


static void connected(struct mosquitto *m, void *data, int result)
{
	const struct sub *sub;

	assert(initialized);
	if (shutting_down)
		return;
	if (result) {
		fprintf(stderr, "MQTT connect failed: %s\n",
		    mosquitto_strerror(result));
		exit(1);
	}
	if (mqtt_verbose)
		fprintf(stderr, "MQTT connected\n");
	lock(&mutex);
	is_connected = 1;
	for (sub = subs; sub; sub = sub->next)
		subscribe_one(sub->topic, sub->qos);
	unlock(&mutex);
}


static void disconnected(struct mosquitto *m, void *data, int result)
{
	int res;

	assert(initialized);
	if (shutting_down)
		return;
	lock(&mutex);	/* for synchronization */
	is_connected = 0;
	unlock(&mutex);

	if (mqtt_verbose)
		fprintf(stderr,
		    "warning: reconnecting MQTT (disconnect reason %s)\n",
		    mosquitto_strerror(result));
	res = mosquitto_reconnect(mosq);
	if (res != MOSQ_ERR_SUCCESS)
		fprintf(stderr, "mosquitto_reconnect: %s\n",
		    mosquitto_strerror(res));
}


/* ----- Event loop -------------------------------------------------------- */


int mqtt_fd(void)
{
	assert(initialized);
	return mosquitto_socket(mosq);
}


short mqtt_events(void)
{
	assert(initialized);
	return POLLHUP | POLLERR | POLLIN |
	    (mosquitto_want_write(mosq) ? POLLOUT : 0);
}


void mqtt_poll(short revents)
{
	int res;

	assert(initialized);
	if (revents & POLLIN) {
		res = mosquitto_loop_read(mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && mqtt_verbose)
			fprintf(stderr, "warning: mosquitto_loop_read: %s\n",
			    mosquitto_strerror(res));
	}
	if (revents & POLLOUT) {
		res = mosquitto_loop_write(mosq, 1);
		if (res != MOSQ_ERR_SUCCESS && mqtt_verbose)
			fprintf(stderr, "warning: mosquitto_loop_write: %s\n",
			    mosquitto_strerror(res));
	}
	res = mosquitto_loop_misc(mosq);
	if (res != MOSQ_ERR_SUCCESS && mqtt_verbose)
		fprintf(stderr, "warning: mosquitto_loop_misc: %s\n",
		    mosquitto_strerror(res));
}


void mqtt_thread(void)
{
	int res;

	assert(initialized);
	res = mosquitto_loop_start(mosq);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "mosquitto_loop_start: %s\n",
		    mosquitto_strerror(res));
		exit(1);
	}
	is_threaded = 1;
}


void mqtt_loop_once(int timeout_ms)
{
	int res;

	assert(initialized);
	res = mosquitto_loop_forever(mosq, timeout_ms, 1);
	if (res == MOSQ_ERR_SUCCESS)
		return;

	fprintf(stderr, "mosquitto_loop_once: %s\n",
	    mosquitto_strerror(res));
	exit(1);
}


void mqtt_loop_forever(void)
{
	while (1)
		mqtt_loop_once(-1);
}


/* ----- Initialization and shutdown---------------------------------------- */


void mqtt_init(const char *host, uint16_t port)
{
	int res;

	mosquitto_lib_init();
	mosq = mosquitto_new(NULL, 1, NULL);
	if (!mosq) {
		fprintf(stderr, "mosquitto_new failed\n");
		exit(1);
	}

	mosquitto_connect_callback_set(mosq, connected);
	mosquitto_disconnect_callback_set(mosq, disconnected);
	mosquitto_message_callback_set(mosq, message);
	mosquitto_publish_callback_set(mosq, published);

	pthread_mutex_init(&mutex, NULL);

	if (will_topic) {
		if (mqtt_verbose > 1)
			fprintf(stderr, "WILL \"%s\" -> \"%s\"\n",
			    will_topic, will_msg);
		res = mosquitto_will_set(mosq, will_topic,
		    strlen(will_msg), will_msg, will_qos, will_retain);
		if (res != MOSQ_ERR_SUCCESS)
			fprintf(stderr,
			    "warning: mosquitto_set_will (%s): %s\n",
			    will_topic, mosquitto_strerror(res));
	}

	initialized = 1;

	res = mosquitto_connect(mosq, host ? host : MQTT_DEFAULT_HOST,
	    port ? port : MQTT_DEFAULT_PORT, 3600);
	if (res != MOSQ_ERR_SUCCESS) {
		fprintf(stderr, "mosquitto_connect: %s\n",
		    mosquitto_strerror(res));
		exit(1);
	}
}


void mqtt_end(void)
{
	unsigned i;
	int res;

	shutting_down = 1;
	/* retry for up to approximately one second */
	for (i = 0; i != 100; i++) {
		if (pub_enq == pub_ack)
			break;
		usleep(10000);
	}
	if (is_connected) {
		res = mosquitto_disconnect(mosq);
		if (res != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "mosquitto_disconnect: %s\n",
			    mosquitto_strerror(res));
			exit(1);
        	}
		is_connected = 0;
	}
	if (is_threaded) {
		res = mosquitto_loop_stop(mosq, 0);
		if (res != MOSQ_ERR_SUCCESS) {
			fprintf(stderr, "mosquitto_loop_stop: %s\n",
			    mosquitto_strerror(res));
			exit(1);
        	}
		is_threaded = 0;
	}
	mosquitto_destroy(mosq);
	mosq = NULL;
	mosquitto_lib_cleanup();
	initialized = 0;
	shutting_down = 0;
}
