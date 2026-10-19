/**
 * zfs_disk.h - ZFS on-disk format constants and accessors (clean-room)
 *
 * Layouts are derived from the MIT-licensed independent specification
 * "ZFS On-Disk Format" (https://mminkus.github.io/zfs-ondiskformat/),
 * chapters 1 (vdevs/labels/uberblocks), 2 (block pointers), 3 (DMU),
 * 5 (ZAP), 9 (space maps), and the glossary (enum values).
 *
 * No OpenZFS (CDDL) source is used or linked.
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_DISK_H
#define ZFS_DISK_H

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Generic little-endian load helpers                                  */
/* ------------------------------------------------------------------ */

static inline uint16_t zle16(const uint8_t *p)
{
	return ((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static inline uint32_t zle32(const uint8_t *p)
{
	return ((uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24));
}

static inline uint64_t zle64(const uint8_t *p)
{
	return ((uint64_t)zle32(p) | ((uint64_t)zle32(p + 4) << 32));
}

static inline uint32_t zbe32(const uint8_t *p)
{
	return (((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	    ((uint32_t)p[2] << 8) | (uint32_t)p[3]);
}

static inline uint64_t zbe64(const uint8_t *p)
{
	return (((uint64_t)zbe32(p) << 32) | zbe32(p + 4));
}

/* ------------------------------------------------------------------ */
/* SPA: vdev labels, boot block, uberblocks                            */
/* ------------------------------------------------------------------ */

#define ZFS_LABEL_SIZE		(256ULL * 1024)		/* one label */
#define ZFS_LABEL_PHYS_OFF	0x4000ULL		/* nvlist offset in label */
#define ZFS_LABEL_PHYS_SIZE	(112ULL * 1024)		/* nvlist area size */
#define ZFS_LABEL_UB_OFF	0x20000ULL		/* uberblock array offset */
#define ZFS_LABEL_UB_SIZE	(128ULL * 1024)		/* uberblock array size */
#define ZFS_LABEL_START_SIZE	(4ULL * 1024 * 1024)	/* two labels + boot block */
#define ZFS_LABEL_END_SIZE	(2ULL * ZFS_LABEL_SIZE)	/* L2 + L3 at device tail */

/* uberblock_t field offsets (within a slot) */
#define ZUB_MAGIC	0x000	/* u64: 0x00bab10c */
#define ZUB_VERSION	0x008	/* u64: 1-28 or 5000 (feature flags) */
#define ZUB_TXG		0x010
#define ZUB_GUID_SUM	0x018
#define ZUB_TIMESTAMP	0x020
#define ZUB_ROOTBP	0x028	/* 128-byte blkptr to MOS */
#define ZUB_MMP_MAGIC	0x0B0
#define ZUB_MMP_DELAY	0x0B8
#define ZUB_MMP_CONFIG	0x0C0
#define ZUB_SIZE	0x0D8	/* allocated buffer we keep */

#define ZUB_MAGIC_VALUE	0x00bab10cULL
#define ZUB_SPA_VERSION_FEATURES 5000ULL

/* uberblock slot sizing: slot_shift = MIN(MAX(ashift,10),13) */
static inline uint32_t zub_slot_shift(uint32_t ashift)
{
	if (ashift < 10)
		ashift = 10;
	if (ashift > 13)
		ashift = 13;
	return (ashift);
}

/* ------------------------------------------------------------------ */
/* blkptr_t: 128 bytes = 16 x 8-byte words (little-endian on x86)      */
/* ------------------------------------------------------------------ */

#define ZBP_SIZE	128

/* word 6 (blk_prop) bit layout:
 *   63    B (byteorder: 0=BE, 1=LE)
 *   62    D (dedup)
 *   61    X (encryption)
 *   60-56 lvl (5)
 *   55-48 type (8)
 *   47-40 cksum (8) / embedded type when E=1
 *   39    E (embedded)
 *   38-32 comp (7)
 *   31-16 PSIZE (16; sectors-1; embedded: bytes at 25-31 together with LSIZE)
 *   15-0  LSIZE (16; sectors-1)
 */
#define ZBP_PROP_B(x)		(((x) >> 63) & 0x1)
#define ZBP_PROP_DEDUP(x)	(((x) >> 62) & 0x1)
#define ZBP_PROP_ENCRYPT(x)	(((x) >> 61) & 0x1)
#define ZBP_PROP_LEVEL(x)	(((x) >> 56) & 0x1f)
#define ZBP_PROP_TYPE(x)	(((x) >> 48) & 0xff)
#define ZBP_PROP_CKSUM(x)	(((x) >> 40) & 0xff)
#define ZBP_PROP_EMBEDDED(x)	(((x) >> 39) & 0x1)
#define ZBP_PROP_COMP(x)	(((x) >> 32) & 0x7f)
#define ZBP_PROP_PSIZE(x)	(((x) >> 16) & 0xffff)
#define ZBP_PROP_LSIZE(x)	((x) & 0xffff)
/* embedded bp sizing: LSIZE = bits 0-24 (bytes), PSIZE = bits 25-31 (bytes) */
/* embedded bp sizing: LSIZE = bits 0-24, PSIZE = bits 25-31, both in
 * bytes and both stored **minus one** (the same +1 bias the standard
 * sector fields use; verified on disk: 0x1ff -> 512-byte lsize,
 * 56 -> 57 = 4-byte prefix + 53-byte LZ4 payload). */
#define ZBP_EMB_LSIZE(x)	(((x) & 0x1ffffffULL) + 1)
#define ZBP_EMB_PSIZE(x)	((((x) >> 25) & 0x7f) + 1)

typedef struct zfs_dva {
	uint64_t	vdev;		/* 24-bit vdev id */
	uint64_t	offset;		/* 63-bit sector offset (from data
					   area start, i.e. +4MiB on disk) */
	uint64_t	asize;		/* allocated size, bytes */
	int		gang;		/* gang block indicator */
} zfs_dva_t;

typedef struct zfs_bp {
	uint8_t		raw[ZBP_SIZE];	/* authoritative on-disk copy */
} zfs_bp_t;

/* DVA word pair d (d=0,1,2): word 2d and 2d+1 */
static inline void zbp_get_dva(const zfs_bp_t *bp, int d, zfs_dva_t *out)
{
	uint64_t w0 = zle64(bp->raw + 16 * d);
	uint64_t w1 = zle64(bp->raw + 16 * d + 8);
	out->asize  = (w0 & 0xffffffULL) << 9;		/* 24-bit sectors */
	out->vdev   = (w0 >> 48) & 0xffffffULL;
	out->gang   = (int)(w1 >> 63);
	out->offset = (w1 & 0x7fffffffffffffffULL) << 9; /* 63-bit sectors */
}

static inline uint64_t zbp_prop(const zfs_bp_t *bp)
{
	return (zle64(bp->raw + 6 * 8));
}

static inline int	zbp_level(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_LEVEL(zbp_prop(bp))); }
static inline int	zbp_type(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_TYPE(zbp_prop(bp))); }
static inline int	zbp_comp(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_COMP(zbp_prop(bp))); }
static inline int	zbp_is_embedded(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_EMBEDDED(zbp_prop(bp))); }
static inline int	zbp_is_encrypted(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_ENCRYPT(zbp_prop(bp))); }
static inline int	zbp_byteorder(const zfs_bp_t *bp)
			{ return ((int)ZBP_PROP_B(zbp_prop(bp))); }

