/*
 * mqtt.h - Wrappers for MQTT operations (using Mosquitto)
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_MQTT_H
#define	LINZHI_LIBCOMMON_MQTT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>


#define	MQTT_DEFAULT_HOST	"localhost"
#define	MQTT_DEFAULT_PORT	1883


enum mqtt_qos {
	qos_be		= 0,
	qos_ack		= 1,
	qos_once	= 2
};


/*
 * Verbosity:
 * 0	only report fatal errors
 * 1	report disconnects, other warnings, and errors
 * 2	report progress and messages
 * 3	report publish acknowledgements
 */

extern int mqtt_verbose;


void mqtt_vprintf(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, va_list ap);
void mqtt_printf(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));
void mqtt_printf_arg(const char *topic, enum mqtt_qos qos, bool retain,
    const char *arg, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

/*
 * mqtt_last_will must be called before (!) mqtt_init.
 * topic == NULL clears the last will message.
 */
 
void mqtt_last_will(const char *topic, enum mqtt_qos qos, bool retain,
    const char *fmt, ...);

void mqtt_subscribe(const char *topic, enum mqtt_qos qos,
    void (*cb)(void *user, const char *topic, const char *msg), void *user,
    ...)
    __attribute__((format(printf, 1, 5)));

int mqtt_fd(void);
short mqtt_events(void);
void mqtt_poll(short revents);

/* run MQTT processing in a thread */

void mqtt_thread(void);

/* run in a loop, once or forever */

void mqtt_loop_once(int timeout_ms); /* -1 for the default of 1000 ms */
void mqtt_loop_forever(void) __attribute__((noreturn));

/* host may be NULL, port may be 0 */

void mqtt_init(const char *host, uint16_t port);
void mqtt_end(void);

#endif /* !LINZHI_LIBCOMMON_MQTT_H */
