/**
 * zfsclone.c - part of Partclone project
 *
 * ZFS used-block bitmap provider, modelled after extfsclone.c / btrfsclone.c.
 *
 * v1 scope: **stripe pool on a single partition** only.
 *   - one top-level vdev (vdev_children == 1)
 *   - user supplies the ZFS partition (e.g. /dev/sda1); whole-disk pools
 *     (zpool create ... /dev/sda) are rejected because vdev labels live on
 *     the first GPT partition, not on the whole device itself.
 *
 * Algorithm (no decompression; only metadata is read):
 *   1. Open the pool with libzpool (kernel emulation):
 *      kernel_init(SPA_MODE_READ) + spa_open_rewind(pool_name).
 *      If the pool isn't in /etc/zfs/zpool.cache we fall back to
 *      zpool_find_config() + spa_import() which scans /dev/ (same as
 *      `zdb -e`). This lets a completely exported pool still be read.
 *   2. traverse_pool() walks every live blkptr (MOS + all datasets +
 *      snapshots + all indirect levels). For each bp, DVA_GET_OFFSET()/
 *      DVA_GET_ASIZE() give *bytes* within the single vdev; for a stripe
 *      vdev id is always 0.
 *   3. Always mark the vdev "label area" (first 4 MiB, last 512 KiB)
 *      used: it holds the four vdev labels and the uberblock ring, which
 *      ZFS needs to open the pool at all.
 *   4. As a safety net replay each metaslab's space-map (ALLOC/FREE) and
 *      any log space-maps (SPA_FEATURE_LOG_SPACEMAP). This matches the
 *      "spacemap allocated" count zdb -b reports.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* OpenZFS headers (shipped by libzfslinux-dev in /usr/include/libzfs). */
#include <sys/zfs_context.h>	/* FTAG, kernel userland shim */
#include <sys/spa.h>
#include <sys/spa_impl.h>	/* spa_root_vdev, spa_meta_objset, spa_sm_logs_by_txg */
#include <sys/vdev.h>
#include <sys/vdev_impl.h>	/* vdev_ms, vdev_asize, vdev_psize, VDEV_LABEL_* */
#include <sys/metaslab.h>
#include <sys/metaslab_impl.h>	/* ms_sm, ms_start, ms_id */
#include <sys/space_map.h>	/* space_map_*, SM_*_DECODE */
#include <sys/dmu.h>		/* dmu_read */
#include <sys/dmu_traverse.h>	/* traverse_pool, blkptr_cb_t, TRAVERSE_* */
#include <sys/zio.h>		/* zbookmark_phys_t */
#include <sys/avl.h>
#include <sys/spa_log_spacemap.h>
#include <libzfs.h>		/* zpool_find_config, libzpool_config_ops, ZFS_IMPORT_SKIP_MMP */
#include <libzutil.h>		/* zpool_read_label */
#include <libnvpair.h>
#include <sys/nvpair.h>
#include <zfeature_common.h>

#include <config.h>
#include "partclone.h"
#include "zfsclone.h"
#include "progress.h"
#include "fs_common.h"

/* libzpool.h is not shipped in libzfslinux-dev; declare the two symbols
 * we need: they live in libzpool.so. */
extern void kernel_init(int mode);
extern void kernel_fini(void);

#define ZFS_SECTOR_SHIFT	9
#define ZFS_SECTOR_SIZE		(1ULL << ZFS_SECTOR_SHIFT)	/* 512 */

typedef struct {
	unsigned long  *bitmap;		/* partclone bitmap (1 bit/block) */
	uint64_t	total_blocks;	/* total blocks per caller fs_info  */
} walk_ctx_t;

static spa_t	*g_spa = NULL;
static int	 g_kernel_inited = 0;
static char	 g_pool_name[MAXNAMELEN];

/* ------------------------------------------------------------------ */
/* Bitmap helpers (translate vdev byte offsets to partclone block idx) */
/* ------------------------------------------------------------------ */

static inline uint64_t
clamp_last(uint64_t first, uint64_t last, uint64_t total)
{
	(void)first;
	if (last >= total)
		last = total - 1;
	return (last);
}

