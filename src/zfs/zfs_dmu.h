/**
 * zfs_dmu.h - clean-room DMU: dnode fetch and generic object reads
 * through indirect block trees.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_DMU_H
#define ZFS_DMU_H

#include <stdint.h>
#include <stddef.h>

#include "zfs_disk.h"
#include "zfs_spa.h"

/* metadnode of an objset: first 512 bytes of the objset block */
static inline void zdmu_objset_metadnode(const uint8_t *objset,
    zfs_dnode_t *out)
{
	memcpy(out->raw, objset + ZOS_METADNODE_OFF, ZDN_SIZE);
}

/* read bytes [off, off+len) of an object described by dnode dn.
 * Holes and zero regions return zeroes. Returns 0 on success. */
int	zdmu_read(zfs_spa_t *spa, const zfs_dnode_t *dn, uint64_t off,
	    void *buf, size_t len);

/* fetch the raw (first-slot) dnode of object objid in an objset */
int	zdmu_dnode_of(zfs_spa_t *spa, const zfs_dnode_t *metadn,
	    uint64_t objid, zfs_dnode_t *out);

/* fetch dnode of objid and require a non-empty bonus buffer */
int	zdmu_bonus_of(zfs_spa_t *spa, const zfs_dnode_t *metadn,
	    uint64_t objid, zfs_dnode_t *out);

#endif /* ZFS_DMU_H */
