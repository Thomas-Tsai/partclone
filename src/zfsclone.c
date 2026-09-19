/**
 * zfsclone.c - part of Partclone project
 *
 * ZFS used-block bitmap provider, **clean-room reimplementation**.
 *
 * Unlike zfsclone.c (v1), this driver links no OpenZFS library
 * (libzpool/libzfs are CDDL): it parses the on-disk format directly,
 * following the MIT-licensed independent specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/).
 * Only license-clean external libraries are used: liblz4 (BSD) and
 * zlib (zlib license).
 *
 * v1 scope (same as zfsclone.c v1):
 *   - stripe pool, single top-level "disk" vdev
 *   - user supplies the ZFS partition (e.g. /dev/sda1)
 *   - little-endian pools only (x86/arm64 LE hosts)
 *
 * Algorithm:
 *   1. Read 4 vdev labels; parse the XDR nvlist in each; pick the
 *      newest valid uberblock (highest txg/timestamp/mmp-seq).
 *   2. Read the MOS objset via ub_rootbp (decompress as needed).
 *   3. MOS object 1 (object directory ZAP) -> "config" and the
 *      optional "com.delphix:log_spacemap_zap".
 *   4. config (packed nvlist) -> vdev_tree -> top vdev ->
 *      "com.delphix:vdev_zap_top" -> "com.delphix:ms_unflushed_phys_txgs".
 *   5. Replay every metaslab's space map (alloc/free) into the bitmap.
 *   6. Replay log space maps with txg > per-metaslab unflushed txg
 *      (txg <= unflushed entries are already folded into step 5).
 *      If the unflushed array is unavailable, apply alloc entries and
 *      skip frees (safe over-estimate).
 *   7. Mark the vdev label area (first 4 MiB; last 512 KiB) used.
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <config.h>
#include "partclone.h"
#include "zfsclone.h"
#include "progress.h"
#include "fs_common.h"

#include "zfs/zfs_disk.h"
#include "zfs/zfs_nvlist.h"
#include "zfs/zfs_spa.h"
#include "zfs/zfs_dmu.h"
#include "zfs/zfs_zap.h"
#include "zfs/zfs_spacemap.h"
#include "zfs/zfs_traverse.h"

#define ZFS_SECTOR_SHIFT	9
#define ZFS_SECTOR_SIZE		(1ULL << ZFS_SECTOR_SHIFT)	/* 512 */

static zfs_spa_t	*g_spa;
static zfs_dnode_t	 g_metadn;		/* MOS metadnode */

/* ------------------------------------------------------------------ */
/* logging adapter (libintl-style log_mesg is printf-based)            */
/* ------------------------------------------------------------------ */

static void vlogf(const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof (buf), fmt, ap);
	va_end(ap);
	log_mesg(1, 0, 0, fs_opt.debug, "%s", buf);
}

/* ------------------------------------------------------------------ */
/* bitmap helpers                                                       */
/* ------------------------------------------------------------------ */

typedef struct {
	unsigned long	*bitmap;
	uint64_t	total_blocks;
	uint64_t	alloc_bytes;	/* running total of marked space */
} bm_ctx_t;

static bm_ctx_t g_bm;

static void bm_range(uint64_t off, uint64_t len, int used)
{
	uint64_t first, last;

	if (len == 0)
		return;
	first = off >> ZFS_SECTOR_SHIFT;
	last  = (off + len - 1) >> ZFS_SECTOR_SHIFT;
	if (first >= g_bm.total_blocks)
		return;
	if (last >= g_bm.total_blocks)
		last = g_bm.total_blocks - 1;
	for (uint64_t s = first; s <= last; s++) {
		if (used)
			pc_set_bit(s, g_bm.bitmap, g_bm.total_blocks);
		else
			pc_clear_bit(s, g_bm.bitmap, g_bm.total_blocks);
	}
}

/* space-map / DVA offsets are data-area relative: add 4 MiB */
static void bm_range_dva(uint64_t dva_off, uint64_t len, int used)
{
	bm_range(dva_off + ZFS_LABEL_START_SIZE, len, used);
}

