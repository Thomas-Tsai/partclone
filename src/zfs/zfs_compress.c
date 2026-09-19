/**
 * zfs_compress.c - clean-room block decompression.
 *
 * LZ4 via system liblz4 (BSD-2-Clause), GZIP via zlib (zlib license).
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <string.h>
#include <lz4.h>
#include <zlib.h>

#include "zfs_disk.h"
#include "zfs_compress.h"

static int gunzip_raw(const void *src, size_t srclen, void *dst, size_t dstlen)
{
	z_stream zs;
	int ret;

	memset(&zs, 0, sizeof (zs));
	zs.next_in = (Bytef *)src;
	zs.avail_in = (uInt)srclen;
	zs.next_out = dst;
	zs.avail_out = (uInt)dstlen;

	/* ZFS stores raw deflate streams (negative window bits) */
	if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
		return (-1);
	ret = inflate(&zs, Z_FINISH);
	inflateEnd(&zs);
	if (ret != Z_STREAM_END && zs.total_out != dstlen)
		return (-1);
	return (0);
}

/* On-disk LZ4 framing (verified empirically): 4-byte big-endian
 * compressed payload length, then the raw LZ4 block stream. */
static int lz4_zfs(const void *src, size_t srclen, void *dst, size_t dstlen)
{
	const uint8_t *s = src;
	uint32_t payload_len;

	if (srclen < 4)
		return (-1);
	payload_len = ((uint32_t)s[0] << 24) | ((uint32_t)s[1] << 16) |
	    ((uint32_t)s[2] << 8) | s[3];
	if (payload_len + 4 > srclen)
		return (-1);
	if (LZ4_decompress_safe((const char *)s + 4, dst,
	    (int)payload_len, (int)dstlen) != (int)dstlen)
		return (-1);
	return (0);
}

int
zfs_decompress(int comp, const void *src, size_t srclen, void *dst,
    size_t dstlen)
{
	switch (comp) {
	case ZC_OFF:
		if (srclen < dstlen)
			return (-1);
		memcpy(dst, src, dstlen);
		return (0);
	case ZC_LZ4:
		return (lz4_zfs(src, srclen, dst, dstlen));
	default:
		if (comp >= ZC_GZIP_1 && comp <= ZC_GZIP_9)
			return (gunzip_raw(src, srclen, dst, dstlen));
		return (-1);	/* lzjb/zle/zstd: not yet supported */
	}
}
