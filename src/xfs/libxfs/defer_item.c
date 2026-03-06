// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 */
#include "libxfs_priv.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_da_format.h"
#include "xfs_trans_resv.h"
#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_alloc.h"
#include "xfs_rmap.h"
#include "xfs_refcount.h"
#include "xfs_bmap.h"
#include "xfs_inode.h"
#include "xfs_da_btree.h"
#include "xfs_attr.h"
#include "libxfs.h"
#include "defer_item.h"
#include "xfs_ag.h"
#include "xfs_exchmaps.h"
#include "defer_item.h"
#include "xfs_group.h"

/* Dummy defer item ops, since we don't do logging. */

/* Extent Freeing */

static inline struct xfs_extent_free_item *xefi_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_extent_free_item, xefi_list);
}

/* Sort bmap items by AG. */
static int
xfs_extent_free_diff_items(
	void				*priv,
	const struct list_head		*a,
	const struct list_head		*b)
{
	struct xfs_extent_free_item	*ra = xefi_entry(a);
	struct xfs_extent_free_item	*rb = xefi_entry(b);

	return ra->xefi_group->xg_gno - rb->xefi_group->xg_gno;
}

/* Get an EFI. */
static struct xfs_log_item *
xfs_extent_free_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	struct xfs_mount		*mp = tp->t_mountp;

	if (sort)
		list_sort(mp, items, xfs_extent_free_diff_items);
	return NULL;
}

/* Get an EFD so we can process all the free extents. */
static struct xfs_log_item *
xfs_extent_free_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return NULL;
}

static inline const struct xfs_defer_op_type *
xefi_ops(
	struct xfs_extent_free_item	*xefi)
{
	if (xfs_efi_is_realtime(xefi))
		return &xfs_rtextent_free_defer_type;
	if (xefi->xefi_agresv == XFS_AG_RESV_AGFL)
		return &xfs_agfl_free_defer_type;
	return &xfs_extent_free_defer_type;
}

/* Add this deferred EFI to the transaction. */
void
xfs_extent_free_defer_add(
	struct xfs_trans		*tp,
	struct xfs_extent_free_item	*xefi,
	struct xfs_defer_pending	**dfpp)
{
	struct xfs_mount		*mp = tp->t_mountp;

	trace_xfs_extent_free_defer(mp, xefi);

	xefi->xefi_group = xfs_group_intent_get(mp, xefi->xefi_startblock,
			xfs_efi_is_realtime(xefi) ? XG_TYPE_RTG : XG_TYPE_AG);
	*dfpp = xfs_defer_add(tp, &xefi->xefi_list, xefi_ops(xefi));
}

/* Cancel a free extent. */
STATIC void
xfs_extent_free_cancel_item(
	struct list_head		*item)
{
	struct xfs_extent_free_item	*xefi = xefi_entry(item);

	xfs_group_intent_put(xefi->xefi_group);
	kmem_cache_free(xfs_extfree_item_cache, xefi);
}

/* Process a free extent. */
STATIC int
xfs_extent_free_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_owner_info		oinfo = { };
	struct xfs_extent_free_item	*xefi = xefi_entry(item);
	xfs_agblock_t			agbno;
	int				error = 0;

	oinfo.oi_owner = xefi->xefi_owner;
	if (xefi->xefi_flags & XFS_EFI_ATTR_FORK)
		oinfo.oi_flags |= XFS_OWNER_INFO_ATTR_FORK;
	if (xefi->xefi_flags & XFS_EFI_BMBT_BLOCK)
		oinfo.oi_flags |= XFS_OWNER_INFO_BMBT_BLOCK;

	agbno = XFS_FSB_TO_AGBNO(tp->t_mountp, xefi->xefi_startblock);

	if (!(xefi->xefi_flags & XFS_EFI_CANCELLED)) {
		error = xfs_free_extent(tp, to_perag(xefi->xefi_group), agbno,
				xefi->xefi_blockcount, &oinfo,
				XFS_AG_RESV_NONE);
	}

	/*
	 * Don't free the XEFI if we need a new transaction to complete
	 * processing of it.
	 */
	if (error != -EAGAIN)
		xfs_extent_free_cancel_item(item);
	return error;
}

/* Abort all pending EFIs. */
STATIC void
xfs_extent_free_abort_intent(
	struct xfs_log_item		*intent)
{
}