/* metaslab space map callback (vdev 0, absolute data-area offsets) */
static void apply_ms(int is_alloc, uint64_t off, uint64_t run,
    uint64_t vdev, uint64_t txg, void *arg)
{
	(void)vdev; (void)txg; (void)arg;
	bm_range_dva(off, run, is_alloc);
}

/* MOS traversal callback: mark every live bp's DVAs */
static void apply_traverse(const zfs_bp_t *bp, void *arg)
{
	(void)arg;
	for (int d = 0; d < 3; d++) {
		zfs_dva_t dva;

		zbp_get_dva(bp, d, &dva);
		if (dva.asize == 0 || dva.vdev != 0)
			continue;
		bm_range_dva(dva.offset, dva.asize, 1);
	}
}

/* log space map callback: filter by per-metaslab unflushed txg */
typedef struct {
	const uint64_t	*unflushed;	/* per-metaslab txg array or NULL */
	uint64_t	ms_count;
	uint32_t	ms_shift;
	uint64_t	txg;		/* this log's target txg */
	int		allocs_only;	/* unflushed unavailable: safe mode */
	uint64_t	applied;	/* entries applied (debug) */
	uint64_t	skipped;	/* entries filtered out */
} log_ctx_t;

static void apply_log(int is_alloc, uint64_t off, uint64_t run,
    uint64_t vdev, uint64_t txg_dbg, void *arg)
{
	log_ctx_t *ctx = arg;
	uint64_t m;

	(void)txg_dbg;
	if (vdev != 0)
		return;				/* v1: stripe only */
	m = off >> ctx->ms_shift;
	if (m >= ctx->ms_count)
		return;
	if (ctx->unflushed != NULL) {
		if (ctx->txg <= ctx->unflushed[m]) {
			ctx->skipped++;
			return;			/* already flushed */
		}
	} else if (ctx->allocs_only && !is_alloc) {
		return;				/* safe over-estimate */
	}
	ctx->applied++;
	bm_range_dva(off, run, is_alloc);
}

/* ------------------------------------------------------------------ */
/* MOS anchor resolution                                                */
/* ------------------------------------------------------------------ */

static int resolve_anchors(zfs_spa_t *spa)
{
	zfs_dnode_t objdir_dn;
	zfs_dnode_t cfg_dn;
	uint8_t *cfg = NULL;
	size_t cfg_len;
	znv_list_t *nv = NULL, *tree = NULL, *child = NULL;
	znv_pair_t *children;
	int rc = -1;

	zdmu_objset_metadnode(spa->mos_objset, &g_metadn);

	if (zdmu_dnode_of(spa, &g_metadn, ZMOS_OBJECT_DIRECTORY,
	    &objdir_dn) != 0) {
		vlogf("zfs: cannot read MOS object directory dnode\n");
		return (-1);
	}

	if (zzap_lookup_u64(spa, &objdir_dn, ZMOS_CFG_OBJECT,
	    &spa->config_obj) != 0) {
		vlogf("zfs: object directory lacks 'config'\n");
		return (-1);
	}
	/* optional: log spacemap zap object id */
	if (zzap_lookup_u64(spa, &objdir_dn, ZMOS_LOG_SPACEMAP_ZAP,
	    &spa->log_spacemap_zap) != 0)
		spa->log_spacemap_zap = 0;

	/* config object: packed nvlist */
	if (zdmu_dnode_of(spa, &g_metadn, spa->config_obj, &cfg_dn) != 0)
		return (-1);
	cfg_len = (size_t)zdn_datablksz(&cfg_dn) *
	    (uint64_t)(zle64(cfg_dn.raw + ZDN_MAXBLKID) + 1);
	if (cfg_len == 0 || cfg_len > (1U << 20))
		return (-1);
	cfg = malloc(cfg_len);
	if (cfg == NULL)
		return (-1);
	if (zdmu_read(spa, &cfg_dn, 0, cfg, cfg_len) != 0)
		goto out;
	if (znv_parse(cfg, cfg_len, &nv) != 0) {
		vlogf("zfs: cannot parse MOS config nvlist\n");
		goto out;
	}
	if (znv_get_nvlist(nv, ZPOOL_CFG_VDEV_TREE, &tree) != 0)
		goto out;
	children = znv_find(tree, ZPOOL_CFG_CHILDREN);
	if (children == NULL || children->type != ZNV_NVLIST_ARRAY ||
	    children->nelem != 1) {
		vlogf("zfs: root vdev has != 1 children "
		    "(stripe-only build)\n");
		goto out;
	}
	child = children->childa[0];

	/* optional per-top-vdev ZAP -> unflushed phys txgs object */
	if (znv_get_u64(child, ZVDEV_TOP_ZAP_KEY, &spa->vdev_top_zap) == 0) {
		zfs_dnode_t vzap_dn;

		if (zdmu_dnode_of(spa, &g_metadn, spa->vdev_top_zap,
		    &vzap_dn) == 0 &&
		    zzap_lookup_u64(spa, &vzap_dn, ZVDEV_TOP_ZAP_UNFLUSHED,
		    &spa->unflushed_obj) != 0)
			spa->unflushed_obj = 0;
	}
	rc = 0;
	vlogf("zfs: config_obj=%llu log_sm_zap=%llu vdev_top_zap=%llu "
	    "unflushed_obj=%llu\n",
	    (unsigned long long)spa->config_obj,
	    (unsigned long long)spa->log_spacemap_zap,
	    (unsigned long long)spa->vdev_top_zap,
	    (unsigned long long)spa->unflushed_obj);
out:
	if (nv != NULL)
		znv_free(nv);
	free(cfg);
	return (rc);
}

