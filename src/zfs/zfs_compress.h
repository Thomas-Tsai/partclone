/**
 * zfs_compress.h - clean-room block decompression.
 *
 * Only the algorithms reachable through MOS metadata on a default pool
 * are implemented: OFF and LZ4 (metadata default), plus GZIP via zlib.
 * Others return an explicit error so the caller can fail loudly.
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
