// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef	__LIBXFS_DEFER_ITEM_H_
#define	__LIBXFS_DEFER_ITEM_H_

struct xfs_bmap_intent;

void xfs_bmap_defer_add(struct xfs_trans *tp, struct xfs_bmap_intent *bi);

enum xfs_attr_defer_op {
	XFS_ATTR_DEFER_SET,
	XFS_ATTR_DEFER_REMOVE,
	XFS_ATTR_DEFER_REPLACE,
};

void xfs_attr_defer_add(struct xfs_da_args *args, enum xfs_attr_defer_op op);

struct xfs_exchmaps_intent;

void xfs_exchmaps_defer_add(struct xfs_trans *tp,
		struct xfs_exchmaps_intent *xmi);

struct xfs_extent_free_item;
struct xfs_defer_pending;

void xfs_extent_free_defer_add(struct xfs_trans *tp,
		struct xfs_extent_free_item *xefi,
		struct xfs_defer_pending **dfpp);

struct xfs_rmap_intent;

void xfs_rmap_defer_add(struct xfs_trans *tp, struct xfs_rmap_intent *ri);

struct xfs_refcount_intent;

void xfs_refcount_defer_add(struct xfs_trans *tp,
		struct xfs_refcount_intent *ri);

#endif /* __LIBXFS_DEFER_ITEM_H_ */
