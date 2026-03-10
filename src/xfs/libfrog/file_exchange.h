/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2020-2024 Oracle.  All rights reserved.
 * All Rights Reserved.
 */
#ifndef __LIBFROG_FILE_EXCHANGE_H__
#define __LIBFROG_FILE_EXCHANGE_H__

void xfrog_exchangerange_prep(struct xfs_exchange_range *fxr,
		off_t file2_offset, int file1_fd,
		off_t file1_offset, uint64_t length);
int xfrog_exchangerange(int file2_fd, struct xfs_exchange_range *fxr,
		uint64_t flags);

int xfrog_commitrange_prep(struct xfs_commit_range *xcr, int file2_fd,
		off_t file2_offset, int file1_fd, off_t file1_offset,
		uint64_t length);
int xfrog_commitrange(int file2_fd, struct xfs_commit_range *xcr,
		uint64_t flags);

int xfrog_defragrange_prep(struct xfs_commit_range *xdf, int file2_fd,
		const struct xfs_bulkstat *file2_stat, int file1_fd);
int xfrog_defragrange(int file2_fd, struct xfs_commit_range *xdf);

#endif	/* __LIBFROG_FILE_EXCHANGE_H__ */