/* ------------------------------------------------------------------ */
/* metaslab array + unflushed array loading                             */
/* ------------------------------------------------------------------ */

static uint64_t *load_u64_array(zfs_spa_t *spa, uint64_t obj,
    uint64_t count)
{
	zfs_dnode_t dn;
	uint64_t *arr;

	if (obj == 0 || count == 0 || count > (1ULL << 24))
		return (NULL);
	if (zdmu_dnode_of(spa, &g_metadn, obj, &dn) != 0)
		return (NULL);
	arr = malloc(count * sizeof (uint64_t));
	if (arr == NULL)
		return (NULL);
	if (zdmu_read(spa, &dn, 0, arr, count * sizeof (uint64_t)) != 0) {
		free(arr);
		return (NULL);
	}
	return (arr);
}

/* ------------------------------------------------------------------ */
/* log spacemap zap: collect (txg -> sm object) pairs                   */
/* ------------------------------------------------------------------ */

typedef struct {
	uint64_t	*txgs;
	uint64_t	*objs;
	size_t		n;
	size_t		cap;
} log_list_t;

static int log_collect_cb(const uint8_t *name, uint32_t name_len,
    uint64_t value, int has_value, void *arg)
{
	log_list_t *l = arg;
	uint64_t txg = 0;

	/* keys are the txg number as a lowercase hex ASCII string
	 * (empirically observed format, e.g. "4b", "88", "c5") */
	if (name_len < 2 || name_len > 17 || !has_value ||
	    name[name_len - 1] != '\0')
		return (0);
	for (uint32_t i = 0; i + 1 < name_len; i++) {
		uint8_t c = name[i];
		uint32_t d;

		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else
			return (0);
		txg = (txg << 4) | d;
	}
	if (l->n == l->cap) {
		size_t ncap = l->cap ? l->cap * 2 : 64;
		uint64_t *nt = realloc(l->txgs, ncap * sizeof (uint64_t));
		uint64_t *no;

		if (nt == NULL)
			return (-1);
		l->txgs = nt;
		no = realloc(l->objs, ncap * sizeof (uint64_t));
		if (no == NULL)
			return (-1);
		l->objs = no;
		l->cap = ncap;
	}
	l->txgs[l->n] = txg;
	l->objs[l->n] = value;
	l->n++;
	return (0);
}

static int cmp_u64_pair(const void *a, const void *b, void *ctx)
{
	const log_list_t *l = ctx;
	size_t ia = *(const size_t *)a, ib = *(const size_t *)b;

	if (l->txgs[ia] < l->txgs[ib])
		return (-1);
	if (l->txgs[ia] > l->txgs[ib])
		return (1);
	return (0);
}

