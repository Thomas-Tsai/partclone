// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef __LIBFROG_UTIL_H__
#define __LIBFROG_UTIL_H__

#include <sys/types.h>

unsigned int	log2_roundup(unsigned int i);
unsigned int	log2_rounddown(unsigned long long i);

#define min_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#define max_t(type,x,y) \
	({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })

void *memchr_inv(const void *start, int c, size_t bytes);

#endif /* __LIBFROG_UTIL_H__ */