static inline uint64_t zbp_lsize(const zfs_bp_t *bp)
{
	uint64_t p = zbp_prop(bp);
	if (ZBP_PROP_EMBEDDED(p))
		return (ZBP_EMB_LSIZE(p));
	return ((ZBP_PROP_LSIZE(p) + 1) << 9);
}

static inline uint64_t zbp_psize(const zfs_bp_t *bp)
{
	uint64_t p = zbp_prop(bp);
	if (ZBP_PROP_EMBEDDED(p))
		return (ZBP_EMB_PSIZE(p));
	return ((ZBP_PROP_PSIZE(p) + 1) << 9);
}

/* hole: not embedded and dva[0] entirely zero */
static inline int zbp_is_hole(const zfs_bp_t *bp)
{
	if (zbp_is_embedded(bp))
		return (0);
	return (zle64(bp->raw) == 0 && zle64(bp->raw + 8) == 0);
}

static inline int zbp_is_gang(const zfs_bp_t *bp)
{
	zfs_dva_t d;
	if (zbp_is_hole(bp) || zbp_is_embedded(bp))
		return (0);
	zbp_get_dva(bp, 0, &d);
	return (d.gang);
}

/* embedded bp payload: 112 bytes in words 0-5,7-9,11-15 (skip 6 and 10) */
#define ZBP_EMB_PAYLOAD_SIZE 112
static inline void zbp_embedded_payload(const zfs_bp_t *bp, uint8_t *dst)
{
	static const int words[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 12, 13,
	    14, 15 };
	for (unsigned i = 0; i < sizeof (words) / sizeof (words[0]); i++)
		memcpy(dst + i * 8, bp->raw + words[i] * 8, 8);
}

