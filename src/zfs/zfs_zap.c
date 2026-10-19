/**
 * zfs_zap.c - clean-room ZAP reader (microzap + fatzap).
 *
 * Format derived from the MIT-licensed specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/),
 * chapter 5 (ZAP). No OpenZFS (CDDL) source is used or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <stdlib.h>
#include <string.h>

#include "zfs_disk.h"
#include "zfs_spa.h"
#include "zfs_dmu.h"
#include "zfs_zap.h"

/* ------------------------------------------------------------------ */
/* microzap: flat array of 64-byte entries after a 64-byte header      */
/* ------------------------------------------------------------------ */

static int micro_iterate(const uint8_t *blk, size_t blen, zzap_iter_cb cb,
    void *arg)
{
	for (size_t off = ZMZAP_HDR_SIZE; off + ZMZAP_ENT_SIZE <= blen;
	    off += ZMZAP_ENT_SIZE) {
		const uint8_t *e = blk + off;
		uint64_t value = zle64(e + ZMZE_VALUE);	/* native order */
		const char *name = (const char *)e + ZMZE_NAME;

		if (name[0] == '\0')
			continue;
		if (cb((const uint8_t *)name,
		    (uint32_t)strnlen(name, ZMZE_NAME_LEN) + 1, value, 1,
		    arg) != 0)
			return (1);
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* fatzap: linear scan of leaf chunk arrays                            */
/* ------------------------------------------------------------------ */

/* decode one big-endian integer of 1/2/4/8 bytes */
static uint64_t be_int(const uint8_t *p, int intlen)
{
	uint64_t v = 0;

	for (int i = 0; i < intlen; i++)
		v = (v << 8) | p[i];
	return (v);
}

/* extract a chained array chunk payload */
static int leaf_read_array(const uint8_t *chunks, size_t nchunks,
    uint16_t first, uint32_t total, uint8_t *out)
{
	uint32_t copied = 0;
	uint32_t idx = first;
	uint32_t guard = 0;

	while (copied < total) {
		const uint8_t *c;
		uint32_t n;

		if (idx >= nchunks || idx == 0xffff || guard++ > nchunks)
			return (-1);
		c = chunks + (size_t)idx * ZFL_CHUNK_SIZE;
		if (c[ZLA_TYPE] != ZCHUNK_ARRAY)
			return (-1);
		n = total - copied;
		if (n > ZLA_ARRAY_LEN)
			n = ZLA_ARRAY_LEN;
		memcpy(out + copied, c + ZLA_ARRAY, n);
		copied += n;
		idx = zle16(c + ZLA_NEXT);
	}
	return (0);
}

static int fat_leaf_iterate(const uint8_t *blk, size_t blen,
    int uint64_keys, zzap_iter_cb cb, void *arg)
{
	size_t n_hash = blen / 32;
	size_t chunks_off = ZFL_HDR_SIZE + n_hash * 2;
	size_t nchunks;
	const uint8_t *chunks;

	if (blen < chunks_off)
		return (-1);
	nchunks = (blen - chunks_off) / ZFL_CHUNK_SIZE;
	chunks = blk + chunks_off;

	for (size_t i = 0; i < nchunks; i++) {
		const uint8_t *c = chunks + i * ZFL_CHUNK_SIZE;
		uint16_t name_chunk, name_ints, val_chunk, val_ints;
		uint8_t intlen;
		uint8_t namebuf[1024 + 8];
		uint8_t valbuf[8];
		uint32_t name_len, val_len;
		uint64_t value = 0;
		int has_value = 0;

		if (c[ZLE_TYPE] != ZCHUNK_ENTRY)
			continue;
		intlen     = c[ZLE_VALUE_INTLEN];
		name_chunk = zle16(c + ZLE_NAME_CHUNK);
		name_ints  = zle16(c + ZLE_NAME_NUMINTS);
		val_chunk  = zle16(c + ZLE_VALUE_CHUNK);
		val_ints   = zle16(c + ZLE_VALUE_NUMINTS);

		/* name byte count: string ZAPs are intlen-1 names; a
		 * ZAP_FLAG_UINT64_KEY ZAP stores one big-endian u64 per
		 * name integer */
		name_len = uint64_keys ? (uint32_t)name_ints * 8 : name_ints;
		if (name_len == 0 || name_len > sizeof (namebuf))
			continue;
		if (leaf_read_array(chunks, nchunks, name_chunk, name_len,
		    namebuf) != 0)
			continue;

		/* values: only single integers are needed by callers */
		val_len = (uint32_t)val_ints * intlen;
		if (val_ints == 1 &&
		    (intlen == 1 || intlen == 2 || intlen == 4 ||
		    intlen == 8) && val_len <= sizeof (valbuf)) {
			if (leaf_read_array(chunks, nchunks, val_chunk,
			    val_len, valbuf) != 0)
				continue;
			value = be_int(valbuf, intlen);
			has_value = 1;
		}

		if (cb(namebuf, name_len, value, has_value, arg) != 0)
			return (1);
	}
	return (0);
}

int
zzap_iterate(zfs_spa_t *spa, const zfs_dnode_t *zap_dn, zzap_iter_cb cb,
    void *arg)
{
	uint64_t data_bs = zdn_datablksz(zap_dn);
	uint8_t *blk = NULL;
	size_t blen;
	uint64_t btype;
	int rc = -1;

	if (data_bs == 0 || data_bs > (1U << 24))
		return (-1);
	blen = (size_t)data_bs;
	blk = malloc(blen);
	if (blk == NULL)
		return (-1);
	if (zdmu_read(spa, zap_dn, 0, blk, blen) != 0)
		goto out;

	btype = zle64(blk);
	if (btype == ZZAP_BT_MICRO) {
		rc = micro_iterate(blk, blen, cb, arg);
		goto out;
	}
	if (btype != ZZAP_BT_HEADER || zle64(blk + ZFZ_MAGIC) != ZZAP_MAGIC)
		goto out;

	/* fatzap: walk blocks [1, zap_freeblk); keep those with a valid
	 * leaf header, linear-scan their chunk arrays */
	{
		uint64_t freeblk = zle64(blk + ZFZ_FREEBLK);
		int uint64_keys =
		    (zle64(blk + ZFZ_FLAGS) & ZFZ_FLAG_UINT64_KEY) != 0;

		if (freeblk > (1ULL << 20))			/* sanity */
			goto out;
		rc = 0;
		for (uint64_t id = 1; id < freeblk; id++) {
			if (zdmu_read(spa, zap_dn, id * data_bs, blk,
			    blen) != 0)
				continue;		/* hole/pad block */
			if (zle64(blk + ZFL_BLOCK_TYPE) != ZZAP_BT_LEAF)
				continue;
			if (zle32(blk + ZFL_MAGIC) != ZZAP_LEAF_MAGIC)
				continue;
			rc = fat_leaf_iterate(blk, blen, uint64_keys, cb,
			    arg);
			if (rc != 0)
				break;			/* 1 stop, -1 err */
		}
	}
out:
	free(blk);
	return (rc);
}

/* ------------------------------------------------------------------ */
/* u64 lookup helper                                                    */
/* ------------------------------------------------------------------ */

typedef struct {
	const char	*want;
	uint64_t	value;
	int		found;
} zap_lookup_ctx_t;

static int lookup_cb(const uint8_t *name, uint32_t name_len,
    uint64_t value, int has_value, void *arg)
{
	zap_lookup_ctx_t *ctx = arg;
	size_t want_len = strlen(ctx->want) + 1;

	if (name_len != want_len || memcmp(name, ctx->want, want_len) != 0)
		return (0);
	if (!has_value)
		return (0);
	ctx->value = value;
	ctx->found = 1;
	return (1);
}

int
zzap_lookup_u64(zfs_spa_t *spa, const zfs_dnode_t *zap_dn, const char *name,
    uint64_t *out)
{
	zap_lookup_ctx_t ctx = { .want = name, .value = 0, .found = 0 };
	int rc;

	rc = zzap_iterate(spa, zap_dn, lookup_cb, &ctx);
	if (rc < 0 || !ctx.found)
		return (-1);
	*out = ctx.value;
	return (0);
}
