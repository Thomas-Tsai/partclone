// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "libxfs_priv.h"
#include "libxfs.h"
#include "libxfs/xfile.h"
#include "libxfs/buf_mem.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Buffer Cache for In-Memory Files
 * ================================
 *
 * Offline fsck wants to create ephemeral ordered recordsets.  The existing
 * btree infrastructure can do this, but we need the buffer cache to target
 * memory instead of block devices.
 *
 * xfiles meet those requirements.  Therefore, the xmbuf mechanism uses a
 * partition on an xfile to store the staging data.
 *
 * xmbufs assume that the caller will handle all required concurrency
 * management.  The resulting xfs_buf objects are kept private to the xmbuf
 * (they are not recycled to the LRU) because b_addr is mapped directly to the
 * memfd file.
 *
 * The only supported block size is the system page size.
 */

/* Figure out the xfile buffer cache block size here */
unsigned int	XMBUF_BLOCKSIZE;
unsigned int	XMBUF_BLOCKSHIFT;

void
xmbuf_libinit(void)
{
	long		ret = sysconf(_SC_PAGESIZE);

	/* If we don't find a power-of-two page size, go with 4k. */
	if (ret < 0 || !is_power_of_2(ret))
		ret = 4096;

	XMBUF_BLOCKSIZE = ret;
	XMBUF_BLOCKSHIFT = libxfs_highbit32(XMBUF_BLOCKSIZE);
}

/* Allocate a new cache node (aka a xfs_buf) */
static struct cache_node *
xmbuf_cache_alloc(
	cache_key_t		key)
{
	struct xfs_bufkey	*bufkey = (struct xfs_bufkey *)key;
	struct xfs_buf		*bp;
	int			error;

	bp = kmem_cache_zalloc(xfs_buf_cache, 0);
	if (!bp)
		return NULL;

	bp->b_cache_key = bufkey->blkno;
	bp->b_length = bufkey->bblen;
	bp->b_target = bufkey->buftarg;
	bp->b_mount = bufkey->buftarg->bt_mount;

	pthread_mutex_init(&bp->b_lock, NULL);
	INIT_LIST_HEAD(&bp->b_li_list);
	bp->b_maps = &bp->__b_map;

	bp->b_nmaps = 1;
	bp->b_maps[0].bm_bn = bufkey->blkno;
	bp->b_maps[0].bm_len = bp->b_length;

	error = xmbuf_map_page(bp);
	if (error) {
		fprintf(stderr,
 _("%s: %s can't mmap %u bytes at xfile offset %llu: %s\n"),
				progname, __FUNCTION__, BBTOB(bp->b_length),
				(unsigned long long)BBTOB(bufkey->blkno),
				strerror(error));

		kmem_cache_free(xfs_buf_cache, bp);
		return NULL;
	}

	return &bp->b_node;
}

/* Flush a buffer to disk before purging the cache node */
static int
xmbuf_cache_flush(
	struct cache_node	*node)
{
	/* direct mapped buffers do not need writing */
	return 0;
}

/* Release resources, free the buffer. */
static void
xmbuf_cache_relse(
	struct cache_node	*node)
{
	struct xfs_buf		*bp;

	bp = container_of(node, struct xfs_buf, b_node);
	xmbuf_unmap_page(bp);
	kmem_cache_free(xfs_buf_cache, bp);
}

/* Release a bunch of buffers */
static unsigned int
xmbuf_cache_bulkrelse(
	struct cache		*cache,
	struct list_head	*list)
{
	struct cache_node	*cn, *n;
	int			count = 0;

	if (list_empty(list))
		return 0;

	list_for_each_entry_safe(cn, n, list, cn_mru) {
		xmbuf_cache_relse(cn);
		count++;
	}

	return count;
}

static struct cache_operations xmbuf_bcache_operations = {
	.hash		= libxfs_bhash,
	.alloc		= xmbuf_cache_alloc,
	.flush		= xmbuf_cache_flush,
	.relse		= xmbuf_cache_relse,
	.compare	= libxfs_bcompare,
	.bulkrelse	= xmbuf_cache_bulkrelse
};

/*
 * Allocate a buffer cache target for a memory-backed file and set up the
 * buffer target.
 */
int
xmbuf_alloc(
	struct xfs_mount	*mp,
	const char		*descr,
	unsigned long long	maxpos,
	struct xfs_buftarg	**btpp)
{
	struct xfs_buftarg	*btp;
	struct xfile		*xfile;
	struct cache		*cache;
	int			error;

