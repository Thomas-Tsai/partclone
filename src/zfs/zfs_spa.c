/**
 * zfs_spa.c - clean-room ZFS pool reader: device I/O, vdev label,
 * uberblock selection, and block (blkptr) reads with decompression.
 *
 * Format derived from the MIT-licensed specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/).
 * No OpenZFS (CDDL) source is used or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/fs.h>

#include "zfs_disk.h"
#include "zfs_nvlist.h"
#include "zfs_compress.h"
#include "zfs_spa.h"

static void nulllog(const char *fmt, ...) { (void)fmt; }

int
zspa_pread(zfs_spa_t *spa, void *buf, size_t len, uint64_t off)
{
	uint8_t *p = buf;
	size_t done = 0;

	while (done < len) {
		ssize_t n = pread(spa->fd, p + done, len - done,
		    (off_t)(off + done));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			return (-1);
		}
		if (n == 0)
			return (-1);
		done += (size_t)n;
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* block pointer reads                                                  */
/* ------------------------------------------------------------------ */

static int read_gang_children(zfs_spa_t *spa, const zfs_bp_t *bp,
    uint8_t *dst, uint64_t lsize);

static int read_bp_dva(zfs_spa_t *spa, const zfs_bp_t *bp, int dva_idx,
    uint8_t *raw, size_t rawlen)
{
	zfs_dva_t d;

	zbp_get_dva(bp, dva_idx, &d);
	if (d.asize == 0 || d.vdev != 0)
		return (-1);			/* v1: stripe only */
	if ((size_t)d.asize < rawlen)
		rawlen = d.asize;
	if ((uint64_t)d.offset + ZFS_LABEL_START_SIZE + rawlen > spa->psize)
		return (-1);
	/* DVA offset is data-area relative: add the 4 MiB label space */
	return (zspa_pread(spa, raw, rawlen,
	    (uint64_t)d.offset + ZFS_LABEL_START_SIZE));
}

static int read_bp_one(zfs_spa_t *spa, const zfs_bp_t *bp, uint8_t **out,
    size_t *out_len, int depth)
{
	uint64_t lsize, psize;
	int comp;
	uint8_t *dst, *raw;

	if (depth > 2)
		return (-1);
	if (zbp_is_hole(bp))
		return (-1);
	if (zbp_is_encrypted(bp))
		return (-1);			/* pool dir data not encr. */
	if (zbp_byteorder(bp) != 1)
		return (-1);			/* little-endian pools only */

	lsize = zbp_lsize(bp);
	comp  = zbp_comp(bp);

	dst = malloc(lsize ? lsize : 1);
	if (dst == NULL)
		return (-1);

	if (zbp_is_embedded(bp)) {
		uint8_t payload[ZBP_EMB_PAYLOAD_SIZE];
		psize = zbp_psize(bp);
		if (psize > sizeof (payload) || psize > lsize) {
			free(dst);
			return (-1);
		}
		zbp_embedded_payload(bp, payload);
		if (zfs_decompress(comp, payload, psize, dst, lsize) != 0) {
			free(dst);
			return (-1);
		}
		*out = dst;
		*out_len = lsize;
		return (0);
	}

	if (zbp_is_gang(bp)) {
		if (read_gang_children(spa, bp, dst, lsize) != 0) {
			free(dst);
			return (-1);
		}
		*out = dst;
		*out_len = lsize;
		return (0);
	}

	psize = zbp_psize(bp);
	raw = malloc(psize ? psize : 1);
	if (raw == NULL) {
		free(dst);
		return (-1);
	}

	for (int d = 0; d < 3; d++) {
		zfs_dva_t dva;
		zbp_get_dva(bp, d, &dva);
		if (dva.asize == 0)
			continue;
		if (read_bp_dva(spa, bp, d, raw, psize) != 0)
			continue;
		if (zfs_decompress(comp, raw, psize, dst, lsize) == 0) {
			free(raw);
			*out = dst;
			*out_len = lsize;
			return (0);
		}
	}
	free(raw);
	free(dst);
	return (-1);
}

/* gang: header holds child block pointers; read and concatenate */
static int read_gang_children(zfs_spa_t *spa, const zfs_bp_t *bp,
    uint8_t *dst, uint64_t lsize)
{
	uint8_t hdr[512];
	int n_children = (512 - 40) / ZBP_SIZE;	/* zio_eck trailer = 40 */
	size_t copied = 0;

	if (read_bp_dva(spa, bp, 0, hdr, sizeof (hdr)) != 0)
		return (-1);
	for (int i = 0; i < n_children && copied < lsize; i++) {
		zfs_bp_t child;
		uint8_t *cbuf = NULL;
		size_t clen = 0;

		memcpy(child.raw, hdr + i * ZBP_SIZE, ZBP_SIZE);
		if (zbp_is_hole(&child))
			continue;
		if (read_bp_one(spa, &child, &cbuf, &clen, 2) != 0)
			return (-1);
		if (copied + clen > lsize)
			clen = lsize - copied;
		memcpy(dst + copied, cbuf, clen);
		copied += clen;
		free(cbuf);
	}
	return (copied == lsize ? 0 : -1);
}