/* compression algorithms (glossary) */
typedef enum {
	ZC_INHERIT	= 0,
	ZC_ON		= 1,
	ZC_OFF		= 2,
	ZC_LZJB		= 3,
	ZC_EMPTY	= 4,
	ZC_GZIP_1	= 5,
	/* ... 6..12 */
	ZC_GZIP_9	= 13,
	ZC_ZLE		= 14,
	ZC_LZ4		= 15,
	ZC_ZSTD		= 16,
} zfs_comp_t;

/* DMU object types (glossary), subset */
typedef enum {
	ZOT_NONE		= 0,
	ZOT_OBJECT_DIRECTORY	= 1,
	ZOT_OBJECT_ARRAY	= 2,
	ZOT_PACKED_NVLIST	= 3,
	ZOT_PACKED_NVLIST_SIZE	= 4,
	ZOT_SPACE_MAP_HEADER	= 7,
	ZOT_SPACE_MAP		= 8,
	ZOT_DNODE		= 10,
	ZOT_OBJSET		= 11,
	ZOT_DSL_DIR		= 12,
	ZOT_ZAP_OTHER		= 27,
} zfs_obj_type_t;

#define ZOT_NEWTYPE	0x80	/* dn_type bit 7: extensible encoding */

/* object set types */
typedef enum {
	ZOST_NONE	= 0,
	ZOST_META	= 1,
	ZOST_ZFS	= 2,
	ZOST_ZVOL	= 3,
} zfs_objset_type_t;

/* ------------------------------------------------------------------ */
/* dnode_phys_t: 512-byte slots (DNODE_SHIFT = 9)                      */
/* ------------------------------------------------------------------ */

#define ZDN_SIZE	512
#define ZDN_SHIFT	9

/* field offsets (doc 3.1) */
#define ZDN_TYPE	0x00	/* u8 */
#define ZDN_INDBLKSHIFT	0x01	/* u8 log2 indirect block size */
#define ZDN_NLEVELS	0x02	/* u8 */
#define ZDN_NBLKPTR	0x03	/* u8 */
#define ZDN_BONUSTYPE	0x04	/* u8 */
#define ZDN_CHECKSUM	0x05	/* u8 */
#define ZDN_COMPRESS	0x06	/* u8 */
#define ZDN_FLAGS	0x07	/* u8 */
#define ZDN_DATABLKSZSEC 0x08	/* u16 */
#define ZDN_BONUSLEN	0x0A	/* u16 */
#define ZDN_EXTRA_SLOTS	0x0C	/* u8 */
#define ZDN_MAXBLKID	0x10	/* u64 */
#define ZDN_USED	0x18	/* u64 */
#define ZDN_BLKPTR	0x40	/* blkptr[0..nblkptr) */
#define ZDN_CORE_SIZE	0x40

#define ZDN_FLAG_USED_BYTES	0x01
#define ZDN_FLAG_SPILL_BLKPTR	0x04

typedef struct zfs_dnode {
	uint8_t		raw[ZDN_SIZE];	/* first slot; extra slots unread */
} zfs_dnode_t;