/* replay log spacemaps in ascending txg order */
static void replay_logs(zfs_spa_t *spa, const uint64_t *unflushed)
{
	zfs_dnode_t zap_dn;
	log_list_t list = { 0 };
	size_t *order = NULL;
	log_ctx_t ctx;

	if (spa->log_spacemap_zap == 0)
		return;
	if (zdmu_dnode_of(spa, &g_metadn, spa->log_spacemap_zap,
	    &zap_dn) != 0)
		return;
	if (zzap_iterate(spa, &zap_dn, log_collect_cb, &list) < 0 ||
	    list.n == 0)
		goto out;

	order = malloc(list.n * sizeof (size_t));
	if (order == NULL)
		goto out;
	for (size_t i = 0; i < list.n; i++)
		order[i] = i;
	qsort_r(order, list.n, sizeof (size_t), cmp_u64_pair, &list);

	ctx.unflushed   = unflushed;
	ctx.ms_count    = spa->ms_count;
	ctx.ms_shift    = spa->metaslab_shift;
	ctx.allocs_only = (unflushed == NULL);

	for (size_t i = 0; i < list.n; i++) {
		ctx.txg = list.txgs[order[i]];
		ctx.applied = 0;
		ctx.skipped = 0;
		if (zsm_replay_object(spa, &g_metadn, list.objs[order[i]],
		    0, ZSM_LOG_SHIFT, apply_log, &ctx, NULL) != 0)
			vlogf("zfs: log txg=%llu obj=%llu UNREADABLE\n",
			    (unsigned long long)ctx.txg,
			    (unsigned long long)list.objs[order[i]]);
	}
	vlogf("zfs: replayed %zu log spacemaps%s\n", list.n,
	    ctx.allocs_only ? " (alloc-only: unflushed unavailable)" : "");
out:
	free(order);
	free(list.txgs);
	free(list.objs);
}

/* ------------------------------------------------------------------ */
/* used-block estimate (for fs_info->usedblocks)                        */
/* ------------------------------------------------------------------ */

static uint64_t estimate_used(zfs_spa_t *spa, const uint64_t *ms_arr)
{
	uint64_t total = 2 * ZFS_LABEL_START_SIZE;	/* labels, approx */

	for (uint64_t m = 0; m < spa->ms_count; m++) {
		uint64_t smp_alloc = 0;

		if (ms_arr[m] == 0)
			continue;
		if (zsm_read_header(spa, &g_metadn, ms_arr[m], NULL, NULL,
		    &smp_alloc) == 0)
			total += smp_alloc;
	}
	return (total >> ZFS_SECTOR_SHIFT);
}

/* ------------------------------------------------------------------ */
/* fs_open / fs_close                                                   */
/* ------------------------------------------------------------------ */

static void fs_open(char *device)
{
	if (g_spa != NULL)
		return;
	if (zspa_open(&g_spa, device, vlogf) != 0)
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: %s: not a readable ZFS stripe pool (v2)\n",
		    __FILE__, device);
	if (resolve_anchors(g_spa) != 0) {
		zspa_close(g_spa);
		g_spa = NULL;
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: cannot resolve MOS anchors on %s\n",
		    __FILE__, device);
	}
}

static void fs_close(void)
{
	if (g_spa != NULL) {
		zspa_close(g_spa);
		g_spa = NULL;
	}
}

/* ------------------------------------------------------------------ */
/* partclone interface                                                  */
/* ------------------------------------------------------------------ */

void read_super_blocks(char *device, file_system_info *fs_info)
{
	uint64_t *ms_arr = NULL;

	fs_open(device);

	strncpy(fs_info->fs, zfs_MAGIC, FS_MAGIC_SIZE);
	fs_info->block_size  = ZFS_SECTOR_SIZE;
	fs_info->totalblock  =
	    (unsigned long long)(g_spa->psize >> ZFS_SECTOR_SHIFT);

	ms_arr = load_u64_array(g_spa, g_spa->metaslab_array,
	    g_spa->ms_count);
	fs_info->usedblocks = (ms_arr != NULL) ? estimate_used(g_spa, ms_arr)
	    : fs_info->totalblock / 10;		/* crude fallback */
	free(ms_arr);

	fs_info->device_size =
	    (unsigned long long)fs_info->block_size * fs_info->totalblock;
	fs_info->superBlockUsedBlocks = fs_info->usedblocks;

	log_mesg(1, 0, 0, fs_opt.debug,
	    "%s: zfs pool '%s' block_size=%u totalblock=%llu "
	    "usedblocks~=%llu device_size=%llu\n",
	    __FILE__, g_spa->pool_name, fs_info->block_size,
	    fs_info->totalblock, fs_info->usedblocks, fs_info->device_size);

	fs_close();
}

