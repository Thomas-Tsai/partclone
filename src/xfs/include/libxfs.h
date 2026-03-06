// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#ifndef __LIBXFS_H__
#define __LIBXFS_H__

/* CONFIG_XFS_* must be defined to 1 to work with IS_ENABLED() */

/* For userspace XFS_RT is always defined */
#define CONFIG_XFS_RT 1
/* Ditto in-memory btrees */
#define CONFIG_XFS_BTREE_IN_MEM 1

#include "libxfs_api_defs.h"
#include "platform_defs.h"
#include "xfs.h"

#include "list.h"
#include "hlist.h"
#include "cache.h"
#include "bitops.h"
#include "kmem.h"
#include "libfrog/radix-tree.h"
#include "libfrog/bitmask.h"
#include "libfrog/div64.h"
#include "atomic.h"
#include "spinlock.h"

#include "xfs_types.h"
#include "xfs_fs.h"
#include "xfs_arch.h"

#include "xfs_shared.h"
#include "xfs_format.h"
#include "xfs_log_format.h"
#include "xfs_quota_defs.h"
#include "xfs_trans_resv.h"


/* CRC stuff, buffer API dependent on it */
extern uint32_t crc32c_le(uint32_t crc, unsigned char const *p, size_t len);
#define crc32c(c,p,l)	crc32c_le((c),(unsigned char const *)(p),(l))

/* fake up kernel's iomap, (not) used in xfs_bmap.[ch] */
struct iomap;
#include "xfs_cksum.h"

#define __round_mask(x, y) ((__typeof__(x))((y)-1))
#define round_up(x, y) ((((x)-1) | __round_mask(x, y))+1)
#define unlikely(x) (x)
#define likely(x) (x)

/*
 * This mirrors the kernel include for xfs_buf.h - it's implicitly included in
 * every files via a similar include in the kernel xfs_linux.h.
 */
#include "libxfs_io.h"

#include "xfs_bit.h"
#include "xfs_sb.h"
#include "xfs_mount.h"
#include "xfs_defer.h"
#include "xfs_errortag.h"
#include "xfs_da_format.h"
#include "xfs_da_btree.h"
#include "xfs_inode.h"
#include "xfs_dir2.h"
#include "xfs_dir2_priv.h"
#include "xfs_bmap_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_attr.h"
#include "xfs_attr_sf.h"
#include "xfs_inode_fork.h"
#include "xfs_inode_buf.h"
#include "xfs_inode_util.h"
#include "xfs_alloc.h"
#include "xfs_btree.h"
#include "xfs_bmap.h"
#include "xfs_trace.h"
#include "xfs_trans.h"
#include "xfs_ag.h"
#include "xfs_rmap_btree.h"
#include "xfs_rmap.h"
#include "xfs_refcount_btree.h"
#include "xfs_refcount.h"
#include "xfs_btree_staging.h"
#include "xfs_rtbitmap.h"
#include "xfs_symlink_remote.h"
#include "libxfs/xfile.h"
#include "libxfs/buf_mem.h"
#include "xfs_btree_mem.h"
#include "xfs_parent.h"
#include "xfs_ag_resv.h"
#include "xfs_metafile.h"
#include "xfs_metadir.h"
#include "xfs_rtgroup.h"
#include "xfs_rtbitmap.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef XFS_SUPER_MAGIC
#define XFS_SUPER_MAGIC 0x58465342
#endif

#define xfs_isset(a,i)	((a)[(i)/(sizeof(*(a))*NBBY)] & (1ULL<<((i)%(sizeof(*(a))*NBBY))))

struct libxfs_dev {
	/* input parameters */
	char		*name;	/* pathname of the device */
	bool		isfile;	/* is the device a file? */
	bool		create;	/* create file if it doesn't exist */

	/* output parameters */
	dev_t		dev;	/* device name for the device */
	long long       size;	/* size of subvolume (BBs) */
	int		bsize;	/* device blksize */
	int		fd;	/* file descriptor */
};

/*
 * Argument structure for libxfs_init().
 */
struct libxfs_init {
	struct libxfs_dev	data;
	struct libxfs_dev	log;
	struct libxfs_dev	rt;

	/* input parameters */
	unsigned	flags;		/* LIBXFS_* flags below */
	int		bcache_flags;	/* cache init flags */
	int		setblksize;	/* value to set device blksizes to */
};

/* disallow all mounted filesystems: */
#define LIBXFS_ISREADONLY	(1U << 0)

/* allow mounted only if mounted ro: */
#define LIBXFS_ISINACTIVE	(1U << 1)

/* repairing a device mounted ro: */
#define LIBXFS_DANGEROUSLY	(1U << 2)

/* disallow other accesses (O_EXCL): */
#define LIBXFS_EXCLUSIVELY	(1U << 3)

/* can use direct I/O, not buffered: */
#define LIBXFS_DIRECT		(1U << 4)

/* lock xfs_buf's - for MT usage */
#define LIBXFS_USEBUFLOCK	(1U << 5)

extern char	*progname;
extern xfs_lsn_t libxfs_max_lsn;

