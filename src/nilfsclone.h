/**
 * nilfsclone.h - part of Partclone project
 *
 * Copyright (c) 2015~ Thomas Tsai <thomas at nchc org tw>
 *
 * read nilfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/types.h>
#include <endian.h>
#include <byteswap.h>

typedef __u64	__be64;
#define min_t(type, x, y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x : __y; })
