// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef __XFS_BUF_MEM_H__
#define __XFS_BUF_MEM_H__

extern unsigned int		XMBUF_BLOCKSIZE;
extern unsigned int		XMBUF_BLOCKSHIFT;

void xmbuf_libinit(void);

static inline bool xfs_buftarg_is_mem(const struct xfs_buftarg *target)
{
	return target->bt_xfile != NULL;
}

int xmbuf_alloc(struct xfs_mount *mp, const char *descr,
		unsigned long long maxpos, struct xfs_buftarg **btpp);
void xmbuf_free(struct xfs_buftarg *btp);

int xmbuf_map_page(struct xfs_buf *bp);
void xmbuf_unmap_page(struct xfs_buf *bp);

bool xmbuf_verify_daddr(struct xfs_buftarg *btp, xfs_daddr_t daddr);
void xmbuf_trans_bdetach(struct xfs_trans *tp, struct xfs_buf *bp);
int xmbuf_finalize(struct xfs_buf *bp);

static inline unsigned long long xmbuf_bytes(struct xfs_buftarg *btp)
{
	return xfile_bytes(btp->bt_xfile);
}

#endif /* __XFS_BUF_MEM_H__ */