const struct xfs_defer_op_type xfs_extent_free_defer_type = {
	.name		= "extent_free",
	.create_intent	= xfs_extent_free_create_intent,
	.abort_intent	= xfs_extent_free_abort_intent,
	.create_done	= xfs_extent_free_create_done,
	.finish_item	= xfs_extent_free_finish_item,
	.cancel_item	= xfs_extent_free_cancel_item,
};

STATIC int
xfs_rtextent_free_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_extent_free_item	*xefi = xefi_entry(item);
	int				error;

	error = xfs_rtfree_blocks(tp, to_rtg(xefi->xefi_group),
			xefi->xefi_startblock, xefi->xefi_blockcount);
	if (error != -EAGAIN)
		xfs_extent_free_cancel_item(item);
	return error;
}

const struct xfs_defer_op_type xfs_rtextent_free_defer_type = {
	.name		= "rtextent_free",
	.create_intent	= xfs_extent_free_create_intent,
	.abort_intent	= xfs_extent_free_abort_intent,
	.create_done	= xfs_extent_free_create_done,
	.finish_item	= xfs_rtextent_free_finish_item,
	.cancel_item	= xfs_extent_free_cancel_item,
};

/*
 * AGFL blocks are accounted differently in the reserve pools and are not
 * inserted into the busy extent list.
 */
STATIC int
xfs_agfl_free_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_owner_info		oinfo = { };
	struct xfs_mount		*mp = tp->t_mountp;
	struct xfs_extent_free_item	*xefi = xefi_entry(item);
	struct xfs_buf			*agbp;
	int				error;
	xfs_agblock_t			agbno;

	ASSERT(xefi->xefi_blockcount == 1);
	agbno = XFS_FSB_TO_AGBNO(mp, xefi->xefi_startblock);
	oinfo.oi_owner = xefi->xefi_owner;

	error = xfs_alloc_read_agf(to_perag(xefi->xefi_group), tp, 0, &agbp);
	if (!error)
		error = xfs_free_ag_extent(tp, agbp, agbno, 1, &oinfo,
				XFS_AG_RESV_AGFL);

	xfs_extent_free_cancel_item(item);
	return error;
}

/* sub-type with special handling for AGFL deferred frees */
const struct xfs_defer_op_type xfs_agfl_free_defer_type = {
	.name		= "agfl_free",
	.create_intent	= xfs_extent_free_create_intent,
	.abort_intent	= xfs_extent_free_abort_intent,
	.create_done	= xfs_extent_free_create_done,
	.finish_item	= xfs_agfl_free_finish_item,
	.cancel_item	= xfs_extent_free_cancel_item,
};

/* Reverse Mapping */

static inline struct xfs_rmap_intent *ri_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_rmap_intent, ri_list);
}

/* Sort rmap intents by AG. */
static int
xfs_rmap_update_diff_items(
	void				*priv,
	const struct list_head		*a,
	const struct list_head		*b)
{
	struct xfs_rmap_intent		*ra = ri_entry(a);
	struct xfs_rmap_intent		*rb = ri_entry(b);

	return ra->ri_group->xg_gno - rb->ri_group->xg_gno;
}

/* Get an RUI. */
static struct xfs_log_item *
xfs_rmap_update_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	 struct xfs_mount		*mp = tp->t_mountp;

	if (sort)
		list_sort(mp, items, xfs_rmap_update_diff_items);
	return NULL;
}

/* Get an RUD so we can process all the deferred rmap updates. */
static struct xfs_log_item *
xfs_rmap_update_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return NULL;
}

/* Add this deferred RUI to the transaction. */
void
xfs_rmap_defer_add(
	struct xfs_trans	*tp,
	struct xfs_rmap_intent	*ri)
{
	struct xfs_mount	*mp = tp->t_mountp;

	trace_xfs_rmap_defer(mp, ri);

	ri->ri_group = xfs_group_intent_get(mp, ri->ri_bmap.br_startblock,
			XG_TYPE_AG);
	xfs_defer_add(tp, &ri->ri_list, &xfs_rmap_update_defer_type);
}

/* Cancel a deferred rmap update. */
STATIC void
xfs_rmap_update_cancel_item(
	struct list_head		*item)
{
	struct xfs_rmap_intent		*ri = ri_entry(item);