static inline int	zdn_type(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_TYPE]); }
static inline int	zdn_indblkshift(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_INDBLKSHIFT]); }
static inline int	zdn_nlevels(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_NLEVELS]); }
static inline int	zdn_nblkptr(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_NBLKPTR]); }
static inline int	zdn_bonustype(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_BONUSTYPE]); }
static inline int	zdn_flags(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_FLAGS]); }
static inline uint64_t zdn_datablksz(const zfs_dnode_t *dn)
			{ return ((uint64_t)zle16(dn->raw + ZDN_DATABLKSZSEC)
			    << 9); }
static inline uint32_t zdn_bonuslen(const zfs_dnode_t *dn)
			{ return (zle16(dn->raw + ZDN_BONUSLEN)); }
static inline int	zdn_extra_slots(const zfs_dnode_t *dn)
			{ return (dn->raw[ZDN_EXTRA_SLOTS]); }

/* bonus buffer starts after core + nblkptr block pointers */
static inline const uint8_t *zdn_bonus(const zfs_dnode_t *dn)
{
	return (dn->raw + ZDN_CORE_SIZE + zdn_nblkptr(dn) * ZBP_SIZE);
}

static inline void zdn_get_bp(const zfs_dnode_t *dn, int i, zfs_bp_t *out)
{
	memcpy(out->raw, dn->raw + ZDN_BLKPTR + i * ZBP_SIZE, ZBP_SIZE);
}

/* ------------------------------------------------------------------ */
/* objset_phys_t: metadnode at offset 0 (first 512 bytes)              */
/* ------------------------------------------------------------------ */

#define ZOS_METADNODE_OFF 0x000

/* ------------------------------------------------------------------ */
/* ZAP (doc chapter 5)                                                 */
/* ------------------------------------------------------------------ */

#define ZZAP_BT_MICRO	((1ULL << 63) + 3)
#define ZZAP_BT_HEADER	((1ULL << 63) + 1)
#define ZZAP_BT_LEAF	((1ULL << 63) + 0)
#define ZZAP_MAGIC	0x2F52AB2ABULL
#define ZZAP_LEAF_MAGIC	0x2AB1EAFU

/* microzap entry: 64 bytes */
#define ZMZAP_HDR_SIZE	64
#define ZMZAP_ENT_SIZE	64
#define ZMZE_VALUE	0x00	/* u64 */
#define ZMZE_CD		0x08	/* u32 */
#define ZMZE_NAME	0x0E	/* char[50] */
#define ZMZE_NAME_LEN	50

/* fat zap header (block 0), little-endian fields */
#define ZFZ_BLOCK_TYPE	0x00	/* u64 = ZZAP_BT_HEADER */
#define ZFZ_MAGIC	0x08	/* u64 */
#define ZFZ_PTRTBL_BLK	0x10	/* u64 */
#define ZFZ_PTRTBL_NUMBLKS 0x18
#define ZFZ_PTRTBL_SHIFT 0x20
#define ZFZ_FREEBLK	0x38	/* u64: next allocatable block id */
#define ZFZ_NUM_LEAFS	0x40
#define ZFZ_NUM_ENTRIES	0x48
#define ZFZ_SALT	0x50	/* u64 */
#define ZFZ_FLAGS	0x60	/* u64 */

#define ZFZ_FLAG_UINT64_KEY 0x01	/* names are uint64 (stored BE) */

/* fat zap leaf header */
#define ZFL_BLOCK_TYPE	0x00
#define ZFL_PREFIX	0x10	/* u64 */
#define ZFL_MAGIC	0x18	/* u32 */
#define ZFL_NFREE	0x1C	/* u16 */
#define ZFL_NENTRIES	0x1E	/* u16 */
#define ZFL_FREELIST	0x22	/* u16 */
#define ZFL_HDR_SIZE	48
/* hash table: blocksize/32 x u16 at offset 48; chunks follow */
#define ZFL_CHUNK_SIZE	24

/* chunk types */
#define ZCHUNK_ARRAY	251
#define ZCHUNK_ENTRY	252
#define ZCHUNK_FREE	253

/* entry chunk fields */
#define ZLE_TYPE		0x00	/* u8 = 252 */
#define ZLE_VALUE_INTLEN	0x01	/* u8 */
#define ZLE_NEXT		0x02	/* u16 */
#define ZLE_NAME_CHUNK		0x04	/* u16 */
#define ZLE_NAME_NUMINTS	0x06	/* u16 */
#define ZLE_VALUE_CHUNK		0x08	/* u16 */
#define ZLE_VALUE_NUMINTS	0x0A	/* u16 */
#define ZLE_CD			0x0C	/* u32 */
#define ZLE_HASH		0x10	/* u64 */

