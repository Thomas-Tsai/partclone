/**
 * zfs_zap.h - clean-room ZAP (ZFS Attribute Processor) reader.
 *
 * Iteration-based: instead of computing the salted CRC64 hash for
 * lookups, every entry of the ZAP is walked (microzap array scan, or
 * fatzap leaf chunk linear scan). ZAPs reached here (MOS object
 * directory, per-vdev ZAPs, log-spacemap ZAP) hold at most a few
 * hundred entries, so a linear walk is cheap and entirely avoids
 * reimplementing the hash function.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_ZAP_H
#define ZFS_ZAP_H

#include <stdint.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"

/* Called for each ZAP entry.
 *
 *   name/name_len : raw name bytes. String ZAPs: NUL-terminated string
 *                   (name_len includes the NUL). uint64-key ZAPs:
 *                   8-byte big-endian key.
 *   value         : host-order value when the entry is a single
 *                   integer of 1/2/4/8 bytes (has_value=1); fatzap
 *                   integer arrays are stored big-endian on disk and
 *                   are converted here; microzap values are stored
 *                   in the pool's native order (little-endian here).
 * Return non-zero to stop iteration early (reported as 1). */
typedef int (*zzap_iter_cb)(const uint8_t *name, uint32_t name_len,
    uint64_t value, int has_value, void *arg);

/* iterate all entries. 0 = walked everything; 1 = stopped by callback;
 * -1 = error. */
int	zzap_iterate(zfs_spa_t *spa, const zfs_dnode_t *zap_dn,
	    zzap_iter_cb cb, void *arg);

/* convenience: lookup a u64 value by C string name. 0 on success. */
int	zzap_lookup_u64(zfs_spa_t *spa, const zfs_dnode_t *zap_dn,
	    const char *name, uint64_t *out);

#endif /* ZFS_ZAP_H */