/*
 * DVA_GET_OFFSET() returns an offset in "data area" space: a leaf vdev's
 * data area starts at VDEV_LABEL_START_SIZE (=4 MiB) within the partition,
 * NOT at partition byte 0. zio_vdev_child_io() adds VDEV_LABEL_START_SIZE
 * to every non-labeling leaf I/O. So every DVA-derived offset or spacemap
 * entry must be translated by adding +4 MiB before indexing the bitmap.
 * Label-area writes (vdev labels, uberblocks) are NOT offset-adjusted, so
 * we have two helpers.
 */

static void
mark_range_used(walk_ctx_t *ctx, uint64_t off, uint64_t len)
{
	if (len == 0)
		return;
	uint64_t first = off >> ZFS_SECTOR_SHIFT;
	uint64_t last  = (off + len - 1) >> ZFS_SECTOR_SHIFT;
	if (first >= ctx->total_blocks)
		return;
	last = clamp_last(first, last, ctx->total_blocks);
	for (uint64_t s = first; s <= last; s++)
		pc_set_bit(s, ctx->bitmap, ctx->total_blocks);
}

static void
clear_range_used(walk_ctx_t *ctx, uint64_t off, uint64_t len)
{
	if (len == 0)
		return;
	uint64_t first = off >> ZFS_SECTOR_SHIFT;
	uint64_t last  = (off + len - 1) >> ZFS_SECTOR_SHIFT;
	if (first >= ctx->total_blocks)
		return;
	last = clamp_last(first, last, ctx->total_blocks);
	for (uint64_t s = first; s <= last; s++)
		pc_clear_bit(s, ctx->bitmap, ctx->total_blocks);
}

/* DVA-based marking: DVA offset is data-area-relative. */
static inline void
mark_range_used_dva(walk_ctx_t *ctx, uint64_t dva_off, uint64_t len)
{
	mark_range_used(ctx, dva_off + VDEV_LABEL_START_SIZE, len);
}

static inline void
clear_range_used_dva(walk_ctx_t *ctx, uint64_t dva_off, uint64_t len)
{
	clear_range_used(ctx, dva_off + VDEV_LABEL_START_SIZE, len);
}

/* ------------------------------------------------------------------ */
/* traverse_pool callback                                               */
/* ------------------------------------------------------------------ */

