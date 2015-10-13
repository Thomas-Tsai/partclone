/*
 * util.h - utility definitions
 */
#ifndef __UTIL_H__
#define __UTIL_H__

#include <assert.h>
#include <stdlib.h>

#define BUG_ON(x)	   assert(!(x))
/* Force a compilation error if the condition is true */
#define BUILD_BUG_ON(condition) ((void)sizeof(struct { int: -!!(condition); }))

#define typecheck(type, x)					\
	({							\
	   type __dummy;					\
	   typeof(x) __dummy2;					\
	   (void)(&__dummy == &__dummy2);			\
	   1;							\
	})


#define DIV_ROUND_UP(n, m)	(((n) + (m) - 1) / (m))
#define ARRAY_SIZE(arr)		(sizeof(arr) / sizeof((arr)[0]))


#define min_t(type, x, y) \
	({ type __x = (x); type __y = (y); __x < __y ? __x : __y; })
#define max_t(type, x, y) \
	({ type __x = (x); type __y = (y); __x > __y ? __x : __y; })


#define cnt64_gt(a, b)						\
	(typecheck(__u64, a) && typecheck(__u64, b) &&		\
	 ((__s64)(b) - (__s64)(a) < 0))
#define cnt64_ge(a, b)						\
	(typecheck(__u64, a) && typecheck(__u64, b) &&		\
	 ((__s64)(a) - (__s64)(b) >= 0))
#define cnt64_lt(a, b)		cnt64_gt(b, a)
#define cnt64_le(a, b)		cnt64_ge(b, a)


#define timeval_to_timespec(tv, ts)		\
do {						\
	(ts)->tv_sec = (tv)->tv_sec;		\
	(ts)->tv_nsec = (tv)->tv_usec * 1000;	\
} while (0)

#endif /* __UTIL_H__ */