int		libxfs_init(struct libxfs_init *);
void		libxfs_destroy(struct libxfs_init *li);

extern int	libxfs_device_alignment (void);

/* check or write log footer: specify device, log size in blocks & uuid */
typedef char	*(libxfs_get_block_t)(char *, int, void *);

/*
 * Helpers to clear the log to a particular log cycle.
 */
#define XLOG_INIT_CYCLE	1
extern int	libxfs_log_clear(struct xfs_buftarg *, char *, xfs_daddr_t,
				 uint, uuid_t *, int, int, int, int, bool);
extern int	libxfs_log_header(char *, uuid_t *, int, int, int, xfs_lsn_t,
				  xfs_lsn_t, libxfs_get_block_t *, void *);


/* Shared utility routines */

int	libxfs_alloc_file_space(struct xfs_inode *ip, xfs_off_t offset,
		xfs_off_t len, uint32_t bmapi_flags);
int	libxfs_file_write(struct xfs_inode *ip, void *buf, off_t pos,
		size_t len);

/* XXX: this is messy and needs fixing */
#ifndef __LIBXFS_INTERNAL_XFS_H__
extern void cmn_err(int, char *, ...);
enum ce { CE_DEBUG, CE_CONT, CE_NOTE, CE_WARN, CE_ALERT, CE_PANIC };
#endif

#include "xfs_ialloc.h"

#include "xfs_attr_leaf.h"
#include "xfs_attr_remote.h"
#include "xfs_trans_space.h"

#define XFS_INOBT_IS_FREE_DISK(rp,i)		\
			((be64_to_cpu((rp)->ir_free) & XFS_INOBT_MASK(i)) != 0)

static inline bool
xfs_inobt_is_sparse_disk(
	struct xfs_inobt_rec	*rp,
	int			offset)
{
	int			spshift;
	uint16_t		holemask;

	holemask = be16_to_cpu(rp->ir_u.sp.ir_holemask);
	spshift = offset / XFS_INODES_PER_HOLEMASK_BIT;
	if ((1 << spshift) & holemask)
		return true;

	return false;
}

static inline void
libxfs_bmbt_disk_get_all(
	struct xfs_bmbt_rec	*rec,
	struct xfs_bmbt_irec	*irec)
{
	uint64_t		l0 = get_unaligned_be64(&rec->l0);
	uint64_t		l1 = get_unaligned_be64(&rec->l1);

	irec->br_startoff = (l0 & xfs_mask64lo(64 - BMBT_EXNTFLAG_BITLEN)) >> 9;
	irec->br_startblock = ((l0 & xfs_mask64lo(9)) << 43) | (l1 >> 21);
	irec->br_blockcount = l1 & xfs_mask64lo(21);
	if (l0 >> (64 - BMBT_EXNTFLAG_BITLEN))
		irec->br_state = XFS_EXT_UNWRITTEN;
	else
		irec->br_state = XFS_EXT_NORM;
}

#include "xfs_attr.h"
#include "topology.h"

/*
 * Superblock helpers for programs that act on independent superblock
 * structures.  These used to be part of xfs_format.h.
 */
static inline bool xfs_sb_version_haslazysbcount(struct xfs_sb *sbp)
{
	return (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5) ||
	       (xfs_sb_version_hasmorebits(sbp) &&
		(sbp->sb_features2 & XFS_SB_VERSION2_LAZYSBCOUNTBIT));
}

static inline bool xfs_sb_version_hascrc(struct xfs_sb *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5;
}

static inline bool xfs_sb_version_hasmetauuid(struct xfs_sb *sbp)
{
	return (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5) &&
		(sbp->sb_features_incompat & XFS_SB_FEAT_INCOMPAT_META_UUID);
}

static inline bool xfs_sb_version_hasalign(struct xfs_sb *sbp)
{
	return (XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5 ||
		(sbp->sb_versionnum & XFS_SB_VERSION_ALIGNBIT));
}

static inline bool xfs_sb_version_hasdalign(struct xfs_sb *sbp)
{
	return (sbp->sb_versionnum & XFS_SB_VERSION_DALIGNBIT);
}

static inline bool xfs_sb_version_haslogv2(struct xfs_sb *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5 ||
	       (sbp->sb_versionnum & XFS_SB_VERSION_LOGV2BIT);
}

static inline bool xfs_sb_version_hassector(struct xfs_sb *sbp)
{
	return (sbp->sb_versionnum & XFS_SB_VERSION_SECTORBIT);
}

static inline bool xfs_sb_version_needsrepair(struct xfs_sb *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5 &&
		(sbp->sb_features_incompat & XFS_SB_FEAT_INCOMPAT_NEEDSREPAIR);
}

static inline bool xfs_sb_version_hassparseinodes(struct xfs_sb *sbp)
{
	return XFS_SB_VERSION_NUM(sbp) == XFS_SB_VERSION_5 &&
		xfs_sb_has_incompat_feature(sbp, XFS_SB_FEAT_INCOMPAT_SPINODES);
}

#endif	/* __LIBXFS_H__ */
