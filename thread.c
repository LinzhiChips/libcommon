/*
 * thread.c - Wrappers for common pthreads operations
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#define	_GNU_SOURCE	/* for pthread_setname_np, vasprintf */
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

#include "alloc.h"
#include "thread.h"


#define	MAX_THREAD_NAME_LEN	15


unsigned lock_timeout_s = DEFAULT_LOCK_TIMEOUT_S;


static void get_time(struct timespec *t)
{
	if (clock_gettime(CLOCK_REALTIME, t) < 0) {
		perror("clock_gettime CLOCK_REALTIME");
		exit(1);
	}
}


void lock_tracking(pthread_mutex_t *mutex, const char *file, unsigned line)
{
	struct timespec timeout, now;
	int err;

	get_time(&timeout);
	timeout.tv_sec += lock_timeout_s;
	err = pthread_mutex_timedlock(mutex, &timeout);
	if (!err)
		return;
	if (err != ETIMEDOUT) {
		fprintf(stderr, "pthread_mutex_timedlock: %s\n", strerror(err));
		exit(1);
	}
	fprintf(stderr, "%s:%u: waiting for lock > %u s\n",
	    file, line, lock_timeout_s);
	err = pthread_mutex_lock(mutex);
	if (err) {
		fprintf(stderr, "pthread_mutex_lock: %s\n", strerror(err));
		exit(1);
	}
	get_time(&now);
	fprintf(stderr, "lock successfully acquired after: %.3f s\n",
	    (now.tv_sec - timeout.tv_sec + lock_timeout_s) +
	    (double) (now.tv_nsec - timeout.tv_nsec) * 1e-9);
}


bool trylock(pthread_mutex_t *mutex)
{
	int err;

	err = pthread_mutex_trylock(mutex);
	if (!err)
		return 1;
	if (err == EBUSY)
		return 0;
	fprintf(stderr, "pthread_mutex_trylock: %s\n", strerror(err));
	exit(1);
}


void unlock(pthread_mutex_t *mutex)
{
	int err;

	err = pthread_mutex_unlock(mutex);
	if (err) {
		fprintf(stderr, "pthread_unmutex_lock: %s\n", strerror(err));
		exit(1);
	}
}


void mutex_destroy(pthread_mutex_t *mutex)
{
	int err;

	err = pthread_mutex_destroy(mutex);
	if (err) {
		fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(err));
		exit(1);
	}
}


void wake_up(struct thread_wait *w)
{
	lock(&w->mutex);
	pthread_cond_signal(&w->cond);
	w->signaled = 1;
	unlock(&w->mutex);
}


void begin_wait(struct thread_wait *w)
{
	pthread_mutex_init(&w->mutex, NULL);
	pthread_cond_init(&w->cond, NULL);
	w->signaled = 0;
}


void wait_on(struct thread_wait *w)
{
	lock(&w->mutex);
	while (!w->signaled)
		pthread_cond_wait(&w->cond, &w->mutex);
	w->signaled = 0;
	unlock(&w->mutex);
}


void end_wait(struct thread_wait *w)
{
	int err;

	err = pthread_cond_destroy(&w->cond);
	if (err) {
		fprintf(stderr, "pthread_cond_destroy: %s\n", strerror(err));
		exit(1);
	}
	err = pthread_mutex_destroy(&w->mutex);
	if (err) {
		fprintf(stderr, "pthread_mutex_destroy: %s\n", strerror(err));
		exit(1);
	}
}


struct thread_data {
	void *(*fn)(void *arg);
	void *arg;
	char *name;
};


static void *thread_wrapper(void *arg)
{
	struct thread_data *data = arg;
	void *res;

	if (data->name) {
		int err;

		err = pthread_setname_np(pthread_self(), data->name);
		if (err) {
			fprintf(stderr, "pthread_setname_np: %s\n",
			    strerror(err));
			exit(1);
		}
		free(data->name);
	}
	/* could copy then free, and use tail-call optimization */
	res = data->fn(data->arg);
	free(data);
	return res;
}


pthread_t thread_create(void *(*fn)(void *arg), void *arg,
    const char *name, ...)
{
	va_list ap;
	struct thread_data *data;
	pthread_t thread;
	int err;

	va_start(ap, name);
	data = alloc_type(struct thread_data);
	data->fn = fn;
	data->arg = arg;

	if (name) {
		if (vasprintf(&data->name, name, ap) < 0) {
			perror("vasprintf");
			exit(1);
		}
		if (strlen(data->name) > MAX_THREAD_NAME_LEN) {
			fprintf(stderr, "warning: truncating \"%s\"\n",
			    data->name);
			data->name[MAX_THREAD_NAME_LEN] = 0;
		}
	} else {
		data->name = NULL;
	}

	err = pthread_create(&thread, NULL, thread_wrapper, data);
	if (err) {
		fprintf(stderr, "pthread_create: %s\n", strerror(err));
		exit(1);
	}
	va_end(ap);
	return thread;
}


void thread_detach(pthread_t thread)
{
	int err;

	err = pthread_detach(thread);
	if (err) {
		fprintf(stderr, "pthread_detach: %s\n", strerror(err));
		exit(1);
	}
}


void thread_cancel(pthread_t thread)
{
	int err;

	err = pthread_cancel(thread);
	if (err) {
		fprintf(stderr, "pthread_cancel: %s\n", strerror(err));
		exit(1);
	}
}


void *thread_join(pthread_t thread)
{
	void *ret;
	int err;

	err = pthread_join(thread, &ret);
	if (err) {
		fprintf(stderr, "pthread_join: %s\n", strerror(err));
		exit(1);
	}
	return ret;
}