	xfs_group_intent_put(ri->ri_group);
	kmem_cache_free(xfs_rmap_intent_cache, ri);
}

/* Process a deferred rmap update. */
STATIC int
xfs_rmap_update_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_rmap_intent		*ri = ri_entry(item);
	int				error;

	error = xfs_rmap_finish_one(tp, ri, state);

	xfs_rmap_update_cancel_item(item);
	return error;
}

/* Clean up after calling xfs_rmap_finish_one. */
STATIC void
xfs_rmap_finish_one_cleanup(
	struct xfs_trans	*tp,
	struct xfs_btree_cur	*rcur,
	int			error)
{
	struct xfs_buf		*agbp = NULL;

	if (rcur == NULL)
		return;
	agbp = rcur->bc_ag.agbp;
	xfs_btree_del_cursor(rcur, error);
	if (error && agbp)
		xfs_trans_brelse(tp, agbp);
}

/* Abort all pending RUIs. */
STATIC void
xfs_rmap_update_abort_intent(
	struct xfs_log_item		*intent)
{
}

const struct xfs_defer_op_type xfs_rmap_update_defer_type = {
	.name		= "rmap",
	.create_intent	= xfs_rmap_update_create_intent,
	.abort_intent	= xfs_rmap_update_abort_intent,
	.create_done	= xfs_rmap_update_create_done,
	.finish_item	= xfs_rmap_update_finish_item,
	.finish_cleanup = xfs_rmap_finish_one_cleanup,
	.cancel_item	= xfs_rmap_update_cancel_item,
};

/* Reference Counting */

static inline struct xfs_refcount_intent *ci_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_refcount_intent, ri_list);
}

/* Sort refcount intents by AG. */
static int
xfs_refcount_update_diff_items(
	void				*priv,
	const struct list_head		*a,
	const struct list_head		*b)
{
	struct xfs_refcount_intent	*ra = ci_entry(a);
	struct xfs_refcount_intent	*rb = ci_entry(b);

	return ra->ri_group->xg_gno - rb->ri_group->xg_gno;
}

/* Get an CUI. */
static struct xfs_log_item *
xfs_refcount_update_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	struct xfs_mount		*mp = tp->t_mountp;

	if (sort)
		list_sort(mp, items, xfs_refcount_update_diff_items);
	return NULL;
}

/* Get an CUD so we can process all the deferred refcount updates. */
static struct xfs_log_item *
xfs_refcount_update_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return NULL;
}

/* Add this deferred CUI to the transaction. */
void
xfs_refcount_defer_add(
	struct xfs_trans		*tp,
	struct xfs_refcount_intent	*ri)
{
	struct xfs_mount		*mp = tp->t_mountp;

	trace_xfs_refcount_defer(mp, ri);

	ri->ri_group = xfs_group_intent_get(mp, ri->ri_startblock,
			XG_TYPE_AG);
	xfs_defer_add(tp, &ri->ri_list, &xfs_refcount_update_defer_type);
}

/* Cancel a deferred refcount update. */
STATIC void
xfs_refcount_update_cancel_item(
	struct list_head		*item)
{
	struct xfs_refcount_intent	*ri = ci_entry(item);

	xfs_group_intent_put(ri->ri_group);
	kmem_cache_free(xfs_refcount_intent_cache, ri);
}

/* Process a deferred refcount update. */
STATIC int
xfs_refcount_update_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_refcount_intent	*ri = ci_entry(item);
	int				error;

	error = xfs_refcount_finish_one(tp, ri, state);

	/* Did we run out of reservation?  Requeue what we didn't finish. */
	if (!error && ri->ri_blockcount > 0) {
		ASSERT(ri->ri_type == XFS_REFCOUNT_INCREASE ||
		       ri->ri_type == XFS_REFCOUNT_DECREASE);
		return -EAGAIN;
	}

	xfs_refcount_update_cancel_item(item);
	return error;
}

/* Abort all pending CUIs. */
STATIC void
xfs_refcount_update_abort_intent(
	struct xfs_log_item		*intent)
{
}

/* Clean up after calling xfs_refcount_finish_one. */
STATIC void
xfs_refcount_finish_one_cleanup(
	struct xfs_trans	*tp,
	struct xfs_btree_cur	*rcur,
	int			error)
{
	struct xfs_buf		*agbp;

	if (rcur == NULL)
		return;
	agbp = rcur->bc_ag.agbp;
	xfs_btree_del_cursor(rcur, error);
	if (error)
		xfs_trans_brelse(tp, agbp);
}

