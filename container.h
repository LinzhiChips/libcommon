/*
 * container.h - Provide the container_of macro
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_CONTAINER_H
#define	LINZHI_LIBCOMMON_CONTAINER_H

#include <stddef.h>


// https://stackoverflow.com/a/10269766

#define container_of(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

#endif /* !LINZHI_LIBCOMMON_CONTAINER_H */
