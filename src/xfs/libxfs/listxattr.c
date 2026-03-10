// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "libxfs.h"
#include "libxlog.h"
#include "libfrog/bitmap.h"
#include "listxattr.h"

/* Call a function for every entry in a shortform xattr structure. */
STATIC int
xattr_walk_sf(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	xattr_walk_fn			attr_fn,
	void				*priv)
{
	struct xfs_attr_sf_hdr		*hdr = ip->i_af.if_data;
	struct xfs_attr_sf_entry	*sfe;
	unsigned int			i;
	int				error;

	sfe = libxfs_attr_sf_firstentry(hdr);
	for (i = 0; i < hdr->count; i++) {
		error = attr_fn(tp, ip, sfe->flags, sfe->nameval, sfe->namelen,
				&sfe->nameval[sfe->namelen], sfe->valuelen,
				priv);
		if (error)
			return error;

		sfe = xfs_attr_sf_nextentry(sfe);
	}

	return 0;
}

/* Call a function for every entry in this xattr leaf block. */
STATIC int
xattr_walk_leaf_entries(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	xattr_walk_fn			attr_fn,
	struct xfs_buf			*bp,
	void				*priv)
{
	struct xfs_attr3_icleaf_hdr	ichdr;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_attr_leafblock	*leaf = bp->b_addr;
	struct xfs_attr_leaf_entry	*entry;
	unsigned int			i;
	int				error;

	libxfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &ichdr, leaf);
	entry = xfs_attr3_leaf_entryp(leaf);

	for (i = 0; i < ichdr.count; entry++, i++) {
		void			*value;
		unsigned char		*name;
		unsigned int		namelen, valuelen;

		if (entry->flags & XFS_ATTR_LOCAL) {
			struct xfs_attr_leaf_name_local		*name_loc;

			name_loc = xfs_attr3_leaf_name_local(leaf, i);
			name = name_loc->nameval;
			namelen = name_loc->namelen;
			value = &name_loc->nameval[name_loc->namelen];
			valuelen = be16_to_cpu(name_loc->valuelen);
		} else {
			struct xfs_attr_leaf_name_remote	*name_rmt;

			name_rmt = xfs_attr3_leaf_name_remote(leaf, i);
			name = name_rmt->name;
			namelen = name_rmt->namelen;
			value = NULL;
			valuelen = be32_to_cpu(name_rmt->valuelen);
		}

		error = attr_fn(tp, ip, entry->flags, name, namelen, value,
				valuelen, priv);
		if (error)
			return error;

	}

	return 0;
}

/*
 * Call a function for every entry in a leaf-format xattr structure.  Avoid
 * memory allocations for the loop detector since there's only one block.
 */
STATIC int
xattr_walk_leaf(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	xattr_walk_fn			attr_fn,
	void				*priv)
{
	struct xfs_buf			*leaf_bp;
	int				error;

	error = -libxfs_attr3_leaf_read(tp, ip, ip->i_ino, 0, &leaf_bp);
	if (error)
		return error;

	error = xattr_walk_leaf_entries(tp, ip, attr_fn, leaf_bp, priv);
	libxfs_trans_brelse(tp, leaf_bp);
	return error;
}

/* Find the leftmost leaf in the xattr dabtree. */
STATIC int
xattr_walk_find_leftmost_leaf(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	struct bitmap			*seen_blocks,
	struct xfs_buf			**leaf_bpp)
{
	struct xfs_da3_icnode_hdr	nodehdr;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_da_intnode		*node;
	struct xfs_da_node_entry	*btree;
	struct xfs_buf			*bp;
	//xfs_failaddr_t			fa;
	xfs_dablk_t			blkno = 0;
	unsigned int			expected_level = 0;
	int				error;