/* array chunk fields */
#define ZLA_TYPE	0x00		/* u8 = 251 */
#define ZLA_ARRAY	0x01		/* u8[21] */
#define ZLA_NEXT	0x16		/* u16 */
#define ZLA_ARRAY_LEN	21

/* ------------------------------------------------------------------ */
/* Space maps (doc chapter 9)                                          */
/* ------------------------------------------------------------------ */

/* space_map_phys_t bonus (dnode bonus, type ZOT_SPACE_MAP_HEADER) */
#define ZSMP_OBJECT	0x00	/* u64 (deprecated) */
#define ZSMP_LENGTH	0x08	/* u64: log length in bytes (authoritative) */
#define ZSMP_ALLOC	0x10	/* i64: net allocated bytes */
#define ZSMP_HISTOGRAM	0x40	/* 32 x u64 */

/* entry encoding */
#define ZSM_DEBUG_PREFIX(w)	(((w) >> 62) == 0x2)	/* prefix 10b */
#define ZSM_TWO_WORD_PREFIX(w)	(((w) >> 62) == 0x3)	/* prefix 11b */

/* one-word entry: bit63=0; off 62-16 (sm_shift units); type bit15;
 * run bits14-0 stored as run-1 */
#define ZSM1_OFFSET(w)		(((w) >> 16) & 0x7fffffffffffULL)
#define ZSM1_TYPE(w)		(((w) >> 15) & 0x1)	/* 0=alloc 1=free */
#define ZSM1_RUN(w)		(((w) & 0x7fff) + 1)

/* two-word entry word0: prefix 11b; run 59-24 (units, stored run-1);
 * vdev 23-0 (24 bits). word1: type bit63; offset 62-0 (units) */
#define ZSM2_RUN(w0)		((((w0) >> 24) & 0xfffffffffULL) + 1)
#define ZSM2_VDEV(w0)		((w0) & 0xffffffULL)
#define ZSM2_TYPE(w1)		(((w1) >> 63) & 0x1)
#define ZSM2_OFFSET(w1)		((w1) & 0x7fffffffffffffffULL)

#define ZSM_TYPE_ALLOC	0
#define ZSM_TYPE_FREE	1

/* log spacemap shift is always 9 (SPA_MINBLOCKSHIFT) */
#define ZSM_LOG_SHIFT	9

/* MOS well-known objects/names */
#define ZMOS_OBJECT_DIRECTORY	1

#define ZPOOL_CFG_VDEV_TREE	"vdev_tree"
#define ZPOOL_CFG_METASLAB_ARRAY "metaslab_array"
#define ZPOOL_CFG_METASLAB_SHIFT "metaslab_shift"
#define ZPOOL_CFG_ASHIFT	"ashift"
#define ZPOOL_CFG_ASIZE		"asize"
#define ZPOOL_CFG_TYPE		"type"
#define ZPOOL_CFG_CHILDREN	"children"
#define ZPOOL_CFG_IS_LOG	"is_log"
#define ZPOOL_CFG_POOL_NAME	"name"
#define ZPOOL_CFG_POOL_GUID	"pool_guid"
#define ZPOOL_CFG_TOP_GUID	"top_guid"
#define ZPOOL_CFG_GUID		"guid"
#define ZPOOL_CFG_TXG		"txg"
#define ZPOOL_CFG_VERSION	"version"

#define ZVDEV_TYPE_ROOT		"root"
#define ZVDEV_TYPE_DISK		"disk"
#define ZVDEV_TYPE_FILE		"file"

#define ZMOS_CFG_OBJECT		"config"
#define ZMOS_LOG_SPACEMAP_ZAP	"com.delphix:log_spacemap_zap"
#define ZVDEV_TOP_ZAP_KEY	"com.delphix:vdev_zap_top"
#define ZVDEV_TOP_ZAP_UNFLUSHED	"com.delphix:ms_unflushed_phys_txgs"

#endif /* ZFS_DISK_H */