int
zspa_read_bp(zfs_spa_t *spa, const zfs_bp_t *bp, uint8_t **out,
    size_t *out_len)
{
	return (read_bp_one(spa, bp, out, out_len, 0));
}

/* ------------------------------------------------------------------ */
/* vdev label + uberblock selection                                     */
/* ------------------------------------------------------------------ */

static int parse_label(zfs_spa_t *spa, uint64_t base, znv_list_t **nv_out,
    void (*logf)(const char *fmt, ...))
{
	uint8_t buf[ZFS_LABEL_PHYS_SIZE];

	if (zspa_pread(spa, buf, sizeof (buf), base + ZFS_LABEL_PHYS_OFF) != 0)
		return (-1);
	if (znv_parse(buf, sizeof (buf), nv_out) != 0) {
		*nv_out = NULL;
		return (-1);
	}
	(void)logf;
	return (0);
}

static int extract_vdev_info(znv_list_t *nv, zfs_spa_t *spa,
    void (*logf)(const char *fmt, ...))
{
	const char *name, *type;
	znv_list_t *tree;
	uint64_t v;

	if (znv_get_string(nv, ZPOOL_CFG_POOL_NAME, &name) != 0)
		return (-1);
	snprintf(spa->pool_name, sizeof (spa->pool_name), "%s", name);
	znv_get_u64(nv, ZPOOL_CFG_POOL_GUID, &spa->pool_guid);
	znv_get_u64(nv, ZPOOL_CFG_TOP_GUID, &spa->top_guid);
	znv_get_u64(nv, ZPOOL_CFG_VERSION, &spa->version);

	if (znv_get_nvlist(nv, ZPOOL_CFG_VDEV_TREE, &tree) != 0) {
		logf("zfs v2: label has no vdev_tree\n");
		return (-1);
	}
	if (znv_get_string(tree, ZPOOL_CFG_TYPE, &type) != 0)
		return (-1);
	/* stripe-only: top-level vdev must be a plain disk/file leaf */
	if (strcmp(type, ZVDEV_TYPE_DISK) != 0 &&
	    strcmp(type, ZVDEV_TYPE_FILE) != 0) {
		logf("zfs v2: top-level vdev type '%s' unsupported "
		    "(stripe-only build)\n", type);
		return (-1);
	}
	if (znv_find(tree, ZPOOL_CFG_CHILDREN) != NULL) {
		logf("zfs v2: vdev has children (not a stripe leaf)\n");
		return (-1);
	}
	if (znv_get_u64(tree, ZPOOL_CFG_IS_LOG, &v) == 0 && v != 0) {
		logf("zfs v2: log device not supported\n");
		return (-1);
	}

	if (znv_get_u64(tree, ZPOOL_CFG_METASLAB_ARRAY,
	    &spa->metaslab_array) != 0 ||
	    znv_get_u64(tree, ZPOOL_CFG_METASLAB_SHIFT, &v) != 0)
		return (-1);
	spa->metaslab_shift = (uint32_t)v;
	if (znv_get_u64(tree, ZPOOL_CFG_ASHIFT, &v) != 0)
		v = 9;					/* fallback */
	spa->ashift = (uint32_t)v;
	if (znv_get_u64(tree, ZPOOL_CFG_ASIZE, &spa->asize) != 0)
		return (-1);

	spa->ms_count = spa->asize >> spa->metaslab_shift;
	return (0);
}

/* pick the best (txg, timestamp, mmp_seq) uberblock across 4 labels */
static int select_uberblock(zfs_spa_t *spa, uint8_t *best_ub,
    int *best_label, void (*logf)(const char *fmt, ...))
{
	uint64_t best_txg = 0, best_ts = 0, best_seq = 0;
	uint32_t slot_shift = zub_slot_shift(spa->ashift);
	uint32_t slot_size = 1U << slot_shift;
	uint32_t n_slots = ZFS_LABEL_UB_SIZE / slot_size;
	int found = 0;

	*best_label = -1;
	for (int l = 0; l < 4; l++) {
		uint64_t base = (l < 2) ? (uint64_t)l * ZFS_LABEL_SIZE
		    : spa->psize - (uint64_t)(4 - l) * ZFS_LABEL_SIZE;
		for (uint32_t s = 0; s < n_slots; s++) {
			uint8_t ub[ZUB_SIZE];
			uint64_t magic, txg, ts, seq = 0;
			uint64_t off = base + ZFS_LABEL_UB_OFF +
			    (uint64_t)s * slot_size;

			if (zspa_pread(spa, ub, sizeof (ub), off) != 0)
				break;
			magic = zle64(ub + ZUB_MAGIC);
			if (magic != ZUB_MAGIC_VALUE)
				continue;
			txg = zle64(ub + ZUB_TXG);
			ts  = zle64(ub + ZUB_TIMESTAMP);
			if (zle64(ub + ZUB_MMP_MAGIC) == 0xa11cea11ULL)
				seq = (zle64(ub + ZUB_MMP_CONFIG) >> 32) &
				    0xffff;
			if (!found || txg > best_txg ||
			    (txg == best_txg && ts > best_ts) ||
			    (txg == best_txg && ts == best_ts &&
			    seq > best_seq)) {
				found = 1;
				best_txg = txg;
				best_ts = ts;
				best_seq = seq;
				memcpy(best_ub, ub, sizeof (ub));
				*best_label = l;
			}
		}
	}
	if (found)
		logf("zfs v2: uberblock txg=%llu from label L%d\n",
		    (unsigned long long)best_txg, *best_label);
	return (found ? 0 : -1);
}

