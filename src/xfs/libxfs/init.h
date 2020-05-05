// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2006 Silicon Graphics, Inc.
 * All Rights Reserved.
 */
#ifndef LIBXFS_INIT_H
#define LIBXFS_INIT_H

struct stat;
extern int     use_xfs_buf_lock;

extern int platform_check_ismounted (char *path, char *block,
					struct stat *sptr, int verbose);
extern int platform_check_iswritable (char *path, char *block, struct stat *sptr);
extern int platform_set_blocksize (int fd, char *path, dev_t device, int bsz, int fatal);
extern void platform_flush_device (int fd, dev_t device);
extern char *platform_findrawpath(char *path);
extern char *platform_findrawpath (char *path);
extern char *platform_findblockpath (char *path);
extern int platform_direct_blockdev (void);
extern int platform_align_blockdev (void);
extern unsigned long platform_physmem(void);	/* in kilobytes */
extern void platform_findsizes(char *path, int fd, long long *sz, int *bsz);
extern int platform_nproc(void);

#endif	/* LIBXFS_INIT_H */
