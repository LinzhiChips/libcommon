/*
 * dtime.c - Delta time operations
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include "thread.h"

#include "dtime.h"


void dtime_get(struct timespec *t)
{
	if (clock_gettime(CLOCK_BOOTTIME, t) < 0) {
		perror("clock_gettime");
		exit(1);
	}
}


void dtime_set(struct dtime *dt, const struct timespec *t)
{
	struct timespec now;

	if (!t) {
		dtime_get(&now);
		t = &now;
	}

	lock(&dt->mutex);
	dt->t0 = *t;
	unlock(&dt->mutex);
}


static double delta(struct dtime *dt, const struct timespec *t)
{
	return (t->tv_sec - dt->t0.tv_sec) +
	    (t->tv_nsec - dt->t0.tv_nsec) * 1e-9;
}


double dtime_s(struct dtime *dt, const struct timespec *t)
{
	struct timespec now;
	double d;

	if (!t) {
		dtime_get(&now);
		t = &now;
	}
	if (dt) {
		lock(&dt->mutex);
		d = delta(dt, t);
		unlock(&dt->mutex);
	} else {
		return t->tv_sec + t->tv_nsec * 1e-9;
	}
	return d;
}


double dtime_step_s(struct dtime *dt, const struct timespec *t)
{
	struct timespec now;
	double d;

	if (!t) {
		dtime_get(&now);
		t = &now;
	}
	lock(&dt->mutex);
	d = delta(dt, t);
	dt->t0 = *t;
	unlock(&dt->mutex);
	return d;
}


bool dtime_timeout(struct dtime *dt, double timeout_s, const struct timespec *t)
{
	struct timespec now;
	bool to;

	if (!t) {
		dtime_get(&now);
		t = &now;
	}
	lock(&dt->mutex);
	to = delta(dt, t) >= timeout_s;
	if (to)
		dt->t0 = *t;
	unlock(&dt->mutex);
	return to;
}


void dtime_reset(struct dtime *dt)
{
	struct timespec ts;

	dtime_get(&ts);
	dtime_set(dt, &ts);
}


void dtime_init(struct dtime *dt)
{
	pthread_mutex_init(&dt->mutex, NULL);
	dtime_reset(dt);
}
