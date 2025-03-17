/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef __BTRFS_MESSAGES_H__
#define __BTRFS_MESSAGES_H__

#include "kerncompat.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

/*
 * Workaround for custom format %pV that may not be supported on all libcs.
 */
#if defined(HAVE_PRINTF_H) \
	&& defined(HAVE_REGISTER_PRINTF_SPECIFIER) \
	&& defined(HAVE_REGISTER_PRINTF_MODIFIER)
#define DECLARE_PV(name)		struct va_format name
#define PV_FMT				"%pV"
#define PV_VAL(va)			&va
#define PV_ASSIGN(_va, _fmt, _args)	({ _va.fmt = (_fmt); _va.va = &(_args); })
#include <printf.h>
#else
#define PV_WORKAROUND
#define DECLARE_PV(name)		char name[1024]
#define PV_FMT				"%s"
#define PV_VAL(va)			va
#define PV_ASSIGN(_va, _fmt, _args)	vsnprintf(_va, 1024, _fmt, _args)
#endif

#ifdef DEBUG_VERBOSE_ERROR
#define	PRINT_VERBOSE_ERROR	fprintf(stderr, "%s:%d:", __FILE__, __LINE__)
#else
#define PRINT_VERBOSE_ERROR
#endif

#ifdef DEBUG_TRACE_ON_ERROR
#define PRINT_TRACE_ON_ERROR	print_trace()
#else
#define PRINT_TRACE_ON_ERROR	do { } while (0)
#endif

#ifdef DEBUG_ABORT_ON_ERROR
#define DO_ABORT_ON_ERROR	abort()
#else
#define DO_ABORT_ON_ERROR	do { } while (0)
#endif

#ifndef BTRFS_DISABLE_BACKTRACE
static inline void assert_trace(const char *assertion, const char *filename,
				const char *func, unsigned line, long val)
{
	if (val)
		return;
	fprintf(stderr,
		"%s:%d: %s: Assertion `%s` failed, value %ld\n",
		filename, line, func, assertion, val);
#ifndef BTRFS_DISABLE_BACKTRACE
	print_trace();
#endif
	abort();
	exit(1);
}

#define	UASSERT(c) assert_trace(#c, __FILE__, __func__, __LINE__, (long)(c))
#else
#define UASSERT(c) assert(c)
#endif

#define PREFIX_ERROR		"ERROR: "
#define PREFIX_WARNING		"WARNING: "

#define __btrfs_msg(prefix, fmt, ...)					\
	do {								\
		fputs((prefix), stderr);				\
		__btrfs_printf((fmt), ##__VA_ARGS__);			\
		fputc('\n', stderr);					\
	} while (0)

#define __btrfs_warning(fmt, ...) \
	__btrfs_msg(PREFIX_WARNING, fmt, ##__VA_ARGS__)

#define __btrfs_error(fmt, ...) \
	__btrfs_msg(PREFIX_ERROR, fmt, ##__VA_ARGS__)

#define internal_error(fmt, ...)						\
	do {									\
		__btrfs_msg("INTERNAL " PREFIX_ERROR, fmt, ##__VA_ARGS__);	\
		print_trace();							\
	} while (0)

#define error(fmt, ...)							\
	do {								\
		PRINT_TRACE_ON_ERROR;					\
		PRINT_VERBOSE_ERROR;					\
		__btrfs_error((fmt), ##__VA_ARGS__);			\
		DO_ABORT_ON_ERROR;					\
	} while (0)

#define error_on(cond, fmt, ...)					\
	do {								\
		if ((cond)) {						\
			PRINT_TRACE_ON_ERROR;				\
			PRINT_VERBOSE_ERROR;				\
			__btrfs_error((fmt), ##__VA_ARGS__);		\
			DO_ABORT_ON_ERROR;				\
		}							\
	} while (0)

#define error_btrfs_util(err)						\
	do {								\
		const char *errno_str = strerror(errno);		\
		const char *lib_str = btrfs_util_strerror(err);		\
		PRINT_TRACE_ON_ERROR;					\
		PRINT_VERBOSE_ERROR;					\
		if (lib_str && strcmp(errno_str, lib_str) != 0)		\
			__btrfs_error("%s: %m", lib_str);		\
		else							\
			__btrfs_error("%m");				\
		DO_ABORT_ON_ERROR;					\
	} while (0)

#define warning(fmt, ...)						\
	do {								\
		PRINT_TRACE_ON_ERROR;					\
		PRINT_VERBOSE_ERROR;					\
		__btrfs_warning((fmt), ##__VA_ARGS__);			\
	} while (0)

#define warning_on(cond, fmt, ...)					\
	do {								\
		if ((cond)) {						\
			PRINT_TRACE_ON_ERROR;				\
			PRINT_VERBOSE_ERROR;				\
			__btrfs_warning((fmt), ##__VA_ARGS__);		\
		}							\
	} while (0)

__attribute__ ((format (printf, 1, 2)))
void __btrfs_printf(const char *fmt, ...);

__attribute__ ((format (printf, 2, 3)))
void btrfs_no_printk(const void *fs_info, const char *fmt, ...);

/*
 * Level of messages that must be printed by default (in case the verbosity
 * options haven't been set by the user) due to backward compatibility reasons
 * where applications may expect the output.
 */
#define LOG_ALWAYS						(-1)
/*
 * Default level for any messages that should be printed by default, a one line
 * summary or with more details. Applications should not rely on such messages.
 */
#define LOG_DEFAULT						(1)
/*
 * Information about the ongoing actions, high level description
 */
#define LOG_INFO						(2)
/*
 * Verbose description and individual steps of the previous level
 */
#define LOG_VERBOSE						(3)
/*
 * Anything that should not be normally printed but can be useful for debugging
 */
#define LOG_DEBUG						(4)

__attribute__ ((format (printf, 2, 3)))
void pr_verbose(int level, const char *fmt, ...);

__attribute__ ((format (printf, 2, 3)))
void pr_stderr(int level, const char *fmt, ...);

/*
 * Commonly used errors
 */
enum common_error {
	ERROR_MSG_MEMORY,
	ERROR_MSG_START_TRANS,
	ERROR_MSG_COMMIT_TRANS,
	ERROR_MSG_UNEXPECTED,
	ERROR_MSG_READ,
	ERROR_MSG_WRITE,
};

__attribute__ ((format (printf, 2, 3)))
void error_msg(enum common_error error, const char *msg, ...);

#endif