int
zspa_open(zfs_spa_t **spa_out, const char *device,
    void (*logf)(const char *fmt, ...))
{
	zfs_spa_t *spa;
	struct stat st;
	znv_list_t *best_nv = NULL;
	uint8_t best_ub[ZUB_SIZE];
	int best_label;

	if (logf == NULL)
		logf = nulllog;
	*spa_out = NULL;
	spa = calloc(1, sizeof (*spa));
	if (spa == NULL)
		return (-1);
	spa->fd = -1;

	spa->fd = open(device, O_RDONLY | O_CLOEXEC);
	if (spa->fd < 0) {
		logf("zfs v2: open %s: %s\n", device, strerror(errno));
		goto fail;
	}
	if (fstat(spa->fd, &st) != 0)
		goto fail;
	if (S_ISBLK(st.st_mode)) {
		uint64_t bytes = 0;
		if (ioctl(spa->fd, BLKGETSIZE64, &bytes) != 0 || bytes == 0)
			goto fail;
		spa->psize = bytes;
	} else if (S_ISREG(st.st_mode)) {
		spa->psize = (uint64_t)st.st_size;
	} else {
		goto fail;
	}

	/* find the newest valid uberblock (needs ashift: probe labels
	 * with the common ashift=9 slot layout first, then refine) */
	spa->ashift = 9;
	if (select_uberblock(spa, best_ub, &best_label, logf) != 0) {
		/* retry with ashift=12 (4Kn) slot layout */
		spa->ashift = 12;
		if (select_uberblock(spa, best_ub, &best_label, logf) != 0) {
			logf("zfs v2: no valid uberblock found\n");
			goto fail;
		}
	}

	if (parse_label(spa, (best_label < 2) ?
	    (uint64_t)best_label * ZFS_LABEL_SIZE :
	    spa->psize - (uint64_t)(4 - best_label) * ZFS_LABEL_SIZE,
	    &best_nv, logf) != 0) {
		logf("zfs v2: cannot parse vdev label nvlist\n");
		goto fail;
	}
	if (extract_vdev_info(best_nv, spa, logf) != 0) {
		logf("zfs v2: vdev_tree not usable (stripe-only)\n");
		goto fail;
	}
	znv_free(best_nv);
	best_nv = NULL;

	/* re-select uberblock with the true ashift slot layout */
	if (zub_slot_shift(spa->ashift) != zub_slot_shift(9)) {
		if (select_uberblock(spa, best_ub, &best_label, logf) != 0)
			goto fail;
	}

	memcpy(spa->ub, best_ub, sizeof (spa->ub));
	spa->ub_txg = zle64(best_ub + ZUB_TXG);
	memcpy(spa->mos_bp.raw, best_ub + ZUB_ROOTBP, ZBP_SIZE);

	/* MOS objset */
	if (zspa_read_bp(spa, &spa->mos_bp, &spa->mos_objset,
	    &spa->mos_objset_len) != 0) {
		logf("zfs v2: cannot read MOS objset (unsupported "
		    "compression?)\n");
		goto fail;
	}
	if (spa->mos_objset_len < ZDN_SIZE)
		goto fail;

	logf("zfs v2: pool '%s' guid=%llu ashift=%u ms_shift=%u "
	    "ms_count=%llu asize=%llu psize=%llu\n",
	    spa->pool_name, (unsigned long long)spa->pool_guid,
	    spa->ashift, spa->metaslab_shift,
	    (unsigned long long)spa->ms_count,
	    (unsigned long long)spa->asize,
	    (unsigned long long)spa->psize);

	*spa_out = spa;
	return (0);

fail:
	if (best_nv != NULL)
		znv_free(best_nv);
	zspa_close(spa);
	return (-1);
}

void
zspa_close(zfs_spa_t *spa)
{
	if (spa == NULL)
		return;
	if (spa->fd >= 0)
		close(spa->fd);
	free(spa->mos_objset);
	free(spa);
}
