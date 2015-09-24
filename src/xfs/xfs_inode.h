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

#ifndef __XFS_INODE_H__
#define __XFS_INODE_H__

/* These match kernel side includes */
#include "xfs_inode_buf.h"
#include "xfs_inode_fork.h"

struct xfs_trans;
struct xfs_mount;
struct xfs_inode_log_item;
struct xfs_dir_ops;

/*
 * Inode interface
 */
typedef struct xfs_inode {
	struct cache_node	i_node;
	struct xfs_mount	*i_mount;	/* fs mount struct ptr */
	xfs_ino_t		i_ino;		/* inode number (agno/agino) */
	struct xfs_imap		i_imap;		/* location for xfs_imap() */
	struct xfs_buftarg	i_dev;		/* dev for this inode */
	struct xfs_ifork	*i_afp;		/* attribute fork pointer */
	struct xfs_ifork	i_df;		/* data fork */
	struct xfs_trans	*i_transp;	/* ptr to owning transaction */
	struct xfs_inode_log_item *i_itemp;	/* logging information */
	unsigned int		i_delayed_blks;	/* count of delay alloc blks */
	struct xfs_icdinode	i_d;		/* most of ondisk inode */
	xfs_fsize_t		i_size;		/* in-memory size */
	const struct xfs_dir_ops *d_ops;	/* directory ops vector */
} xfs_inode_t;

/*
 * For regular files we only update the on-disk filesize when actually
 * writing data back to disk.  Until then only the copy in the VFS inode
 * is uptodate.
 */
static inline xfs_fsize_t XFS_ISIZE(struct xfs_inode *ip)
{
	if (S_ISREG(ip->i_d.di_mode))
		return ip->i_size;
	return ip->i_d.di_size;
}
#define XFS_IS_REALTIME_INODE(ip) ((ip)->i_d.di_flags & XFS_DIFLAG_REALTIME)

/*
 * Project quota id helpers (previously projid was 16bit only and using two
 * 16bit values to hold new 32bit projid was chosen to retain compatibility with
 * "old" filesystems).
 *
 * Copied here from xfs_inode.h because it has to be defined after the struct
 * xfs_inode...
 */
static inline prid_t
xfs_get_projid(struct xfs_icdinode *id)
{
	return (prid_t)id->di_projid_hi << 16 | id->di_projid_lo;
}

static inline void
xfs_set_projid(struct xfs_icdinode *id, prid_t projid)
{
	id->di_projid_hi = (__uint16_t) (projid >> 16);
	id->di_projid_lo = (__uint16_t) (projid & 0xffff);
}

typedef struct cred {
	uid_t	cr_uid;
	gid_t	cr_gid;
} cred_t;

extern int	libxfs_inode_alloc (struct xfs_trans **, struct xfs_inode *,
				mode_t, nlink_t, xfs_dev_t, struct cred *,
				struct fsxattr *, struct xfs_inode **);
extern void	libxfs_trans_inode_alloc_buf (struct xfs_trans *,
				struct xfs_buf *);

extern void	libxfs_trans_ichgtime(struct xfs_trans *,
				struct xfs_inode *, int);
extern int	libxfs_iflush_int (struct xfs_inode *, struct xfs_buf *);

/* Inode Cache Interfaces */
extern int	libxfs_iget(struct xfs_mount *, struct xfs_trans *, xfs_ino_t,
				uint, struct xfs_inode **, xfs_daddr_t);
extern void	libxfs_iput(struct xfs_inode *);

#define IRELE(ip) libxfs_iput(ip)

#endif /* __XFS_INODE_H__ */
