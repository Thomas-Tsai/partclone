// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#include "platform_defs.h"
#include "util.h"

/*
 * libfrog is a collection of miscellaneous userspace utilities.
 * It's a library of Funny Random Oddball Gunk <cough>.
 */

unsigned int
log2_roundup(unsigned int i)
{
	unsigned int	rval;

	for (rval = 0; rval < NBBY * sizeof(i); rval++) {
		if ((1 << rval) >= i)
			break;
	}
	return rval;
}

void *
memchr_inv(const void *start, int c, size_t bytes)
{
	const unsigned char	*p = start;

	while (bytes > 0) {
		if (*p != (unsigned char)c)
			return (void *)p;
		bytes--;
	}

	return NULL;
}

unsigned int
log2_rounddown(unsigned long long i)
{
	int	rval;

	for (rval = NBBY * sizeof(i) - 1; rval >= 0; rval--) {
		if ((1ULL << rval) < i)
			break;
	}
	return rval;
}
