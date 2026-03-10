// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020-2022, Red Hat, Inc.
 * All Rights Reserved.
 */

#include "libxfs_priv.h"
#include "libxfs.h"
#include "libxfs_io.h"
#include "init.h"
#include "xfs_fs.h"
#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_trans_resv.h"
#include "xfs_mount.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_ag.h"
#include "iunlink.h"
#include "xfs_trace.h"

/* in memory log item structure */
struct xfs_iunlink_item {
	struct xfs_inode	*ip;
	struct xfs_perag	*pag;
	xfs_agino_t		next_agino;
	xfs_agino_t		old_agino;
};

/*
 * Look up the inode cluster buffer and log the on-disk unlinked inode change
 * we need to make.
 */
static int
xfs_iunlink_log_dinode(
	struct xfs_trans	*tp,
	struct xfs_iunlink_item	*iup)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_inode	*ip = iup->ip;
	struct xfs_dinode	*dip;
	struct xfs_buf		*ibp;
	int			offset;
	int			error;

	error = xfs_imap_to_bp(mp, tp, &ip->i_imap, &ibp);
	if (error)
		return error;
	/*
	 * Don't log the unlinked field on stale buffers as this may be the
	 * transaction that frees the inode cluster and relogging the buffer
	 * here will incorrectly remove the stale state.
	 */
	if (ibp->b_flags & LIBXFS_B_STALE)
		goto out;

	dip = xfs_buf_offset(ibp, ip->i_imap.im_boffset);

	/* Make sure the old pointer isn't garbage. */
	if (be32_to_cpu(dip->di_next_unlinked) != iup->old_agino) {
		xfs_inode_verifier_error(ip, -EFSCORRUPTED, __func__, dip,
				sizeof(*dip), __this_address);
		error = -EFSCORRUPTED;
		goto out;
	}

	trace_xfs_iunlink_update_dinode(mp, pag_agno(iup->pag),
					XFS_INO_TO_AGINO(mp, ip->i_ino),
					be32_to_cpu(dip->di_next_unlinked),
					iup->next_agino);

	dip->di_next_unlinked = cpu_to_be32(iup->next_agino);
	offset = ip->i_imap.im_boffset +
			offsetof(struct xfs_dinode, di_next_unlinked);

	xfs_dinode_calc_crc(mp, dip);
	xfs_trans_inode_buf(tp, ibp);
	xfs_trans_log_buf(tp, ibp, offset, offset + sizeof(xfs_agino_t) - 1);
	return 0;
out:
	xfs_trans_brelse(tp, ibp);
	return error;
}

/*
 * Initialize the inode log item for a newly allocated (in-core) inode.
 *
 * Inode extents can only reside within an AG. Hence specify the starting
 * block for the inode chunk by offset within an AG as well as the
 * length of the allocated extent.
 *
 * This joins the item to the transaction and marks it dirty so
 * that we don't need a separate call to do this, nor does the
 * caller need to know anything about the iunlink item.
 */
int
xfs_iunlink_log_inode(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	struct xfs_perag	*pag,
	xfs_agino_t		next_agino)
{
	struct xfs_iunlink_item	iup = {
		.ip		= ip,
		.pag		= pag,
		.next_agino	= next_agino,
		.old_agino	= ip->i_next_unlinked,
	};

	ASSERT(xfs_verify_agino_or_null(pag, next_agino));
	ASSERT(xfs_verify_agino_or_null(pag, ip->i_next_unlinked));

	/*
	 * Since we're updating a linked list, we should never find that the
	 * current pointer is the same as the new value, unless we're
	 * terminating the list.
	 */
	if (ip->i_next_unlinked == next_agino) {
		if (next_agino != NULLAGINO)
			return -EFSCORRUPTED;
		return 0;
	}

	return xfs_iunlink_log_dinode(tp, &iup);
}

/*
 * Load the inode @next_agino into the cache and set its prev_unlinked pointer
 * to @prev_agino.  Caller must hold the AGI to synchronize with other changes
 * to the unlinked list.
 */
int
xfs_iunlink_reload_next(
	struct xfs_trans	*tp,
	struct xfs_buf		*agibp,
	xfs_agino_t		prev_agino,
	xfs_agino_t		next_agino)
{
	struct xfs_perag	*pag = agibp->b_pag;
	struct xfs_mount	*mp = pag_mount(pag);
	struct xfs_inode	*next_ip = NULL;
	xfs_ino_t		ino;
	int			error;

	ASSERT(next_agino != NULLAGINO);

	ino = XFS_AGINO_TO_INO(mp, pag_agno(pag), next_agino);
	error = libxfs_iget(mp, tp, ino, XFS_IGET_UNTRUSTED, &next_ip);
	if (error)
		return error;

	/* If this is not an unlinked inode, something is very wrong. */
	if (VFS_I(next_ip)->i_nlink != 0) {
		error = -EFSCORRUPTED;
		goto rele;
	}

	next_ip->i_prev_unlinked = prev_agino;
	trace_xfs_iunlink_reload_next(next_ip);
rele:
	xfs_irele(next_ip);
	return error;
}
