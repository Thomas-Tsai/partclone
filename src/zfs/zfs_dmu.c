/**
 * zfs_dmu.c - clean-room DMU: dnode fetch and generic object reads
 * through indirect block trees.
 *
 * Format derived from the MIT-licensed specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/),
 * chapter 3 (DMU). No OpenZFS (CDDL) source is used or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <stdlib.h>
#include <string.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"

/* resolve the level-0 block pointer covering l0blkid (level-0 block id).
 * For nlevels==1 the pointer is dn->dn_blkptr[l0blkid]; otherwise walk
 * the indirect tree. */
static int find_l0_bp(zfs_spa_t *spa, const zfs_dnode_t *dn,
    uint64_t l0blkid, zfs_bp_t *out)
{
	int nlevels = zdn_nlevels(dn);
	uint32_t ind_bs;
	uint64_t n_ind;
	uint64_t path[16];	/* index per indirect level, leaf first */
	int depth;
	zfs_bp_t cur;
	uint8_t *blk = NULL;

	if (nlevels < 1 || nlevels > 12)
		return (-1);
	if (nlevels == 1) {
		if (l0blkid >= (uint64_t)zdn_nblkptr(dn))
			return (-1);
		zdn_get_bp(dn, (int)l0blkid, out);
		return (0);
	}

	ind_bs = 1U << zdn_indblkshift(dn);
	n_ind  = ind_bs / ZBP_SIZE;		/* bps per indirect block */

	/* leaf-to-root index path */
	depth = 0;
	while (depth < nlevels - 1) {
		path[depth++] = l0blkid % n_ind;
		l0blkid /= n_ind;
	}
	if (l0blkid >= (uint64_t)zdn_nblkptr(dn))
		return (-1);

	zdn_get_bp(dn, (int)l0blkid, &cur);
	for (int lvl = depth - 1; lvl >= 0; lvl--) {
		size_t blen = 0;

		if (zbp_is_hole(&cur)) {
			memset(out, 0, sizeof (*out));	/* hole bp */
			return (0);
		}
		if (zbp_is_embedded(&cur))
			return (-1);	/* indirect blocks are never embedded */
		if (zspa_read_bp(spa, &cur, &blk, &blen) != 0)
			return (-1);
		if (blen < (path[lvl] + 1) * ZBP_SIZE) {
			free(blk);
			return (-1);
		}
		memcpy(cur.raw, blk + path[lvl] * ZBP_SIZE, ZBP_SIZE);
		free(blk);
		blk = NULL;
	}
	memcpy(out, &cur, sizeof (cur));
	return (0);
}

/* read one level-0 data block and copy the requested byte window */
static int read_one_block(zfs_spa_t *spa, const zfs_dnode_t *dn,
    uint64_t l0blkid, uint8_t *dst, uint64_t blk_off, size_t len)
{
	zfs_bp_t bp;
	uint8_t *data = NULL;
	size_t dlen = 0;

	if (find_l0_bp(spa, dn, l0blkid, &bp) != 0)
		return (-1);

	if (zbp_is_hole(&bp) ||
	    (zle64(bp.raw) == 0 && zle64(bp.raw + 8) == 0 &&
	    zbp_lsize(&bp) == 0)) {
		memset(dst, 0, len);
		return (0);
	}

	if (zspa_read_bp(spa, &bp, &data, &dlen) != 0)
		return (-1);
	if (blk_off + len > dlen) {		/* short data: zero-fill */
		if (blk_off >= dlen) {
			memset(dst, 0, len);
		} else {
			memcpy(dst, data + blk_off, dlen - blk_off);
			memset(dst + (dlen - blk_off), 0,
			    len - (dlen - blk_off));
		}
	} else {
		memcpy(dst, data + blk_off, len);
	}
	free(data);
	return (0);
}

int
zdmu_read(zfs_spa_t *spa, const zfs_dnode_t *dn, uint64_t off, void *buf,
    size_t len)
{
	uint64_t data_bs = zdn_datablksz(dn);
	uint8_t *dst = buf;

	if (data_bs == 0 || len == 0)
		return (len == 0 ? 0 : -1);

	while (len > 0) {
		uint64_t blkid = off / data_bs;
		uint64_t blkoff = off % data_bs;
		size_t chunk = (size_t)(data_bs - blkoff);

		if (chunk > len)
			chunk = len;
		if (read_one_block(spa, dn, blkid, dst, blkoff, chunk) != 0)
			return (-1);
		dst += chunk;
		off += chunk;
		len -= chunk;
	}
	return (0);
}

int
zdmu_dnode_of(zfs_spa_t *spa, const zfs_dnode_t *metadn, uint64_t objid,
    zfs_dnode_t *out)
{
	uint64_t off = objid << ZDN_SHIFT;

	if (zdmu_read(spa, metadn, off, out->raw, ZDN_SIZE) != 0)
		return (-1);
	if (zdn_type(out) == ZOT_NONE)
		return (-1);
	return (0);
}

int
zdmu_bonus_of(zfs_spa_t *spa, const zfs_dnode_t *metadn, uint64_t objid,
    zfs_dnode_t *out)
{
	if (zdmu_dnode_of(spa, metadn, objid, out) != 0)
		return (-1);
	if (zdn_bonuslen(out) == 0)
		return (-1);
	return (0);
}