	for (;;) {
		uint16_t		magic;

		error = -libxfs_da3_node_read(tp, ip, blkno, &bp,
				XFS_ATTR_FORK);
		if (error)
			return error;

		node = bp->b_addr;
		magic = be16_to_cpu(node->hdr.info.magic);
		if (magic == XFS_ATTR_LEAF_MAGIC ||
		    magic == XFS_ATTR3_LEAF_MAGIC)
			break;

		error = EFSCORRUPTED;
		if (magic != XFS_DA_NODE_MAGIC &&
		    magic != XFS_DA3_NODE_MAGIC)
			goto out_buf;

		libxfs_da3_node_hdr_from_disk(mp, &nodehdr, node);

		if (nodehdr.count == 0 || nodehdr.level >= XFS_DA_NODE_MAXDEPTH)
			goto out_buf;

		/* Check the level from the root node. */
		if (blkno == 0)
			expected_level = nodehdr.level - 1;
		else if (expected_level != nodehdr.level)
			goto out_buf;
		else
			expected_level--;

		/* Remember that we've seen this node. */
		error = -bitmap_set(seen_blocks, blkno, 1);
		if (error)
			goto out_buf;

		/* Find the next level towards the leaves of the dabtree. */
		btree = nodehdr.btree;
		blkno = be32_to_cpu(btree->before);
		libxfs_trans_brelse(tp, bp);

		/* Make sure we haven't seen this new block already. */
		if (bitmap_test(seen_blocks, blkno, 1))
			return EFSCORRUPTED;
	}

	error = EFSCORRUPTED;
	if (expected_level != 0)
		goto out_buf;

	/* Remember that we've seen this leaf. */
	error = -bitmap_set(seen_blocks, blkno, 1);
	if (error)
		goto out_buf;

	*leaf_bpp = bp;
	return 0;

out_buf:
	libxfs_trans_brelse(tp, bp);
	return error;
}

/* Call a function for every entry in a node-format xattr structure. */
STATIC int
xattr_walk_node(
	struct xfs_trans		*tp,
	struct xfs_inode		*ip,
	xattr_walk_fn			attr_fn,
	void				*priv)
{
	struct xfs_attr3_icleaf_hdr	leafhdr;
	struct bitmap			*seen_blocks;
	struct xfs_mount		*mp = ip->i_mount;
	struct xfs_attr_leafblock	*leaf;
	struct xfs_buf			*leaf_bp;
	int				error;

	bitmap_alloc(&seen_blocks);

	error = xattr_walk_find_leftmost_leaf(tp, ip, seen_blocks, &leaf_bp);
	if (error)
		goto out_bitmap;

	for (;;) {
		error = xattr_walk_leaf_entries(tp, ip, attr_fn, leaf_bp,
				priv);
		if (error)
			goto out_leaf;

		/* Find the right sibling of this leaf block. */
		leaf = leaf_bp->b_addr;
		libxfs_attr3_leaf_hdr_from_disk(mp->m_attr_geo, &leafhdr, leaf);
		if (leafhdr.forw == 0)
			goto out_leaf;

		libxfs_trans_brelse(tp, leaf_bp);

		/* Make sure we haven't seen this new leaf already. */
		if (bitmap_test(seen_blocks, leafhdr.forw, 1))
			goto out_bitmap;

		error = -libxfs_attr3_leaf_read(tp, ip, ip->i_ino,
				leafhdr.forw, &leaf_bp);
		if (error)
			goto out_bitmap;

		/* Remember that we've seen this new leaf. */
		error = -bitmap_set(seen_blocks, leafhdr.forw, 1);
		if (error)
			goto out_leaf;
	}

out_leaf:
	libxfs_trans_brelse(tp, leaf_bp);
out_bitmap:
	bitmap_free(&seen_blocks);
	return error;
}

/* Call a function for every extended attribute in a file. */
int
xattr_walk(
	struct xfs_trans	*tp,
	struct xfs_inode	*ip,
	xattr_walk_fn		attr_fn,
	void			*priv)
{
	int			error;

	if (!libxfs_inode_hasattr(ip))
		return 0;

	if (ip->i_af.if_format == XFS_DINODE_FMT_LOCAL)
		return xattr_walk_sf(tp, ip, attr_fn, priv);

	/* attr functions require that the attr fork is loaded */
	error = -libxfs_iread_extents(tp, ip, XFS_ATTR_FORK);
	if (error)
		return error;

	if (libxfs_attr_is_leaf(ip))
		return xattr_walk_leaf(tp, ip, attr_fn, priv);

	return xattr_walk_node(tp, ip, attr_fn, priv);
}