void read_bitmap(char *device, file_system_info fs_info,
    unsigned long *bitmap, int pui)
{
	uint64_t *ms_arr = NULL;
	uint64_t *unflushed = NULL;
	progress_bar prog;

	(void)pui;
	progress_init(&prog, 0, 5, 5, BITMAP, 1);

	fs_open(device);

	log_mesg(1, 0, 0, fs_opt.debug,
	    "%s: read_bitmap: start pool=%s totalblock=%llu\n",
	    __FILE__, g_spa->pool_name, fs_info.totalblock);

	g_bm.bitmap = bitmap;
	g_bm.total_blocks = fs_info.totalblock;
	pc_init_bitmap(bitmap, 0x00, fs_info.totalblock);

	/* vdev label areas: first 4 MiB, last 512 KiB */
	bm_range(0, ZFS_LABEL_START_SIZE, 1);
	bm_range(g_spa->psize - ZFS_LABEL_END_SIZE, ZFS_LABEL_END_SIZE, 1);

	/* The current uberblock's MOS root block (all ditto copies) is
	 * allocated out-of-band during the txg commit: neither the flushed
	 * metaslab maps nor the log spacemaps track it (observed on a pool
	 * exported at txg 26 whose ub_rootbp's DVA[2] landed in a metaslab
	 * with no space map object at all). Always mark it. */
	{
		const zfs_bp_t *rb = &g_spa->mos_bp;
		for (int d = 0; d < 3; d++) {
			zfs_dva_t dva;
			zbp_get_dva(rb, d, &dva);
			if (dva.asize == 0)
				continue;
			bm_range_dva(dva.offset, dva.asize, 1);
			log_mesg(1, 0, 0, fs_opt.debug,
			    "%s: ub rootbp dva[%d] off=%#llx asize=%#llx "
			    "marked\n", __FILE__, d,
			    (unsigned long long)dva.offset,
			    (unsigned long long)dva.asize);
		}
	}
	update_pui(&prog, 1, 1, 0);

	/* Walk every live block pointer in the MOS subtree. This is the
	 * comprehensive safety net covering out-of-band commit blocks
	 * (final uberblock sync writes: new MOS root, object directory,
	 * config, per-vdev ZAPs, log spacemap objects themselves, ...)
	 * that no spacemap records. */
	if (ztraverse_objset(g_spa, g_spa->mos_objset, g_spa->mos_objset_len,
	    apply_traverse, NULL) != 0)
		log_mesg(2, 0, 0, fs_opt.debug,
		    "%s: MOS traversal returned error\n", __FILE__);
	update_pui(&prog, 2, 2, 0);

	/* metaslab space maps */
	ms_arr = load_u64_array(g_spa, g_spa->metaslab_array,
	    g_spa->ms_count);
	if (ms_arr == NULL)
		log_mesg(0, 1, 1, fs_opt.debug,
		    "%s: cannot read metaslab array object %llu\n",
		    __FILE__, (unsigned long long)g_spa->metaslab_array);
	for (uint64_t m = 0; m < g_spa->ms_count; m++) {
		if (ms_arr[m] == 0)
			continue;
		if (zsm_replay_object(g_spa, &g_metadn, ms_arr[m],
		    m << g_spa->metaslab_shift, g_spa->ashift, apply_ms,
		    NULL, NULL) != 0)
			log_mesg(2, 0, 0, fs_opt.debug,
			    "%s: metaslab %llu (sm obj %llu) unreadable\n",
			    __FILE__, (unsigned long long)m,
			    (unsigned long long)ms_arr[m]);
	}
	free(ms_arr);
	update_pui(&prog, 3, 3, 0);

	/* log space maps (feature@log_spacemap) */
	unflushed = load_u64_array(g_spa, g_spa->unflushed_obj,
	    g_spa->ms_count);
	replay_logs(g_spa, unflushed);
	free(unflushed);
	update_pui(&prog, 4, 4, 0);

	fs_close();
	update_pui(&prog, 5, 5, 1);
}