	btp = kzalloc(sizeof(*btp), GFP_KERNEL);
	if (!btp)
		return -ENOMEM;

	error = xfile_create(descr, maxpos, &xfile);
	if (error)
		goto out_btp;

	cache = cache_init(0, LIBXFS_BHASHSIZE(NULL), &xmbuf_bcache_operations);
	if (!cache) {
		error = -ENOMEM;
		goto out_xfile;
	}

	/* Initialize buffer target */
	btp->bt_mount = mp;
	btp->bt_bdev = (dev_t)-1;
	btp->bt_bdev_fd = -1;
	btp->bt_xfile = xfile;
	btp->bcache = cache;

	error = pthread_mutex_init(&btp->lock, NULL);
	if (error)
		goto out_cache;

	*btpp = btp;
	return 0;

out_cache:
	cache_destroy(cache);
out_xfile:
	xfile_destroy(xfile);
out_btp:
	kfree(btp);
	return error;
}

/* Free a buffer cache target for a memory-backed file. */
void
xmbuf_free(
	struct xfs_buftarg	*btp)
{
	ASSERT(xfs_buftarg_is_mem(btp));

	cache_destroy(btp->bcache);
	pthread_mutex_destroy(&btp->lock);
	xfile_destroy(btp->bt_xfile);
	kfree(btp);
}

/* Directly map a memfd page into the buffer cache. */
int
xmbuf_map_page(
	struct xfs_buf		*bp)
{
	struct xfile		*xfile = bp->b_target->bt_xfile;
	void			*p;
	loff_t			pos;

	pos = xfile->partition_pos + BBTOB(xfs_buf_daddr(bp));
	p = mmap(NULL, BBTOB(bp->b_length), PROT_READ | PROT_WRITE, MAP_SHARED,
			xfile->fcb->fd, pos);
	if (p == MAP_FAILED)
		return -errno;

	bp->b_addr = p;
	bp->b_flags |= LIBXFS_B_UPTODATE | LIBXFS_B_UNCHECKED;
	bp->b_error = 0;
	return 0;
}

/* Unmap a memfd page that was mapped into the buffer cache. */
void
xmbuf_unmap_page(
	struct xfs_buf		*bp)
{
	munmap(bp->b_addr, BBTOB(bp->b_length));
	bp->b_addr = NULL;
}

/* Is this a valid daddr within the buftarg? */
bool
xmbuf_verify_daddr(
	struct xfs_buftarg	*btp,
	xfs_daddr_t		daddr)
{
	struct xfile		*xf = btp->bt_xfile;

	ASSERT(xfs_buftarg_is_mem(btp));

	return daddr < (xf->maxbytes >> BBSHIFT);
}

/* Discard the page backing this buffer. */
static void
xmbuf_stale(
	struct xfs_buf		*bp)
{
	struct xfile		*xf = bp->b_target->bt_xfile;
	loff_t			pos;

	ASSERT(xfs_buftarg_is_mem(bp->b_target));

	pos = BBTOB(xfs_buf_daddr(bp)) + xf->partition_pos;
	fallocate(xf->fcb->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, pos,
			BBTOB(bp->b_length));
}

/*
 * Finalize a buffer -- discard the backing page if it's stale, or run the
 * write verifier to detect problems.
 */
int
xmbuf_finalize(
	struct xfs_buf		*bp)
{
	xfs_failaddr_t		fa;
	int			error = 0;

	if (bp->b_flags & LIBXFS_B_STALE) {
		xmbuf_stale(bp);
		return 0;
	}

	/*
	 * Although this btree is ephemeral, validate the buffer structure so
	 * that we can detect memory corruption errors and software bugs.
	 */
	fa = bp->b_ops->verify_struct(bp);
	if (fa) {
		error = -EFSCORRUPTED;
		xfs_verifier_error(bp, error, fa);
	}

	return error;
}

/*
 * Detach this xmbuf buffer from the transaction by any means necessary.
 * All buffers are direct-mapped, so they do not need bwrite.
 */
void
xmbuf_trans_bdetach(
	struct xfs_trans	*tp,
	struct xfs_buf		*bp)
{
	struct xfs_buf_log_item	*bli = bp->b_log_item;

	ASSERT(bli != NULL);

	bli->bli_flags &= ~(XFS_BLI_DIRTY | XFS_BLI_ORDERED |
			    XFS_BLI_STALE);
	clear_bit(XFS_LI_DIRTY, &bli->bli_item.li_flags);

	while (bp->b_log_item != NULL)
		xfs_trans_bdetach(tp, bp);
}