const struct xfs_defer_op_type xfs_refcount_update_defer_type = {
	.name		= "refcount",
	.create_intent	= xfs_refcount_update_create_intent,
	.abort_intent	= xfs_refcount_update_abort_intent,
	.create_done	= xfs_refcount_update_create_done,
	.finish_item	= xfs_refcount_update_finish_item,
	.finish_cleanup = xfs_refcount_finish_one_cleanup,
	.cancel_item	= xfs_refcount_update_cancel_item,
};

/* Inode Block Mapping */

static inline struct xfs_bmap_intent *bi_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_bmap_intent, bi_list);
}

/* Sort bmap intents by inode. */
static int
xfs_bmap_update_diff_items(
	void				*priv,
	const struct list_head		*a,
	const struct list_head		*b)
{
	struct xfs_bmap_intent		*ba = bi_entry(a);
	struct xfs_bmap_intent		*bb = bi_entry(b);

	return ba->bi_owner->i_ino - bb->bi_owner->i_ino;
}

/* Get an BUI. */
static struct xfs_log_item *
xfs_bmap_update_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	struct xfs_mount		*mp = tp->t_mountp;

	if (sort)
		list_sort(mp, items, xfs_bmap_update_diff_items);
	return NULL;
}

/* Get an BUD so we can process all the deferred rmap updates. */
static struct xfs_log_item *
xfs_bmap_update_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return NULL;
}

/* Take a passive ref to the group containing the space we're mapping. */
static inline void
xfs_bmap_update_get_group(
	struct xfs_mount	*mp,
	struct xfs_bmap_intent	*bi)
{
	enum xfs_group_type	type = XG_TYPE_AG;

	if (xfs_ifork_is_realtime(bi->bi_owner, bi->bi_whichfork))
		type = XG_TYPE_RTG;

	/*
	 * Bump the intent count on behalf of the deferred rmap and refcount
	 * intent items that that we can queue when we finish this bmap work.
	 * This new intent item will bump the intent count before the bmap
	 * intent drops the intent count, ensuring that the intent count
	 * remains nonzero across the transaction roll.
	 */
	bi->bi_group = xfs_group_intent_get(mp, bi->bi_bmap.br_startblock,
				type);
}

/* Add this deferred BUI to the transaction. */
void
xfs_bmap_defer_add(
	struct xfs_trans	*tp,
	struct xfs_bmap_intent	*bi)
{
	xfs_bmap_update_get_group(tp->t_mountp, bi);

	/*
	 * Ensure the deferred mapping is pre-recorded in i_delayed_blks.
	 *
	 * Otherwise stat can report zero blocks for an inode that actually has
	 * data when the entire mapping is in the process of being overwritten
	 * using the out of place write path. This is undone in xfs_bmapi_remap
	 * after it has incremented di_nblocks for a successful operation.
	 */
	if (bi->bi_type == XFS_BMAP_MAP)
		bi->bi_owner->i_delayed_blks += bi->bi_bmap.br_blockcount;

	trace_xfs_bmap_defer(bi);
	xfs_defer_add(tp, &bi->bi_list, &xfs_bmap_update_defer_type);
}

/* Cancel a deferred bmap update. */
STATIC void
xfs_bmap_update_cancel_item(
	struct list_head		*item)
{
	struct xfs_bmap_intent		*bi = bi_entry(item);

	if (bi->bi_type == XFS_BMAP_MAP)
		bi->bi_owner->i_delayed_blks -= bi->bi_bmap.br_blockcount;

	xfs_group_intent_put(bi->bi_group);
	kmem_cache_free(xfs_bmap_intent_cache, bi);
}

/* Process a deferred rmap update. */
STATIC int
xfs_bmap_update_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_bmap_intent		*bi = bi_entry(item);
	int				error;

	error = xfs_bmap_finish_one(tp, bi);
	if (!error && bi->bi_bmap.br_blockcount > 0) {
		ASSERT(bi->bi_type == XFS_BMAP_UNMAP);
		return -EAGAIN;
	}

	xfs_bmap_update_cancel_item(item);
	return error;
}

