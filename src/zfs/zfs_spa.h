/**
 * zfs_spa.h - clean-room ZFS pool reader: device I/O, vdev label,
 * uberblock selection, and block (blkptr) reads with decompression.
 *
 * v1 scope: stripe pools, single top-level vdev ("disk" leaf).
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_SPA_H
#define ZFS_SPA_H

#include <stdint.h>
#include <stddef.h>

#include "zfs_disk.h"
#include "zfs_nvlist.h"

typedef struct zfs_vdev_label_info {
	znv_list_t	*nv;		/* parsed vdev_phys nvlist */
	uint64_t	txg;		/* label txg */
} zfs_vdev_label_info_t;

typedef struct zfs_spa {
	int		fd;
	uint64_t	psize;		/* partition size, bytes */
	char		pool_name[256];
	uint64_t	pool_guid;
	uint64_t	top_guid;
	uint64_t	version;
	uint32_t	ashift;

	/* single top-level (stripe) vdev */
	uint64_t	vdev_guid;
	uint64_t	metaslab_array;	/* MOS object id */
	uint32_t	metaslab_shift;
	uint64_t	asize;		/* allocatable bytes */
	uint64_t	ms_count;	/* asize >> metaslab_shift */

	/* active uberblock */
	uint8_t		ub[ZUB_SIZE];	/* raw uberblock bytes */
	uint64_t	ub_txg;
	int		ub_ashift;	/* ashift used for ub ring slots */

	/* MOS */
	zfs_bp_t	mos_bp;		/* copy of ub_rootbp */
	uint8_t		*mos_objset;	/* decompressed objset_phys_t */
	size_t		mos_objset_len;

	/* optional MOS anchors (0 when absent) */
	uint64_t	config_obj;		/* packed nvlist object */
	uint64_t	log_spacemap_zap;	/* fatzap object */
	uint64_t	vdev_top_zap;		/* from MOS config vdev_tree */
	uint64_t	unflushed_obj;		/* per-ms txg array object */
} zfs_spa_t;

/* open + fully resolve the pool metadata anchors. 0 on success. */
int	zspa_open(zfs_spa_t **spa_out, const char *device,
	    void (*logf)(const char *fmt, ...));
void	zspa_close(zfs_spa_t *spa);

/* raw read at partition-absolute offset */
int	zspa_pread(zfs_spa_t *spa, void *buf, size_t len, uint64_t off);

/* read + decompress a block pointer; returns malloc'd buffer */
int	zspa_read_bp(zfs_spa_t *spa, const zfs_bp_t *bp, uint8_t **out,
	    size_t *out_len);

#endif /* ZFS_SPA_H */
