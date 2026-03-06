// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020-2022, Red Hat, Inc.
 * All Rights Reserved.
 */
#ifndef XFS_IUNLINK_ITEM_H
#define XFS_IUNLINK_ITEM_H	1

struct xfs_trans;
struct xfs_inode;
struct xfs_perag;

static inline struct xfs_inode *
xfs_iunlink_lookup(struct xfs_perag *pag, xfs_agino_t agino)
{
	return NULL;
}

int xfs_iunlink_log_inode(struct xfs_trans *tp, struct xfs_inode *ip,
			struct xfs_perag *pag, xfs_agino_t next_agino);
int xfs_iunlink_reload_next(struct xfs_trans *tp, struct xfs_buf *agibp,
		xfs_agino_t prev_agino, xfs_agino_t next_agino);

#endif	/* XFS_IUNLINK_ITEM_H */
