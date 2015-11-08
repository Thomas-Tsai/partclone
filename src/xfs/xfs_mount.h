/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __XFS_MOUNT_H__
#define __XFS_MOUNT_H__

struct xfs_inode;
struct xfs_buftarg;
struct xfs_dir_ops;
struct xfs_da_geometry;

/*
 * Define a user-level mount structure with all we need
 * in order to make use of the numerous XFS_* macros.
 */
typedef struct xfs_mount {
	xfs_sb_t		m_sb;		/* copy of fs superblock */
#define m_icount	m_sb.sb_icount
#define m_ifree		m_sb.sb_ifree
#define m_fdblocks	m_sb.sb_fdblocks
	char			*m_fsname;	/* filesystem name */
	int			m_bsize;	/* fs logical block size */
	xfs_agnumber_t		m_agfrotor;	/* last ag where space found */
	xfs_agnumber_t		m_agirotor;	/* last ag dir inode alloced */
	xfs_agnumber_t		m_maxagi;	/* highest inode alloc group */
	uint			m_rsumlevels;	/* rt summary levels */
	uint			m_rsumsize;	/* size of rt summary, bytes */
	struct xfs_inode	*m_rbmip;	/* pointer to bitmap inode */
	struct xfs_inode	*m_rsumip;	/* pointer to summary inode */
	struct xfs_buftarg	*m_ddev_targp;
	struct xfs_buftarg	*m_logdev_targp;
	struct xfs_buftarg	*m_rtdev_targp;
#define m_dev		m_ddev_targp
#define m_logdev	m_logdev_targp
#define m_rtdev		m_rtdev_targp
	__uint8_t		m_dircook_elog;	/* log d-cookie entry bits */
	__uint8_t		m_blkbit_log;	/* blocklog + NBBY */
	__uint8_t		m_blkbb_log;	/* blocklog - BBSHIFT */
	__uint8_t		m_sectbb_log;	/* sectorlog - BBSHIFT */
	__uint8_t		m_agno_log;	/* log #ag's */
	__uint8_t		m_agino_log;	/* #bits for agino in inum */
	uint			m_inode_cluster_size;/* min inode buf size */
	uint			m_blockmask;	/* sb_blocksize-1 */
	uint			m_blockwsize;	/* sb_blocksize in words */
	uint			m_blockwmask;	/* blockwsize-1 */
	uint			m_alloc_mxr[2];	/* XFS_ALLOC_BLOCK_MAXRECS */
	uint			m_alloc_mnr[2];	/* XFS_ALLOC_BLOCK_MINRECS */
	uint			m_bmap_dmxr[2];	/* XFS_BMAP_BLOCK_DMAXRECS */
	uint			m_bmap_dmnr[2];	/* XFS_BMAP_BLOCK_DMINRECS */
	uint			m_inobt_mxr[2];	/* XFS_INOBT_BLOCK_MAXRECS */
	uint			m_inobt_mnr[2];	/* XFS_INOBT_BLOCK_MINRECS */
	uint			m_ag_maxlevels;	/* XFS_AG_MAXLEVELS */
	uint			m_bm_maxlevels[2]; /* XFS_BM_MAXLEVELS */
	uint			m_in_maxlevels;	/* XFS_IN_MAXLEVELS */
	struct radix_tree_root	m_perag_tree;
	uint			m_flags;	/* global mount flags */
	uint			m_qflags;	/* quota status flags */
	uint			m_attroffset;	/* inode attribute offset */
	int			m_ialloc_inos;	/* inodes in inode allocation */
	int			m_ialloc_blks;	/* blocks in inode allocation */
	int			m_ialloc_min_blks; /* min blocks in sparse inode
						    * allocation */
	int			m_litino;	/* size of inode union area */
	int			m_inoalign_mask;/* mask sb_inoalignmt if used */
	struct xfs_trans_resv	m_resv;		/* precomputed res values */
	__uint64_t		m_maxicount;	/* maximum inode count */
	int			m_dalign;	/* stripe unit */
	int			m_swidth;	/* stripe width */
	int			m_sinoalign;	/* stripe unit inode alignmnt */
	const struct xfs_nameops *m_dirnameops;	/* vector of dir name ops */

	struct xfs_da_geometry	*m_dir_geo;	/* directory block geometry */
	struct xfs_da_geometry	*m_attr_geo;	/* attribute block geometry */
	const struct xfs_dir_ops *m_dir_inode_ops; /* vector of dir inode ops */
	const struct xfs_dir_ops *m_nondir_inode_ops; /* !dir inode ops */
#define M_DIROPS(mp)	((mp)->m_dir_inode_ops)

	/*
	 * anonymous struct to allow xfs_dquot_buf.c to compile.
	 * Pointer is always null in userspace, so code does not use it at all
	 */
	struct {
		int	qi_dqperchunk;
	}			*m_quotainfo;

} xfs_mount_t;

/*
 * Per-ag incore structure, copies of information in agf and agi,
 * to improve the performance of allocation group selection.
 */
typedef struct xfs_perag {
	struct xfs_mount *pag_mount;	/* owner filesystem */
	xfs_agnumber_t	pag_agno;	/* AG this structure belongs to */
	atomic_t	pag_ref;	/* perag reference count */
	char		pagf_init;	/* this agf's entry is initialized */
	char		pagi_init;	/* this agi's entry is initialized */
	char		pagf_metadata;	/* the agf is preferred to be metadata */
	char		pagi_inodeok;	/* The agi is ok for inodes */
	__uint8_t	pagf_levels[XFS_BTNUM_AGF];
					/* # of levels in bno & cnt btree */
	__uint32_t	pagf_flcount;	/* count of blocks in freelist */
	xfs_extlen_t	pagf_freeblks;	/* total free blocks */
	xfs_extlen_t	pagf_longest;	/* longest free space */
	__uint32_t	pagf_btreeblks;	/* # of blocks held in AGF btrees */
	xfs_agino_t	pagi_freecount;	/* number of free inodes */
	xfs_agino_t	pagi_count;	/* number of allocated inodes */

	/*
	 * Inode allocation search lookup optimisation.
	 * If the pagino matches, the search for new inodes
	 * doesn't need to search the near ones again straight away
	 */
	xfs_agino_t	pagl_pagino;
	xfs_agino_t	pagl_leftrec;
	xfs_agino_t	pagl_rightrec;
	int		pagb_count;	/* pagb slots in use */
} xfs_perag_t;

#define LIBXFS_MOUNT_DEBUGGER		0x0001
#define LIBXFS_MOUNT_32BITINODES	0x0002
#define LIBXFS_MOUNT_32BITINOOPT	0x0004
#define LIBXFS_MOUNT_COMPAT_ATTR	0x0008
#define LIBXFS_MOUNT_ATTR2		0x0010

#define LIBXFS_BHASHSIZE(sbp) 		(1<<10)

extern xfs_mount_t	*libxfs_mount (xfs_mount_t *, xfs_sb_t *,
				dev_t, dev_t, dev_t, int);
extern void	libxfs_umount (xfs_mount_t *);
extern void	libxfs_rtmount_destroy (xfs_mount_t *);

#endif	/* __XFS_MOUNT_H__ */

