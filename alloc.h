/*
 * alloc.h - Common allocation functions
 *
 * Copyright (C) 2021, 2022 Linzhi Ltd.
 * Copyright 2016 Werner Almesberger
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_ALLOC_H
#define LINZHI_LIBCOMMON_ALLOC_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>


#define alloc_size(s)					\
    ({	void *alloc_size_tmp = malloc(s);		\
	if (!alloc_size_tmp) {				\
		perror("malloc");			\
		exit(1);				\
	}						\
	alloc_size_tmp; })

#define alloc_type(t) ((t *) alloc_size(sizeof(t)))
#define alloc_type_n(t, n) ((t *) alloc_size(sizeof(t) * (n)))


#define realloc_size(p, s)				\
    ({ void *alloc_size_tmp = realloc((p), (s));	\
	if (!alloc_size_tmp) {				\
		perror("realloc");			\
		exit(1);				\
	}						\
	alloc_size_tmp; })

#define realloc_type_n(p, n) \
    ((typeof(p)) realloc_size((p), sizeof(*(p)) * (n)))


#define stralloc(s)					\
    ({	char *stralloc_tmp = strdup(s);			\
	if (!stralloc_tmp) {				\
		perror("strdup");			\
		exit(1);				\
	}						\
	stralloc_tmp; })

#define strnalloc(s, n)					\
    ({ char *strnalloc_tmp = alloc_size((n) + 1);	\
       memcpy(strnalloc_tmp, s, n);			\
       strnalloc_tmp[n] = 0;				\
       strnalloc_tmp; })


static inline char *stralloc_append(char *a, const char *b)
{
	size_t len_a = a ? strlen(a) : 0;
	size_t len_b = strlen(b);
	size_t len = len_a + len_b;

	a = realloc_size(a, len + 1);
	memcpy(a + len_a, b, len_b + 1);
	return a;
}

#endif /* !LINZHI_LIBCOMMON_ALLOC_H */