static int
walk_cb(spa_t *spa, zilog_t *zilog, const blkptr_t *bp,
    const zbookmark_phys_t *zb, const struct dnode_phys *dnp, void *arg)
{
	walk_ctx_t *ctx = arg;
	(void)spa, (void)zilog, (void)zb, (void)dnp;

	if (bp == NULL)
		return (0);
	if (BP_IS_HOLE(bp))
		return (0);
	if (BP_IS_REDACTED(bp))
		return (0);
	if (BP_IS_EMBEDDED(bp))	/* inline data, no DVA */
		return (0);

	int ndvas = BP_GET_NDVAS(bp);
	/* tag the uberblock rootbp (zb = 0/-1/0) for debugging */
	int is_rootbp = (zb != NULL && zb->zb_object == 0 &&
	    zb->zb_level == -1 && zb->zb_blkid == 0);
	for (int d = 0; d < ndvas; d++) {
		const dva_t *dva = &bp->blk_dva[d];
		uint64_t off = DVA_GET_OFFSET(dva);	/* bytes */
		uint64_t asize = DVA_GET_ASIZE(dva);	/* bytes */
		if (is_rootbp)
			log_mesg(0, 0, 0, fs_opt.debug,
			    "zfsclone: ROOTBP dva[%d] v=%llu off=0x%llx "
			    "asize=0x%llx\n",
			    d, (unsigned long long)DVA_GET_VDEV(dva),
			    (unsigned long long)off,
			    (unsigned long long)asize);
		/* v1: stripe only, so DVA vdev id is always 0. */
		mark_range_used_dva(ctx, off, asize);
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* Space-map replay (safety net; matches zdb -b bp allocated)           */
/* ------------------------------------------------------------------ */

static void
replay_metaslab_spacemap(spa_t *spa, metaslab_t *msp, walk_ctx_t *ctx)
{
	if (msp->ms_sm == NULL || msp->ms_sm->sm_object == 0)
		return;

	uint64_t sm_obj	= msp->ms_sm->sm_object;
	uint64_t start	= msp->ms_start;
	uint8_t  shift	= msp->ms_sm->sm_shift;
	uint64_t length	= space_map_length(msp->ms_sm);
	objset_t *os	= spa_meta_objset(spa);

	for (uint64_t off = 0; off + sizeof (uint64_t) <= length;
	    off += sizeof (uint64_t)) {
		uint64_t word = 0;
		if (dmu_read(os, sm_obj, off, sizeof (word), &word,
		    DMU_READ_PREFETCH) != 0)
			break;
		if (sm_entry_is_debug(word))
			continue;

		uint64_t entry_off, entry_run;
		int is_alloc;

		if (sm_entry_is_single_word(word)) {
			is_alloc = (SM_TYPE_DECODE(word) == SM_ALLOC);
			entry_off = (SM_OFFSET_DECODE(word) << shift) + start;
			entry_run = SM_RUN_DECODE(word) << shift;
		} else {
			uint64_t extra = 0;
			off += sizeof (extra);
			if (off + sizeof (extra) > length)
				break;
			if (dmu_read(os, sm_obj, off, sizeof (extra), &extra,
			    DMU_READ_PREFETCH) != 0)
				break;
			entry_run = SM2_RUN_DECODE(word) << shift;
			is_alloc  = (SM2_TYPE_DECODE(extra) == SM_ALLOC);
			entry_off = SM2_OFFSET_DECODE(extra) << shift;
			/* SM2_VDEV_DECODE(word) would give vdev id; v1 ignores
			 * since stripe is guaranteed vdev==0. */
		}
		if (is_alloc)
			mark_range_used_dva(ctx, entry_off, entry_run);
		else
			clear_range_used_dva(ctx, entry_off, entry_run);
	}
}

static void
replay_log_spacemaps(spa_t *spa, walk_ctx_t *ctx)
{
	if (!spa_feature_is_active(spa, SPA_FEATURE_LOG_SPACEMAP))
		return;

	objset_t *os = spa_meta_objset(spa);

	for (spa_log_sm_t *sls = avl_first(&spa->spa_sm_logs_by_txg);
	    sls != NULL; sls = AVL_NEXT(&spa->spa_sm_logs_by_txg, sls)) {
		space_map_t *sm = NULL;
		if (space_map_open(&sm, os, sls->sls_sm_obj, 0, UINT64_MAX,
		    SPA_MINBLOCKSHIFT) != 0 || sm == NULL)
			continue;
		uint64_t length = space_map_length(sm);
		uint8_t shift   = sm->sm_shift;

		for (uint64_t off = 0; off + sizeof (uint64_t) <= length;
		    off += sizeof (uint64_t)) {
			uint64_t word = 0;
			if (dmu_read(os, sls->sls_sm_obj, off, sizeof (word),
			    &word, DMU_READ_PREFETCH) != 0)
				break;
			if (sm_entry_is_debug(word))
				continue;
			if (sm_entry_is_single_word(word))
				continue;	/* log entries are SM2 */

			uint64_t extra = 0;
			off += sizeof (extra);
			if (off + sizeof (extra) > length)
				break;
			if (dmu_read(os, sls->sls_sm_obj, off, sizeof (extra),
			    &extra, DMU_READ_PREFETCH) != 0)
				break;

			uint64_t run  = SM2_RUN_DECODE(word) << shift;
			int is_alloc  = (SM2_TYPE_DECODE(extra) == SM_ALLOC);
			uint64_t eoff = SM2_OFFSET_DECODE(extra) << shift;
			if (is_alloc)
				mark_range_used_dva(ctx, eoff, run);
			else
				clear_range_used_dva(ctx, eoff, run);
		}
		space_map_close(sm);
	}
}

/* ------------------------------------------------------------------ */
/* fs_open / fs_close                                                   */
/* ------------------------------------------------------------------ */

/*
 * Try to import the pool by scanning a given path (device or directory).
 * Returns 0 on success, or sets g_spa via spa_open_rewind afterwards.
 * Returns the last error if the pool cannot be found there.
 */
static int
try_import_via_device(const char *scan_path)
{
	char *searchdirs[1] = { (char *)scan_path };
	importargs_t iargs = { 0 };
	iargs.paths        = 1;
	iargs.path         = searchdirs;
	iargs.can_be_active = B_TRUE;

	libpc_handle_t lpch = {
		.lpc_lib_handle = NULL,
		.lpc_ops        = &libzpool_config_ops,
		.lpc_printerr   = B_FALSE,
	};
	nvlist_t *cfg = NULL;
	if (zpool_find_config(&lpch, g_pool_name, &cfg, &iargs) != 0)
		return (ENOENT);

	int ierr = spa_import((char *)g_pool_name, cfg, NULL,
	    ZFS_IMPORT_SKIP_MMP);
	nvlist_free(cfg);
	if (ierr != 0)
		return (ierr);
	return (spa_open_rewind(g_pool_name, &g_spa, FTAG, NULL, NULL));
}

static int
read_pool_name_from_label(const char *device, char *buf, size_t bufsz)
{
	nvlist_t *cfg = NULL;
	int nlabels = 0;
	int err;
	int fd = open(device, O_RDONLY | O_CLOEXEC);
	if (fd < 0)
		return (-1);
	err = zpool_read_label(fd, &cfg, &nlabels);
	close(fd);
	if (err != 0 || cfg == NULL)
		return (-1);
	const char *name = NULL;
	if (nvlist_lookup_string(cfg, ZPOOL_CONFIG_POOL_NAME,
	    &name) != 0 || name == NULL) {
		nvlist_free(cfg);
		return (-1);
	}
	snprintf(buf, bufsz, "%s", name);
	nvlist_free(cfg);
	return (0);
}

static void
fs_open(char *device)
{
	if (g_spa != NULL)
		return;

	if (!g_kernel_inited) {
		kernel_init(SPA_MODE_READ);
		g_kernel_inited = 1;
	}

	if (read_pool_name_from_label(device, g_pool_name,
	    sizeof (g_pool_name)) != 0) {
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: %s: no readable ZFS vdev label (is this a ZFS "
		    "partition?)\n", __FILE__, device);
	}

	int err = spa_open_rewind(g_pool_name, &g_spa, FTAG, NULL, NULL);

	if (err == ENOENT) {
		/*
		 * Pool not in the cachefile.  First try to find the pool
		 * via the source device itself - that avoids ambiguity
		 * when other stale ZFS-labelled devices are present on
		 * the system (e.g. a previously-restored clone of the
		 * same pool sitting on another disk).
		 */
		err = try_import_via_device(device);
		if (err == ENOENT) {
			/* Fall back to scanning all of /dev/ like zdb -e. */
			err = try_import_via_device("/dev");
		}
	}

	if (err != 0 || g_spa == NULL) {
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: cannot open pool '%s' (device %s): %s\n",
		    __FILE__, g_pool_name, device, strerror(err));
	}

	/* v1: enforce stripe-only pool. */
	vdev_t *rvd = g_spa->spa_root_vdev;
	if (rvd->vdev_children != 1) {
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: pool '%s' has %llu top-level vdevs; "
		    "partclone.zfs v1 supports only stripe pools\n",
		    __FILE__, g_pool_name,
		    (unsigned long long)rvd->vdev_children);
	}
	vdev_t *cvd = rvd->vdev_child[0];
	if (cvd->vdev_ms == NULL || cvd->vdev_ms_count == 0) {
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: top-level vdev has no metaslabs (not a stripe "
		    "leaf vdev)\n", __FILE__);
	}
}

