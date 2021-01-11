/*
 * thread.h - Wrappers for common pthreads operations
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_THREAD_H
#define	LINZHI_LIBCOMMON_THREAD_H

#include <stdbool.h>
#include <pthread.h>


#define	DEFAULT_LOCK_TIMEOUT_S	600	/* 10 minutes */


struct thread_wait {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	bool signaled;
};


extern unsigned lock_timeout_s;


void lock_tracking(pthread_mutex_t *mutex, const char *file, unsigned line);
bool trylock(pthread_mutex_t *mutex);
void unlock(pthread_mutex_t *mutex);
void mutex_destroy(pthread_mutex_t *mutex);

#define	lock(mutex) lock_tracking(mutex, __FILE__, __LINE__)

void wake_up(struct thread_wait *w);

void begin_wait(struct thread_wait *w);
void wait_on(struct thread_wait *w);
void end_wait(struct thread_wait *w);

pthread_t thread_create(void *(*fn)(void *arg), void *arg,
    const char *name, ...)
    __attribute__((format(printf, 3, 4)));
void thread_detach(pthread_t thread);
void thread_cancel(pthread_t thread);
void *thread_join(pthread_t thread);

#endif /* !LINZHI_LIBCOMMON_THREAD_H */