/* Abort all pending BUIs. */
STATIC void
xfs_bmap_update_abort_intent(
	struct xfs_log_item		*intent)
{
}

const struct xfs_defer_op_type xfs_bmap_update_defer_type = {
	.name		= "bmap",
	.create_intent	= xfs_bmap_update_create_intent,
	.abort_intent	= xfs_bmap_update_abort_intent,
	.create_done	= xfs_bmap_update_create_done,
	.finish_item	= xfs_bmap_update_finish_item,
	.cancel_item	= xfs_bmap_update_cancel_item,
};

/* Logged extended attributes */

static inline struct xfs_attr_intent *attri_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_attr_intent, xattri_list);
}

/* Get an ATTRI. */
static struct xfs_log_item *
xfs_attr_create_intent(
	struct xfs_trans	*tp,
	struct list_head	*items,
	unsigned int		count,
	bool			sort)
{
	return NULL;
}

/* Abort all pending ATTRs. */
static void
xfs_attr_abort_intent(
	struct xfs_log_item	*intent)
{
}

/* Get an ATTRD so we can process all the attrs. */
static struct xfs_log_item *
xfs_attr_create_done(
	struct xfs_trans	*tp,
	struct xfs_log_item	*intent,
	unsigned int		count)
{
	return NULL;
}

static inline void
xfs_attr_free_item(
	struct xfs_attr_intent	*attr)
{
	if (attr->xattri_da_state)
		xfs_da_state_free(attr->xattri_da_state);
	if (attr->xattri_da_args->op_flags & XFS_DA_OP_RECOVERY)
		kfree(attr);
	else
		kmem_cache_free(xfs_attr_intent_cache, attr);
}

/* Process an attr. */
static int
xfs_attr_finish_item(
	struct xfs_trans	*tp,
	struct xfs_log_item	*done,
	struct list_head	*item,
	struct xfs_btree_cur	**state)
{
	struct xfs_attr_intent	*attr = attri_entry(item);
	struct xfs_da_args	*args;
	int			error;

	args = attr->xattri_da_args;

	/*
	 * Always reset trans after EAGAIN cycle
	 * since the transaction is new
	 */
	args->trans = tp;

	if (XFS_TEST_ERROR(false, args->dp->i_mount, XFS_ERRTAG_LARP)) {
		error = -EIO;
		goto out;
	}

	error = xfs_attr_set_iter(attr);
	if (!error && attr->xattri_dela_state != XFS_DAS_DONE)
		error = -EAGAIN;
out:
	if (error != -EAGAIN)
		xfs_attr_free_item(attr);

	return error;
}

/* Cancel an attr */
static void
xfs_attr_cancel_item(
	struct list_head	*item)
{
	struct xfs_attr_intent	*attr = attri_entry(item);

	xfs_attr_free_item(attr);
}

void
xfs_attr_defer_add(
	struct xfs_da_args	*args,
	enum xfs_attr_defer_op	op)
{
	struct xfs_attr_intent	*new;
	unsigned int		log_op = 0;
	bool			is_pptr = args->attr_filter & XFS_ATTR_PARENT;

	if (is_pptr) {
		ASSERT(xfs_has_parent(args->dp->i_mount));
		ASSERT((args->attr_filter & ~XFS_ATTR_PARENT) != 0);
		ASSERT(args->op_flags & XFS_DA_OP_LOGGED);
		ASSERT(args->valuelen == sizeof(struct xfs_parent_rec));
	}

	new = kmem_cache_zalloc(xfs_attr_intent_cache,
			GFP_NOFS | __GFP_NOFAIL);
	new->xattri_da_args = args;

	/* Compute log operation from the higher level op and namespace. */
	switch (op) {
	case XFS_ATTR_DEFER_SET:
		if (is_pptr)
			log_op = XFS_ATTRI_OP_FLAGS_PPTR_SET;
		else
			log_op = XFS_ATTRI_OP_FLAGS_SET;
		break;
	case XFS_ATTR_DEFER_REPLACE:
		if (is_pptr)
			log_op = XFS_ATTRI_OP_FLAGS_PPTR_REPLACE;
		else
			log_op = XFS_ATTRI_OP_FLAGS_REPLACE;
		break;
	case XFS_ATTR_DEFER_REMOVE:
		if (is_pptr)
			log_op = XFS_ATTRI_OP_FLAGS_PPTR_REMOVE;
		else
			log_op = XFS_ATTRI_OP_FLAGS_REMOVE;
		break;
	default:
		ASSERT(0);
		break;
	}
	new->xattri_op_flags = log_op;

