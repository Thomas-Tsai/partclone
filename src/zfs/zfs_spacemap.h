/**
 * zfs_spacemap.h - clean-room space map decoding and replay.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_SPACEMAP_H
#define ZFS_SPACEMAP_H

#include <stdint.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"

/* one decoded alloc/free segment.
 *   is_alloc: 1 = SM_ALLOC, 0 = SM_FREE
 *   off_bytes/run_bytes: byte offset (base-relative) and length
 *   txg: transaction group of the enclosing debug entry (0 if none) */
typedef void (*zsm_apply_cb)(int is_alloc, uint64_t off_bytes,
    uint64_t run_bytes, uint64_t vdev, uint64_t txg, void *arg);

/* read only the space_map_phys_t bonus: dnode + smp_length + smp_alloc */
int	zsm_read_header(zfs_spa_t *spa, const zfs_dnode_t *metadn,
	    uint64_t sm_object, zfs_dnode_t *dn_out, uint64_t *length_out,
	    uint64_t *alloc_out);

/* decode the space map object [0, smp_length) and apply segments.
 * Offsets in one-word entries are relative to base_off (metaslab start
 * in data-area bytes); two-word entries carry absolute (vdev-relative)
 * offsets and ignore base_off.
 * Returns 0 on success, -1 if the object/dnode could not be read. */
int	zsm_replay_object(zfs_spa_t *spa, const zfs_dnode_t *metadn,
	    uint64_t sm_object, uint64_t base_off, uint32_t shift,
	    zsm_apply_cb cb, void *arg, uint64_t *smp_alloc_out);

#endif /* ZFS_SPACEMAP_H */