static void
fs_close(void)
{
	if (g_spa != NULL) {
		spa_close(g_spa, FTAG);
		g_spa = NULL;
	}
	if (g_kernel_inited) {
		kernel_fini();
		g_kernel_inited = 0;
	}
}

/* ------------------------------------------------------------------ */
/* file_system_info fillers                                             */
/* ------------------------------------------------------------------ */

static unsigned long long
get_block_count(void)
{
	vdev_t *rvd = g_spa->spa_root_vdev;
	vdev_t *cvd = rvd->vdev_child[0];
	/* Use vdev_psize (partition size) so the bitmap covers the WHOLE
	 * partition - including the label area - not just the data area. */
	return (cvd->vdev_psize >> ZFS_SECTOR_SHIFT);
}

static unsigned long long
get_used_blocks_estimate(void)
{
	vdev_t *rvd = g_spa->spa_root_vdev;
	vdev_t *cvd = rvd->vdev_child[0];
	uint64_t alloc = 0;
	for (uint64_t m = 0; m < cvd->vdev_ms_count; m++) {
		metaslab_t *msp = cvd->vdev_ms[m];
		if (msp != NULL && msp->ms_sm != NULL)
			alloc += (uint64_t)space_map_allocated(msp->ms_sm);
	}
	return (alloc >> ZFS_SECTOR_SHIFT);
}

