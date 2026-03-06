// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef LIBFROG_DIV64_H_
#define LIBFROG_DIV64_H_

static inline int __do_div(unsigned long long *n, unsigned base)
{
	int __res;
	__res = (int)(((unsigned long) *n) % (unsigned) base);
	*n = ((unsigned long) *n) / (unsigned) base;
	return __res;
}

#define do_div(n,base)	(__do_div((unsigned long long *)&(n), (base)))
#define do_mod(a, b)		((a) % (b))
#define rol32(x,y)		(((x) << (y)) | ((x) >> (32 - (y))))

/**
 * div_u64_rem - unsigned 64bit divide with 32bit divisor with remainder
 * @dividend: unsigned 64bit dividend
 * @divisor: unsigned 32bit divisor
 * @remainder: pointer to unsigned 32bit remainder
 *
 * Return: sets ``*remainder``, then returns dividend / divisor
 *
 * This is commonly provided by 32bit archs to provide an optimized 64bit
 * divide.
 */
static inline uint64_t
div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

/**
 * div_u64 - unsigned 64bit divide with 32bit divisor
 * @dividend: unsigned 64bit dividend
 * @divisor: unsigned 32bit divisor
 *
 * This is the most common 64bit divide and should be used if possible,
 * as many 32bit archs can optimize this variant better than a full 64bit
 * divide.
 */
static inline uint64_t div_u64(uint64_t dividend, uint32_t divisor)
{
	uint32_t remainder;
	return div_u64_rem(dividend, divisor, &remainder);
}

/**
 * div64_u64_rem - unsigned 64bit divide with 64bit divisor and remainder
 * @dividend: unsigned 64bit dividend
 * @divisor: unsigned 64bit divisor
 * @remainder: pointer to unsigned 64bit remainder
 *
 * Return: sets ``*remainder``, then returns dividend / divisor
 */
static inline uint64_t
div64_u64_rem(uint64_t dividend, uint64_t divisor, uint64_t *remainder)
{
	*remainder = dividend % divisor;
	return dividend / divisor;
}

static inline uint64_t rounddown_64(uint64_t x, uint32_t y)
{
	do_div(x, y);
	return x * y;
}

static inline bool isaligned_64(uint64_t x, uint32_t y)
{
	return do_div(x, y) == 0;
}

static inline uint64_t
roundup_64(uint64_t x, uint32_t y)
{
	x += y - 1;
	do_div(x, y);
	return x * y;
}

static inline uint64_t
howmany_64(uint64_t x, uint32_t y)
{
	x += y - 1;
	do_div(x, y);
	return x;
}

static inline __attribute__((const))
int is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

#endif /* LIBFROG_DIV64_H_ */
