/*
 * format.c - Check if printf-style format string matches pattern
 *
 * Copyright (C) 2021 Linzhi Ltd.
 *
 * This work is licensed under the terms of the MIT License.
 * A copy of the license can be found in the file COPYING.txt
 */

#include <stdbool.h>

#include "linzhi/format.h"


bool format_compatible(const char *fmt, const char *fields)
{
	while (*fmt) {
		if (*fmt++ != '%')
			continue;
		while (*fmt++) {
			switch (fmt[-1]) {
			case 'c':
			case 'd':
			case 'p':
			case 's':
			case 'u':
				if (fmt[-1] != *fields++)
					return 0;
				break;
			case '%':
				break;
			case '*':
				if (*fields++ != '*')
					return 0;
				continue;
			case '0' ... '9':
			case '-':
			case '+':
			case '.':
			case ' ':
				continue;
			default:
				// unrecognized or invalid syntax
				return 0;
			}
			break;
		}
	}
	return !*fields;
}
