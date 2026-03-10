// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
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
#include "xfs_defer.h"
#include "xfs_inode_buf.h"
#include "xfs_inode_fork.h"
#include "xfs_inode.h"
#include "xfs_trans.h"
#include "xfs_bmap.h"
#include "xfs_bmap_btree.h"
#include "xfs_trans_space.h"
#include "xfs_ialloc.h"
#include "xfs_alloc.h"
#include "xfs_bit.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_dir2_priv.h"

/*
 * Initialise a newly allocated inode and return the in-core inode to the
 * caller locked exclusively.
 */
int
libxfs_icreate(
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	const struct xfs_icreate_args *args,
	struct xfs_inode	**ipp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_inode	*ip = NULL;
	int			error;

	error = libxfs_iget(mp, tp, ino, XFS_IGET_CREATE, &ip);
	if (error)
		return error;

	ASSERT(ip != NULL);
	xfs_trans_ijoin(tp, ip, 0);
	xfs_inode_init(tp, args, ip);

	*ipp = ip;
	return 0;
}

/*
 * Writes a modified inode's changes out to the inode's on disk home.
 * Originally based on xfs_iflush_int() from xfs_inode.c in the kernel.
 */
int
libxfs_iflush_int(
	struct xfs_inode		*ip,
	struct xfs_buf			*bp)
{
	struct xfs_inode_log_item	*iip;
	struct xfs_dinode		*dip;
	struct xfs_mount		*mp;

	ASSERT(ip->i_df.if_format != XFS_DINODE_FMT_BTREE ||
		ip->i_df.if_nextents > ip->i_df.if_ext_max);

	iip = ip->i_itemp;
	mp = ip->i_mount;

	/* set *dip = inode's place in the buffer */
	dip = xfs_buf_offset(bp, ip->i_imap.im_boffset);

	if (XFS_ISREG(ip)) {
		ASSERT( (ip->i_df.if_format == XFS_DINODE_FMT_EXTENTS) ||
			(ip->i_df.if_format == XFS_DINODE_FMT_BTREE) );
	} else if (XFS_ISDIR(ip)) {
		ASSERT( (ip->i_df.if_format == XFS_DINODE_FMT_EXTENTS) ||
			(ip->i_df.if_format == XFS_DINODE_FMT_BTREE)   ||
			(ip->i_df.if_format == XFS_DINODE_FMT_LOCAL) );
	}
	ASSERT(ip->i_df.if_nextents+ip.i_af->if_nextents <= ip->i_nblocks);
	ASSERT(ip->i_forkoff <= mp->m_sb.sb_inodesize);

	/* bump the change count on v3 inodes */
	if (xfs_has_v3inodes(mp))
		VFS_I(ip)->i_version++;

	/*
	 * If there are inline format data / attr forks attached to this inode,
	 * make sure they are not corrupt.
	 */
	if (ip->i_df.if_format == XFS_DINODE_FMT_LOCAL &&
	    xfs_ifork_verify_local_data(ip))
		return -EFSCORRUPTED;
	if (xfs_inode_has_attr_fork(ip) &&
	    ip->i_af.if_format == XFS_DINODE_FMT_LOCAL &&
	    xfs_ifork_verify_local_attr(ip))
		return -EFSCORRUPTED;

	/*
	 * Copy the dirty parts of the inode into the on-disk
	 * inode.  We always copy out the core of the inode,
	 * because if the inode is dirty at all the core must
	 * be.
	 */
	xfs_inode_to_disk(ip, dip, iip->ili_item.li_lsn);

	xfs_iflush_fork(ip, dip, iip, XFS_DATA_FORK);
	if (xfs_inode_has_attr_fork(ip))
		xfs_iflush_fork(ip, dip, iip, XFS_ATTR_FORK);

	/* generate the checksum. */
	xfs_dinode_calc_crc(mp, dip);

	return 0;
}

/*
 * Inode cache stubs.
 */

struct kmem_cache		*xfs_inode_cache;
extern struct kmem_cache	*xfs_ili_cache;

int
libxfs_iget(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	uint			flags,
	struct xfs_inode	**ipp)
{
	struct xfs_inode	*ip;
	struct xfs_perag	*pag;
	int			error = 0;

	/* reject inode numbers outside existing AGs */
	if (!xfs_verify_ino(mp, ino))
		return -EINVAL;

	ip = kmem_cache_zalloc(xfs_inode_cache, 0);
	if (!ip)
		return -ENOMEM;

	VFS_I(ip)->i_count = 1;
	ip->i_ino = ino;
	ip->i_mount = mp;
	ip->i_diflags2 = mp->m_ino_geo.new_diflags2;
	ip->i_af.if_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_next_unlinked = NULLAGINO;
	ip->i_prev_unlinked = NULLAGINO;
	spin_lock_init(&VFS_I(ip)->i_lock);

