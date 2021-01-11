/*
 * format.h - Check if printf-style format string matches pattern
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#ifndef LINZHI_LIBCOMMON_FORMAT_H
#define	LINZHI_LIBCOMMON_FORMAT_H

#include <stdbool.h>


/*
 * "fmt" is a printf-style format string. "fields" is a string of field types,
 * with one letter per fields. The following field types are recognized: d, u,
 * c, s, and p. Additionally, * is recognized as representing a * in a format
 * specification. E.g., format "%*.s" would yield true for fields "*s".
 */

bool format_compatible(const char *fmt, const char *fields);

#endif /* !LINZHI_LIBCOMMON_FORMAT_H */
