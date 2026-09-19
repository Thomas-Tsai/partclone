/**
 * zfs_traverse.c - clean-room MOS traversal: walk every live block
 * pointer reachable from an objset's metadnode.
 *
 * Motivation: vdev spacemap replay alone misses "out-of-band" commit
 * blocks such as the current uberblock's MOS root and the final
 * flush-commit writes (observed: pool exported at txg 26 had its
 * ub_rootbp in a metaslab with no space map object at all, and the
 * object directory / config objects likewise untracked). A direct
 * traversal covers every pointer actually live, complementing the
 * spacemaps (which additionally cover deferred allocations no dnode
 * points to).
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <stdlib.h>
#include <string.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"
#include "zfs_traverse.h"

static int walk_bp(zfs_spa_t *spa, const zfs_bp_t *bp, ztraverse_cb cb,
    void *arg, int depth)
{
	uint8_t *blk;
	size_t blen;
	uint32_t n;
	int rc = -1;

	if (depth > 12 || zbp_is_hole(bp) || zbp_is_embedded(bp))
		return (0);
	cb(bp, arg);
	if (zbp_level(bp) == 0)
		return (0);

	/* indirect block: array of child block pointers */
	if (zspa_read_bp(spa, bp, &blk, &blen) != 0)
		return (-1);
	n = (uint32_t)(blen / ZBP_SIZE);
	for (uint32_t i = 0; i < n; i++) {
		zfs_bp_t child;

		memcpy(child.raw, blk + (size_t)i * ZBP_SIZE, ZBP_SIZE);
		if (walk_bp(spa, &child, cb, arg, depth + 1) != 0)
			goto out;
	}
	rc = 0;
out:
	free(blk);
	return (rc);
}

/* mark every blkptr of one dnode tree */
static int walk_dnode(zfs_spa_t *spa, const zfs_dnode_t *dn,
    ztraverse_cb cb, void *arg)
{
	int n = zdn_nblkptr(dn);

	if (n < 1 || n > 3)
		return (-1);
	for (int i = 0; i < n; i++) {
		zfs_bp_t bp;

		zdn_get_bp(dn, i, &bp);
		if (walk_bp(spa, &bp, cb, arg, 0) != 0)
			return (-1);
	}
	return (0);
}

int
ztraverse_objset(zfs_spa_t *spa, const uint8_t *objset, size_t objset_len,
    ztraverse_cb cb, void *arg)
{
	zfs_dnode_t metadn;
	uint64_t data_bs;
	uint64_t maxblk;

	if (objset_len < ZDN_SIZE)
		return (-1);
	zdmu_objset_metadnode(objset, &metadn);

	/* metadnode's own blkptr tree (covers the dnode array blocks) */
	if (walk_dnode(spa, &metadn, cb, arg) != 0)
		return (-1);

	/* walk every dnode slot in the array */
	data_bs = zdn_datablksz(&metadn);
	maxblk = zle64(metadn.raw + ZDN_MAXBLKID);
	if (data_bs == 0 || maxblk > (1ULL << 20))
		return (-1);

	uint64_t total_dnodes = (maxblk + 1) * (data_bs >> ZDN_SHIFT);
	uint8_t *arrblk = malloc(data_bs);
	if (arrblk == NULL)
		return (-1);

	for (uint64_t b = 0; b <= maxblk; b++) {
		zfs_dnode_t dn;
		uint64_t off = b * data_bs;

		if (zdmu_read(spa, &metadn, off, arrblk, data_bs) != 0) {
			free(arrblk);
			return (-1);
		}
		for (uint32_t s = 0; s < (uint32_t)(data_bs >> ZDN_SHIFT);
		    s++) {
			const zfs_dnode_t *cur =
			    (const zfs_dnode_t *)(arrblk +
			    (size_t)s * ZDN_SIZE);
			if (zdn_type(cur) == ZOT_NONE)
				continue;
			memcpy(dn.raw, cur->raw, ZDN_SIZE);
			if (walk_dnode(spa, &dn, cb, arg) != 0) {
				free(arrblk);
				return (-1);
			}
		}
		(void)total_dnodes;
	}
	free(arrblk);
	return (0);
}