	pag = xfs_perag_get(mp, XFS_INO_TO_AGNO(mp, ip->i_ino));
	error = xfs_imap(pag, tp, ip->i_ino, &ip->i_imap, 0);
	xfs_perag_put(pag);

	if (error)
		goto out_destroy;

	/*
	 * For version 5 superblocks, if we are initialising a new inode and we
	 * are not utilising the XFS_MOUNT_IKEEP inode cluster mode, we can
	 * simply build the new inode core with a random generation number.
	 *
	 * For version 4 (and older) superblocks, log recovery is dependent on
	 * the di_flushiter field being initialised from the current on-disk
	 * value and hence we must also read the inode off disk even when
	 * initializing new inodes.
	 */
	if (xfs_has_v3inodes(mp) &&
	    (flags & XFS_IGET_CREATE) && !xfs_has_ikeep(mp)) {
		VFS_I(ip)->i_generation = get_random_u32();
	} else {
		struct xfs_buf		*bp;

		error = xfs_imap_to_bp(mp, tp, &ip->i_imap, &bp);
		if (error)
			goto out_destroy;

		error = xfs_inode_from_disk(ip,
				xfs_buf_offset(bp, ip->i_imap.im_boffset));
		if (!error)
			xfs_buf_set_ref(bp, XFS_INO_REF);
		xfs_trans_brelse(tp, bp);

		if (error)
			goto out_destroy;
	}

	*ipp = ip;
	return 0;

out_destroy:
	kmem_cache_free(xfs_inode_cache, ip);
	*ipp = NULL;
	return error;
}

/*
 * Get a metadata inode.
 *
 * The metafile type must match the file mode exactly, and for files in the
 * metadata directory tree, it must match the inode's metatype exactly.
 */
int
libxfs_trans_metafile_iget(
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	enum xfs_metafile_type	metafile_type,
	struct xfs_inode	**ipp)
{
	struct xfs_mount	*mp = tp->t_mountp;
	struct xfs_inode	*ip;
	umode_t			mode;
	int			error;

	error = libxfs_iget(mp, tp, ino, 0, &ip);
	if (error)
		return error;

	if (metafile_type == XFS_METAFILE_DIR)
		mode = S_IFDIR;
	else
		mode = S_IFREG;
	if (inode_wrong_type(VFS_I(ip), mode))
		goto bad_rele;
	if (xfs_has_metadir(mp)) {
		if (!xfs_is_metadir_inode(ip))
			goto bad_rele;
		if (metafile_type != ip->i_metatype)
			goto bad_rele;
	}

	*ipp = ip;
	return 0;
bad_rele:
	libxfs_irele(ip);
	return -EFSCORRUPTED;
}

/* Grab a metadata file if the caller doesn't already have a transaction. */
int
libxfs_metafile_iget(
	struct xfs_mount	*mp,
	xfs_ino_t		ino,
	enum xfs_metafile_type	metafile_type,
	struct xfs_inode	**ipp)
{
	struct xfs_trans	*tp;
	int			error;

	error = libxfs_trans_alloc_empty(mp, &tp);
	if (error)
		return error;

	error = libxfs_trans_metafile_iget(tp, ino, metafile_type, ipp);
	libxfs_trans_cancel(tp);
	return error;
}

static void
libxfs_idestroy(
	struct xfs_inode	*ip)
{
	switch (VFS_I(ip)->i_mode & S_IFMT) {
		case S_IFREG:
		case S_IFDIR:
		case S_IFLNK:
			libxfs_idestroy_fork(&ip->i_df);
			break;
	}

	libxfs_ifork_zap_attr(ip);

	if (ip->i_cowfp) {
		libxfs_idestroy_fork(ip->i_cowfp);
		kmem_cache_free(xfs_ifork_cache, ip->i_cowfp);
	}
}

void
libxfs_irele(
	struct xfs_inode	*ip)
{
	VFS_I(ip)->i_count--;

	if (VFS_I(ip)->i_count == 0) {
		ASSERT(ip->i_itemp == NULL);
		libxfs_idestroy(ip);
		kmem_cache_free(xfs_inode_cache, ip);
	}
}

void inode_init_owner(struct mnt_idmap *idmap, struct inode *inode,
		      const struct inode *dir, umode_t mode)
{
	inode_fsuid_set(inode, idmap);
	if (dir && dir->i_mode & S_ISGID) {
		inode->i_gid = dir->i_gid;

		/* Directories are special, and always inherit S_ISGID */
		if (S_ISDIR(mode))
			mode |= S_ISGID;
	} else
		inode_fsgid_set(inode, idmap);
	inode->i_mode = mode;
}
