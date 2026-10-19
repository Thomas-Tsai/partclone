/**
 * zfs_compress.h - clean-room block decompression.
 *
 * Only the algorithms reachable through MOS metadata on a default pool
 * are implemented: OFF and LZ4 (metadata default), plus GZIP via zlib.
 * Others return an explicit error so the caller can fail loudly.
 *
 * LZ4 via system liblz4 (BSD-2-Clause), GZIP via zlib (zlib license).
 *
 * Format derived from the MIT-licensed specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/),
 * glossary (compression algorithms). No OpenZFS (CDDL) source is used
 * or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_COMPRESS_H
#define ZFS_COMPRESS_H

#include <stddef.h>

/* 0 on success (dst filled with dstlen bytes); -1 on error */
int	zfs_decompress(int comp, const void *src, size_t srclen,
	    void *dst, size_t dstlen);

#endif /* ZFS_COMPRESS_H */
