/**
 * zfs_traverse.h - clean-room MOS traversal: walk every live block
 * pointer reachable from an objset's metadnode and report it.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_TRAVERSE_H
#define ZFS_TRAVERSE_H

#include <stdint.h>

#include "zfs_disk.h"
#include "zfs_spa.h"

typedef void (*ztraverse_cb)(const zfs_bp_t *bp, void *arg);

/* Walk the dnode array of an objset (MOS or dataset) and invoke cb for
 * every non-hole, non-embedded block pointer at every indirection
 * level. 0 on success, -1 on structural error. */
int	ztraverse_objset(zfs_spa_t *spa, const uint8_t *objset,
	    size_t objset_len, ztraverse_cb cb, void *arg);

#endif /* ZFS_TRAVERSE_H */