	/* Set up initial attr operation state. */
	switch (log_op) {
	case XFS_ATTRI_OP_FLAGS_PPTR_SET:
	case XFS_ATTRI_OP_FLAGS_SET:
		new->xattri_dela_state = xfs_attr_init_add_state(args);
		break;
	case XFS_ATTRI_OP_FLAGS_PPTR_REPLACE:
		ASSERT(args->new_valuelen == args->valuelen);
		new->xattri_dela_state = xfs_attr_init_replace_state(args);
		break;
	case XFS_ATTRI_OP_FLAGS_REPLACE:
		new->xattri_dela_state = xfs_attr_init_replace_state(args);
		break;
	case XFS_ATTRI_OP_FLAGS_PPTR_REMOVE:
	case XFS_ATTRI_OP_FLAGS_REMOVE:
		new->xattri_dela_state = xfs_attr_init_remove_state(args);
		break;
	}

	xfs_defer_add(args->trans, &new->xattri_list, &xfs_attr_defer_type);
}

const struct xfs_defer_op_type xfs_attr_defer_type = {
	.name		= "attr",
	.max_items	= 1,
	.create_intent	= xfs_attr_create_intent,
	.abort_intent	= xfs_attr_abort_intent,
	.create_done	= xfs_attr_create_done,
	.finish_item	= xfs_attr_finish_item,
	.cancel_item	= xfs_attr_cancel_item,
};

/* File Mapping Exchanges */

STATIC struct xfs_log_item *
xfs_exchmaps_create_intent(
	struct xfs_trans		*tp,
	struct list_head		*items,
	unsigned int			count,
	bool				sort)
{
	return NULL;
}
STATIC struct xfs_log_item *
xfs_exchmaps_create_done(
	struct xfs_trans		*tp,
	struct xfs_log_item		*intent,
	unsigned int			count)
{
	return NULL;
}

/* Add this deferred XMI to the transaction. */
void
xfs_exchmaps_defer_add(
	struct xfs_trans		*tp,
	struct xfs_exchmaps_intent	*xmi)
{
	trace_xfs_exchmaps_defer(tp->t_mountp, xmi);

	xfs_defer_add(tp, &xmi->xmi_list, &xfs_exchmaps_defer_type);
}

static inline struct xfs_exchmaps_intent *xmi_entry(const struct list_head *e)
{
	return list_entry(e, struct xfs_exchmaps_intent, xmi_list);
}

/* Process a deferred swapext update. */
STATIC int
xfs_exchmaps_finish_item(
	struct xfs_trans		*tp,
	struct xfs_log_item		*done,
	struct list_head		*item,
	struct xfs_btree_cur		**state)
{
	struct xfs_exchmaps_intent	*xmi = xmi_entry(item);
	int				error;

	/*
	 * Exchange one more extent between the two files.  If there's still
	 * more work to do, we want to requeue ourselves after all other
	 * pending deferred operations have finished.  This includes all of the
	 * dfops that we queued directly as well as any new ones created in the
	 * process of finishing the others.
	 */
	error = xfs_exchmaps_finish_one(tp, xmi);
	if (error != -EAGAIN)
		kmem_cache_free(xfs_exchmaps_intent_cache, xmi);
	return error;
}

/* Abort all pending XMIs. */
STATIC void
xfs_exchmaps_abort_intent(
	struct xfs_log_item		*intent)
{
}

/* Cancel a deferred swapext update. */
STATIC void
xfs_exchmaps_cancel_item(
	struct list_head		*item)
{
	struct xfs_exchmaps_intent	*xmi = xmi_entry(item);

	kmem_cache_free(xfs_exchmaps_intent_cache, xmi);
}

const struct xfs_defer_op_type xfs_exchmaps_defer_type = {
	.name		= "exchmaps",
	.create_intent	= xfs_exchmaps_create_intent,
	.abort_intent	= xfs_exchmaps_abort_intent,
	.create_done	= xfs_exchmaps_create_done,
	.finish_item	= xfs_exchmaps_finish_item,
	.cancel_item	= xfs_exchmaps_cancel_item,
};
