// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef __XFS_PLATFORM_DEFS_H__
#define __XFS_PLATFORM_DEFS_H__

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/param.h>
#include <limits.h>
#include <stdbool.h>
#include <libgen.h>
#include <urcu.h>

/* long and pointer must be either 32 bit or 64 bit */
#define BITS_PER_LONG (sizeof(long) * CHAR_BIT)

typedef unsigned short umode_t;

/* Define if you want gettext (I18N) support */
#ifdef ENABLE_GETTEXT
# include <libintl.h>
# define _(x)                   gettext(x)
# define N_(x)			 x
#else
# define _(x)                   (x)
# define N_(x)			 x
# define textdomain(d)          do { } while (0)
# define bindtextdomain(d,dir)  do { } while (0)
#endif
#include <locale.h>

#define IRIX_DEV_BITSMAJOR      14
#define IRIX_DEV_BITSMINOR      18
#define IRIX_DEV_MAXMAJ         0x1ff
#define IRIX_DEV_MAXMIN         0x3ffff
#define IRIX_DEV_MAJOR(dev)	((int)(((unsigned)(dev) >> IRIX_DEV_BITSMINOR) \
					& IRIX_DEV_MAXMAJ))
#define IRIX_DEV_MINOR(dev)	((int)((dev) & IRIX_DEV_MAXMIN))
#define IRIX_MKDEV(major,minor)	((xfs_dev_t)(((major) << IRIX_DEV_BITSMINOR) \
					| (minor&IRIX_DEV_MAXMIN)))
#define IRIX_DEV_TO_KDEVT(dev)	makedev(IRIX_DEV_MAJOR(dev),IRIX_DEV_MINOR(dev))

#ifndef min
#define min(a,b)	(((a)<(b))?(a):(b))
#define max(a,b)	(((a)>(b))?(a):(b))
#endif
#define max3(a,b,c)	max(max(a, b), c)

/* If param.h doesn't provide it, i.e. for Android */
#ifndef NBBY
#define NBBY 8
#endif

#ifdef DEBUG
# define ASSERT(EX)	assert(EX)
#else
# define ASSERT(EX)	((void) 0)
#endif

extern int	platform_nproc(void);

#define NSEC_PER_SEC	(1000000000ULL)
#define NSEC_PER_USEC	(1000ULL)

/* Simplified from version in include/linux/overflow.h */

/*
 * Compute a*b+c, returning SIZE_MAX on overflow. Internal helper for
 * struct_size() below.
 */
static inline size_t __ab_c_size(size_t a, size_t b, size_t c)
{
	return (a * b) + c;
}

#define __must_be_array(a) (0)

/**
 * struct_size() - Calculate size of structure with trailing array.
 * @p: Pointer to the structure.
 * @member: Name of the array member.
 * @count: Number of elements in the array.
 *
 * Calculates size of memory needed for structure @p followed by an
 * array of @count number of @member elements.
 *
 * Return: number of bytes needed or SIZE_MAX on overflow.
 */
#define struct_size(p, member, count)					\
	__ab_c_size(count,						\
		sizeof(*(p)->member) + __must_be_array((p)->member),	\
		sizeof(*(p)))

/**
 * struct_size_t() - Calculate size of structure with trailing flexible array
 * @type: structure type name.
 * @member: Name of the array member.
 * @count: Number of elements in the array.
 *
 * Calculates size of memory needed for structure @type followed by an
 * array of @count number of @member elements. Prefer using struct_size()
 * when possible instead, to keep calculations associated with a specific
 * instance variable of type @type.
 *
 * Return: number of bytes needed or SIZE_MAX on overflow.
 */
#define struct_size_t(type, member, count)					\
	struct_size((type *)NULL, member, count)

/*
 * Add the pseudo keyword 'fallthrough' so case statement blocks
 * must end with any of these keywords:
 *   break;
 *   fallthrough;
 *   continue;
 *   goto <label>;
 *   return [expression];
 *
 *  gcc: https://gcc.gnu.org/onlinedocs/gcc/Statement-Attributes.html#Statement-Attributes
 */
#if defined __has_attribute
#  if __has_attribute(__fallthrough__)
#    define fallthrough                    __attribute__((__fallthrough__))
#  else
#    define fallthrough                    do {} while (0)  /* fallthrough */
#  endif
#else
#    define fallthrough                    do {} while (0)  /* fallthrough */
#endif

/* Only needed for the kernel. */
#define __init

#ifdef __GNUC__
# define __return_address	__builtin_return_address(0)