void
read_super_blocks(char *device, file_system_info *fs_info)
{
	fs_open(device);

	strncpy(fs_info->fs, zfs_MAGIC, FS_MAGIC_SIZE);
	fs_info->block_size  = ZFS_SECTOR_SIZE;
	fs_info->totalblock  = get_block_count();
	fs_info->usedblocks  = get_used_blocks_estimate();
	fs_info->device_size =
	    (unsigned long long)fs_info->block_size * fs_info->totalblock;
	fs_info->superBlockUsedBlocks = fs_info->usedblocks;

	log_mesg(1, 0, 0, fs_opt.debug, "%s: zfs pool '%s' block_size=%u "
	    "totalblock=%llu usedblocks=%llu device_size=%llu\n",
	    __FILE__, g_pool_name, fs_info->block_size, fs_info->totalblock,
	    fs_info->usedblocks, fs_info->device_size);

	fs_close();
}

void
read_bitmap(char *device, file_system_info fs_info, unsigned long *bitmap,
    int pui)
{
	walk_ctx_t ctx = {
		.bitmap       = bitmap,
		.total_blocks = fs_info.totalblock,
	};

	progress_bar prog;
	progress_init(&prog, 0, 4, 4, BITMAP, 1);

	fs_open(device);
	vdev_t *rvd = g_spa->spa_root_vdev;
	vdev_t *cvd = rvd->vdev_child[0];

	log_mesg(1, 0, 0, fs_opt.debug, "%s: read_bitmap: start, pool=%s "
	    "totalblock=%llu\n", __FILE__, g_pool_name, fs_info.totalblock);

	/* Start from "all free" then mark used regions. */
	pc_init_bitmap(bitmap, 0x00, fs_info.totalblock);

	/* Always mark vdev label areas used: labels + uberblock ring. */
	mark_range_used(&ctx, 0, VDEV_LABEL_START_SIZE);
	mark_range_used(&ctx, cvd->vdev_psize - VDEV_LABEL_END_SIZE,
	    VDEV_LABEL_END_SIZE);

	update_pui(&prog, 1, 1, 0);

	/* Walk live blkptrs (MOS + datasets + snapshots + indirects). */
	int terr = traverse_pool(g_spa, 0,
	    TRAVERSE_PRE | TRAVERSE_PREFETCH_METADATA, walk_cb, &ctx);
	if (terr != 0)
		log_mesg(2, 0, 0, fs_opt.debug,
		    "%s: traverse_pool returned %d\n", __FILE__, terr);

	update_pui(&prog, 2, 2, 0);

	/* Replay every metaslab's spacemap: catches orphan allocations that
	 * no live dnode points to (deferred frees, partially-destroyed
	 * objects). Produces the same total as `zdb -b`'s bp allocated when
	 * summed over the whole vdev. */
	for (uint64_t m = 0; m < cvd->vdev_ms_count; m++) {
		metaslab_t *msp = cvd->vdev_ms[m];
		if (msp == NULL || msp->ms_sm == NULL)
			continue;
		replay_metaslab_spacemap(g_spa, msp, &ctx);
	}

	update_pui(&prog, 3, 3, 0);

	/* Log space-maps (if enabled) add pending spacemap entries. */
	replay_log_spacemaps(g_spa, &ctx);

	fs_close();
	update_pui(&prog, 4, 4, 1);
}
