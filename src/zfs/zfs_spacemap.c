/**
 * zfs_spacemap.c - clean-room space map decoding and replay.
 *
 * Format derived from the MIT-licensed specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/),
 * chapter 9 (space maps). No OpenZFS (CDDL) source is used or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <stdlib.h>
#include <string.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"
#include "zfs_spacemap.h"

int
zsm_read_header(zfs_spa_t *spa, const zfs_dnode_t *metadn,
    uint64_t sm_object, zfs_dnode_t *dn_out, uint64_t *length_out,
    uint64_t *alloc_out)
{
	zfs_dnode_t *dn = dn_out;

	if (dn == NULL) {
		static zfs_dnode_t tmp;			/* single-threaded */
		dn = &tmp;
	}
	if (zdmu_dnode_of(spa, metadn, sm_object, dn) != 0)
		return (-1);
	if (zdn_bonustype(dn) != ZOT_SPACE_MAP_HEADER ||
	    zdn_bonuslen(dn) < 0x18)
		return (-1);
	if (length_out != NULL)
		*length_out = zle64(zdn_bonus(dn) + ZSMP_LENGTH);
	if (alloc_out != NULL)
		*alloc_out = zle64(zdn_bonus(dn) + ZSMP_ALLOC);
	return (0);
}

int
zsm_replay_object(zfs_spa_t *spa, const zfs_dnode_t *metadn,
    uint64_t sm_object, uint64_t base_off, uint32_t shift,
    zsm_apply_cb cb, void *arg, uint64_t *smp_alloc_out)
{
	zfs_dnode_t dn;
	uint64_t length;
	uint64_t off = 0;
	uint64_t cur_txg = 0;
	uint8_t wordbuf[8];

	if (zsm_read_header(spa, metadn, sm_object, &dn, &length,
	    smp_alloc_out) != 0)
		return (-1);
	if (length > (1ULL << 34))			/* sanity: 16 GiB */
		return (-1);

	while (off + 8 <= length) {
		uint64_t word;

		if (zdmu_read(spa, &dn, off, wordbuf, 8) != 0)
			return (-1);
		word = zle64(wordbuf);
		off += 8;

		if (ZSM_DEBUG_PREFIX(word)) {
			/* debug entry: carry txg for logging context;
			 * txg==0 entries are pure padding */
			uint64_t txg = word & ((1ULL << 50) - 1);
			if (txg != 0)
				cur_txg = txg;
			continue;
		}

		if (!ZSM_TWO_WORD_PREFIX(word)) {
			/* one-word: offset relative to base_off */
			uint64_t o = ZSM1_OFFSET(word) << shift;
			uint64_t r = ZSM1_RUN(word) << shift;
			int is_alloc = (ZSM1_TYPE(word) == ZSM_TYPE_ALLOC);

			if (cb != NULL)
				cb(is_alloc, base_off + o, r, 0, cur_txg,
				    arg);
		} else {
			uint64_t w1, r, o, vdev;
			int is_alloc;

			if (off + 8 > length)
				return (-1);
			if (zdmu_read(spa, &dn, off, wordbuf, 8) != 0)
				return (-1);
			w1 = zle64(wordbuf);
			off += 8;

			r    = ZSM2_RUN(word) << shift;
			vdev = ZSM2_VDEV(word);
			is_alloc = (ZSM2_TYPE(w1) == ZSM_TYPE_ALLOC);
			o    = ZSM2_OFFSET(w1) << shift;
			/* Two-word offsets obey the same relativity rule as
			 * one-word ones. For metaslab maps (base_off = slab
			 * start) they are slab-relative; for pool log maps
			 * (base_off = 0) vdev-relative. Condensed slab maps
			 * are full of two-word entries at slab-relative
			 * offsets - treating them as absolute is the classic
			 * 24K-sector drift bug. */
			if (cb != NULL)
				cb(is_alloc, base_off + o, r, vdev, cur_txg,
				    arg);
		}
	}
	return (0);
}
