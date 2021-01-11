/*
 * dtime.h - Delta time operations
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_DTIME_H
#define	LINZHI_LIBCOMMON_DTIME_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>


struct dtime {
	pthread_mutex_t mutex;
	struct timespec t0;
};


void dtime_get(struct timespec *t);

/*
 * Warning: if used to obtain cumulative intervals, dtime_set will only yield
 * correct results if no locking is needed. Else, use dtime_step_s to ensure
 * that no overlapping get/set sequences can occur.
 */

void dtime_set(struct dtime *dt, const struct timespec *t);
double dtime_s(struct dtime *dt, const struct timespec *t);
double dtime_step_s(struct dtime *dt, const struct timespec *t);
bool dtime_timeout(struct dtime *dt, double timeout_s,
    const struct timespec *now);

void dtime_reset(struct dtime *dt);
void dtime_init(struct dtime *dt);

#endif /* !LINZHI_LIBCOMMON_DTIME_H */
