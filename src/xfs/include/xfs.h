// SPDX-License-Identifier: LGPL-2.1
/*
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
 */
#ifndef __XFS_H__
#define __XFS_H__

#if defined(__linux__)
#include <xfs/linux.h>
#else
# error unknown platform... have fun porting!
#endif

/*
 * make sure that any user of the xfs headers has a 64bit off_t type
 */
extern int xfs_assert_largefile[sizeof(off_t)-8];

/*
 * sparse kernel source annotations
 */
#ifndef __user
#define __user
#endif

/*
 * kernel struct packing shortcut
 */
#ifndef __packed
#define __packed __attribute__((packed))
#endif

#ifndef __maybe_unused
#define __maybe_unused __attribute__((__unused__))
#endif

#ifndef BUILD_BUG_ON
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))
#endif

/**
 * sizeof_field() - Report the size of a struct field in bytes
 *
 * @TYPE: The structure containing the field of interest
 * @MEMBER: The field to return the size of
 */
#define sizeof_field(TYPE, MEMBER) sizeof((((TYPE *)0)->MEMBER))

/**
 * offsetofend() - Report the offset of a struct field within the struct
 *
 * @TYPE: The type of the structure
 * @MEMBER: The member within the structure to get the end offset of
 */
#define offsetofend(TYPE, MEMBER) \
	(offsetof(TYPE, MEMBER)	+ sizeof_field(TYPE, MEMBER))

#include <xfs/xfs_types.h>
/* Include deprecated/compat pre-vfs xfs-specific symbols */
#include <xfs/xfs_fs_compat.h>
#include <xfs/xfs_fs.h>

#endif	/* __XFS_H__ */