/*
 * Return the address of a label.  Use barrier() so that the optimizer
 * won't reorder code to refactor the error jumpouts into a single
 * return, which throws off the reported address.
 */
# define __this_address	({ __label__ __here; __here: barrier(); &&__here; })
/* Optimization barrier */

/* The "volatile" is due to gcc bugs */
# ifndef barrier
#  define barrier() __asm__ __volatile__("": : :"memory")
# endif
#endif

/* Optimization barrier */
#ifndef barrier
# define barrier() __memory_barrier()
#endif

/* stuff from include/linux/kconfig.h */
#define __ARG_PLACEHOLDER_1 0,
#define __take_second_arg(__ignored, val, ...) val

/*
 * The use of "&&" / "||" is limited in certain expressions.
 * The following enable to calculate "and" / "or" with macro expansion only.
 */
#define __and(x, y)			___and(x, y)
#define ___and(x, y)			____and(__ARG_PLACEHOLDER_##x, y)
#define ____and(arg1_or_junk, y)	__take_second_arg(arg1_or_junk y, 0)

#define __or(x, y)			___or(x, y)
#define ___or(x, y)			____or(__ARG_PLACEHOLDER_##x, y)
#define ____or(arg1_or_junk, y)		__take_second_arg(arg1_or_junk 1, y)

/*
 * Helper macros to use CONFIG_ options in C/CPP expressions. Note that
 * these only work with boolean and tristate options.
 */

/*
 * Getting something that works in C and CPP for an arg that may or may
 * not be defined is tricky.  Here, if we have "#define CONFIG_BOOGER 1"
 * we match on the placeholder define, insert the "0," for arg1 and generate
 * the triplet (0, 1, 0).  Then the last step cherry picks the 2nd arg (a one).
 * When CONFIG_BOOGER is not defined, we generate a (... 1, 0) pair, and when
 * the last step cherry picks the 2nd arg, we get a zero.
 */
#define __is_defined(x)			___is_defined(x)
#define ___is_defined(val)		____is_defined(__ARG_PLACEHOLDER_##val)
#define ____is_defined(arg1_or_junk)	__take_second_arg(arg1_or_junk 1, 0)

/*
 * IS_BUILTIN(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y', 0
 * otherwise. For boolean options, this is equivalent to
 * IS_ENABLED(CONFIG_FOO).
 */
#define IS_BUILTIN(option) __is_defined(option)

/*
 * IS_MODULE(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'm', 0
 * otherwise.  CONFIG_FOO=m results in "#define CONFIG_FOO_MODULE 1" in
 * autoconf.h.
 */
#define IS_MODULE(option) __is_defined(option##_MODULE)

/*
 * IS_REACHABLE(CONFIG_FOO) evaluates to 1 if the currently compiled
 * code can call a function defined in code compiled based on CONFIG_FOO.
 * This is similar to IS_ENABLED(), but returns false when invoked from
 * built-in code when CONFIG_FOO is set to 'm'.
 */
#define IS_REACHABLE(option) __or(IS_BUILTIN(option), \
				__and(IS_MODULE(option), __is_defined(MODULE)))

/*
 * IS_ENABLED(CONFIG_FOO) evaluates to 1 if CONFIG_FOO is set to 'y' or 'm',
 * 0 otherwise.  Note that CONFIG_FOO=y results in "#define CONFIG_FOO 1" in
 * autoconf.h, while CONFIG_FOO=m results in "#define CONFIG_FOO_MODULE 1".
 */
#define IS_ENABLED(option) __or(IS_BUILTIN(option), IS_MODULE(option))

/* miscellaneous kernel routines not in user space */
#define likely(x)		(x)
#define unlikely(x)		(x)

#define __must_check	__attribute__((__warn_unused_result__))

/*
 * Allows for effectively applying __must_check to a macro so we can have
 * both the type-agnostic benefits of the macros while also being able to
 * enforce that the return value is, in fact, checked.
 */
static inline bool __must_check __must_check_overflow(bool overflow)
{
	return unlikely(overflow);
}

/*
 * For simplicity and code hygiene, the fallback code below insists on
 * a, b and *d having the same type (similar to the min() and max()
 * macros), whereas gcc's type-generic overflow checkers accept
 * different types. Hence we don't just make check_add_overflow an
 * alias for __builtin_add_overflow, but add type checks similar to
 * below.
 */
#define check_add_overflow(a, b, d) __must_check_overflow(({	\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	typeof(d) __d = (d);			\
	(void) (&__a == &__b);			\
	(void) (&__a == __d);			\
	__builtin_add_overflow(__a, __b, __d);	\
}))

#endif	/* __XFS_PLATFORM_DEFS_H__ */
