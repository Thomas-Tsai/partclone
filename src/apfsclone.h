/**
 * reiser4clone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read reiser4 super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <stdio.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <errno.h> 
#include <inttypes.h> 
#include <unistd.h> 

#include <byteswap.h>
#include <endian.h>
// Swap to/from little endian.
//inline int16_t bswap_le(const int16_t x) { return x; }
//inline int32_t bswap_le(const int32_t x) { return x; }
//inline int64_t bswap_le(const int64_t x) { return x; }
//inline uint16_t bswap_le(const uint16_t x) { return x; }
//inline uint32_t bswap_le(const uint32_t x) { return x; }
//inline uint64_t bswap_le(const uint64_t x) { return x; }
// Swap to/from big endian.
//inline int16_t bswap_be(const int16_t x) { return bswap_16(x); }
//inline int32_t bswap_be(const int32_t x) { return bswap_32(x); }
//inline int64_t bswap_be(const int64_t x) { return bswap_64(x); }
//inline uint16_t bswap_be(const uint16_t x) { return bswap_16(x); }
//inline uint32_t bswap_be(const uint32_t x) { return bswap_32(x); }
//inline uint64_t bswap_be(const uint64_t x) { return bswap_64(x); }

typedef uint8_t le_uint8_t;
typedef uint16_t le_uint16_t;
typedef uint32_t le_uint32_t;
typedef uint64_t le_uint64_t;
typedef int8_t le_int8_t;
typedef int16_t le_int16_t;
typedef int32_t le_int32_t;
typedef int64_t le_int64_t;
typedef uint8_t be_uint8_t;

typedef unsigned char apfs_uuid_t[16];
typedef uint64_t paddr_t; // Apple: int64_t
typedef uint64_t oid_t;
typedef uint64_t xid_t;

typedef le_uint64_t le_paddr_t;
typedef le_uint64_t le_oid_t;
typedef le_uint64_t le_xid_t;

struct prange_t {
	le_paddr_t pr_start_addr;
	le_uint64_t pr_block_count;
};
typedef struct prange_t prange_t;
typedef uint32_t crypto_flags_t;
typedef uint32_t cp_key_class_t;
typedef uint32_t cp_key_os_version_t;
typedef uint16_t cp_key_revision_t;
typedef struct cpx* cpx_t;

typedef le_uint32_t le_crypto_flags_t;
typedef le_uint32_t le_cp_key_class_t;
typedef le_uint32_t le_cp_key_os_version_t;
typedef le_uint16_t le_cp_key_revision_t;

struct wrapped_meta_crypto_state_t {
	le_uint16_t major_version;
	le_uint16_t minor_version;
	le_crypto_flags_t cpflags;
	le_cp_key_class_t persistent_class;
	le_cp_key_os_version_t key_os_version;
	le_cp_key_revision_t key_revision;
	le_uint16_t unused;
};


#define MAX_CKSUM_SIZE 8

struct obj_phys_t {
	uint8_t o_cksum[MAX_CKSUM_SIZE];
	le_oid_t o_oid;
	le_xid_t o_xid;
	le_uint32_t o_type;
	le_uint32_t o_subtype;
};
typedef struct obj_phys_t obj_phys_t;

const uint64_t OID_NX_SUPERBLOCK = 1;

const uint64_t OID_INVALID = 0;
const uint64_t OID_RESERVED_COUNT = 1024;

const uint32_t OBJECT_TYPE_MASK = 0x0000FFFF;
const uint32_t OBJECT_TYPE_FLAGS_MASK = 0xFFFF0000;

const uint32_t OBJECT_TYPE_FLAGS_DEFINED_MASK = 0xF8000000;

const uint32_t OBJECT_TYPE_NX_SUPERBLOCK = 0x00000001;

const uint32_t OBJECT_TYPE_BTREE = 0x00000002;
const uint32_t OBJECT_TYPE_BTREE_NODE = 0x00000003;

const uint32_t OBJECT_TYPE_SPACEMAN = 0x00000005;
const uint32_t OBJECT_TYPE_SPACEMAN_CAB = 0x00000006;
const uint32_t OBJECT_TYPE_SPACEMAN_CIB = 0x00000007;
const uint32_t OBJECT_TYPE_SPACEMAN_BITMAP = 0x00000008;
const uint32_t OBJECT_TYPE_SPACEMAN_FREE_QUEUE = 0x00000009;

const uint32_t OBJECT_TYPE_EXTENT_LIST_TREE = 0x0000000A;
const uint32_t OBJECT_TYPE_OMAP = 0x0000000B;
const uint32_t OBJECT_TYPE_CHECKPOINT_MAP = 0x0000000C;
const uint32_t OBJECT_TYPE_FS = 0x0000000D;
const uint32_t OBJECT_TYPE_FSTREE = 0x0000000E;
const uint32_t OBJECT_TYPE_BLOCKREFTREE = 0x0000000F;
const uint32_t OBJECT_TYPE_SNAPMETATREE = 0x00000010;

const uint32_t OBJECT_TYPE_NX_REAPER = 0x00000011;
const uint32_t OBJECT_TYPE_NX_REAP_LIST = 0x00000012;
const uint32_t OBJECT_TYPE_OMAP_SNAPSHOT = 0x00000013;
const uint32_t OBJECT_TYPE_EFI_JUMPSTART = 0x00000014;
const uint32_t OBJECT_TYPE_FUSION_MIDDLE_TREE = 0x00000015;
const uint32_t OBJECT_TYPE_NX_FUSION_WBC = 0x00000016;
const uint32_t OBJECT_TYPE_NX_FUSION_WBC_LIST = 0x00000017;
const uint32_t OBJECT_TYPE_ER_STATE = 0x00000018;

const uint32_t OBJECT_TYPE_GBITMAP = 0x00000019;
const uint32_t OBJECT_TYPE_GBITMAP_TREE = 0x0000001A;
const uint32_t OBJECT_TYPE_GBITMAP_BLOCK = 0x0000001B;

const uint32_t OBJECT_TYPE_ER_RECOVERY_BLOCK = 0x0000001C;
const uint32_t OBJECT_TYPE_SNAP_META_EXT = 0x0000001D;
const uint32_t OBJECT_TYPE_INTEGRITY_META = 0x0000001E;
const uint32_t OBJECT_TYPE_FEXT_TREE = 0x0000001F;
const uint32_t OBJECT_TYPE_RESERVED_20 = 0x00000020;

const uint32_t OBJECT_TYPE_INVALID = 0;
const uint32_t OBJECT_TYPE_TEST = 0x000000FF;

const uint32_t OBJECT_TYPE_CONTAINER_KEYBAG = 0x7379656B; // 'keys'
const uint32_t OBJECT_TYPE_VOLUME_KEYBAG = 0x73636572; // 'recs'
const uint32_t OBJECT_TYPE_MEDIA_KEYBAG = 0x79656B6D; // 'mkey'

const uint32_t OBJ_STORAGETYPE_MASK = 0xC0000000;

const uint32_t OBJ_VIRTUAL = 0x00000000;
const uint32_t OBJ_PHYSICAL = 0x40000000;
const uint32_t OBJ_EPHEMERAL = 0x80000000;

const uint32_t OBJ_NOHEADER = 0x20000000;
const uint32_t OBJ_ENCRYPTED = 0x10000000;
const uint32_t OBJ_NONPERSISTENT = 0x08000000;


const uint32_t NX_EFI_JUMPSTART_MAGIC = 0x5244534A; // RDSJ
const uint32_t NX_EFI_JUMPSTART_VERSION = 1;

struct nx_efi_jumpstart_t {
	obj_phys_t nej_o;
	le_uint32_t nej_magic;
	le_uint32_t nej_version;
	le_uint32_t nej_efi_file_len;
	le_uint32_t nej_num_extents;
	uint64_t nej_reserved[16];
	prange_t nej_rec_extents[];
};


const uint32_t NX_MAGIC = 0x4253584E; // BSXN
#define NX_MAX_FILE_SYSTEMS 100

#define NX_EPH_INFO_COUNT 4
const int NX_EPH_MIN_BLOCK_COUNT = 8;
const int NX_MAX_FILE_SYSTEM_EPH_STRUCTS = 4;
const int NX_TX_MIN_CHECKPOINT_COUNT = 4;
const int NX_EPH_INFO_VERSION_1 = 1;

const uint64_t NX_RESERVED_1 = 1;
const uint64_t NX_RESERVED_2 = 2;
const uint64_t NX_CRYPTO_SW = 4;

const uint64_t NX_FEATURE_DEFRAG = 1;
const uint64_t NX_FEATURE_LCFD = 2;
const uint64_t NX_SUPPORTED_FEATURES_MASK = NX_FEATURE_DEFRAG | NX_FEATURE_LCFD;

const uint64_t NX_SUPPORTED_ROCOMPAT_MASK = 0;

const uint64_t NX_INCOMPAT_VERSION1 = 1;
const uint64_t NX_INCOMPAT_VERSION2 = 2;
const uint64_t NX_INCOMPAT_FUSION = 0x100;
const uint64_t NX_SUPPORTED_INCOMPAT_MASK = NX_INCOMPAT_VERSION2 | NX_INCOMPAT_FUSION;

const int NX_MINIMUM_BLOCK_SIZE = 4096;
const int NX_DEFAULT_BLOCK_SIZE = 4096;
const int NX_MAXIMUM_BLOCK_SIZE = 65536;

const uint64_t NX_MINIMUM_CONTAINER_SIZE = 1048576;

enum nx_counter_id_t {
	NX_CNTR_OBJ_CKSUM_SET = 0,
	NX_CNTR_OBJ_CKSUM_FAIL = 1,

	NX_NUM_COUNTERS = 32
};

struct nx_superblock_t {
	obj_phys_t nx_o;
	le_uint32_t nx_magic;
	le_uint32_t nx_block_size;
	le_uint64_t nx_block_count;

	le_uint64_t nx_features;
	le_uint64_t nx_readonly_compatible_features;
	le_uint64_t nx_incompatible_features;

	apfs_uuid_t nx_uuid;

	le_oid_t nx_next_oid;
	le_xid_t nx_next_xid;

	le_uint32_t nx_xp_desc_blocks;
	le_uint32_t nx_xp_data_blocks;
	le_paddr_t nx_xp_desc_base;
	le_paddr_t nx_xp_data_base;
	le_uint32_t nx_xp_desc_next;
	le_uint32_t nx_xp_data_next;
	le_uint32_t nx_xp_desc_index;
	le_uint32_t nx_xp_desc_len;
	le_uint32_t nx_xp_data_index;
	le_uint32_t nx_xp_data_len;

	le_oid_t nx_spaceman_oid;
	le_oid_t nx_omap_oid;
	le_oid_t nx_reaper_oid;

	le_uint32_t nx_test_type;

	le_uint32_t nx_max_file_systems;
	le_oid_t nx_fs_oid[NX_MAX_FILE_SYSTEMS];
	le_uint64_t nx_counters[NX_NUM_COUNTERS];
	prange_t nx_blocked_out_prange;
	le_oid_t nx_evict_mapping_tree_oid;
	le_uint64_t nx_flags;
	le_paddr_t nx_efi_jumpstart;
	apfs_uuid_t nx_fusion_uuid;
	prange_t nx_keylocker;
	le_uint64_t nx_ephemeral_info[NX_EPH_INFO_COUNT];

	le_oid_t nx_test_oid;

	le_oid_t nx_fusion_mt_oid;
	le_oid_t nx_fusion_wbc_oid;
	prange_t nx_fusion_wbc;

	le_uint64_t nx_newest_mounted_version;

	prange_t nx_mkb_locker;
};

struct spaceman_free_queue_key_t {
	le_xid_t sfqk_xid;
	le_paddr_t sfqk_paddr;
};
typedef struct spaceman_free_queue_key_t spaceman_free_queue_key_t;

typedef le_uint64_t spaceman_free_queue_val_t;

struct spaceman_free_queue_entry_t {
	spaceman_free_queue_key_t   sfqe_key;
	spaceman_free_queue_val_t   sfqe_count;
};
typedef struct spaceman_free_queue_entry_t spaceman_free_queue_entry_t;

struct spaceman_free_queue_t {
	le_uint64_t sfq_count;
	le_oid_t sfq_tree_oid;
	le_xid_t sfq_oldest_xid;
	le_uint16_t sfq_tree_node_limit;
	le_uint16_t sfq_pad16;
	le_uint32_t sfq_pad32;
	le_uint64_t sfq_reserved;
};
typedef struct spaceman_free_queue_t spaceman_free_queue_t;

struct spaceman_device_t {
	le_uint64_t sm_block_count;
	le_uint64_t sm_chunk_count;
	le_uint32_t sm_cib_count;
	le_uint32_t sm_cab_count;
	le_uint64_t sm_free_count;
	le_uint32_t sm_addr_offset;
	le_uint32_t sm_reserved;
	le_uint64_t sm_reserved2;
};
typedef struct spaceman_device_t spaceman_device_t;

#define SM_ALLOCZONE_INVALID_END_BOUNDARY 0
#define SM_ALLOCZONE_NUM_PREVIOUS_BOUNDARIES 7
#define SM_DATAZONE_ALLOCZONE_COUNT 8

enum sfq {
	SFQ_IP = 0,
	SFQ_MAIN = 1,
	SFQ_TIER2 = 2,
	SFQ_COUNT = 3
};

enum smdev {
	SD_MAIN = 0,
	SD_TIER2 = 1,
	SD_COUNT = 2
};

struct spaceman_allocation_zone_boundaries_t {
	le_uint64_t saz_zone_start;
	le_uint64_t saz_zone_end;
};
typedef struct spaceman_allocation_zone_boundaries_t spaceman_allocation_zone_boundaries_t;

struct spaceman_allocation_zone_info_phys_t {
	spaceman_allocation_zone_boundaries_t saz_current_boundaries;
	spaceman_allocation_zone_boundaries_t saz_previous_boundaries[SM_ALLOCZONE_NUM_PREVIOUS_BOUNDARIES];
	le_uint16_t saz_zone_id;
	le_uint16_t saz_previous_boundary_index;
	le_uint32_t saz_reserved;
};
typedef struct spaceman_allocation_zone_info_phys_t spaceman_allocation_zone_info_phys_t;

struct spaceman_datazone_info_phys_t {
	spaceman_allocation_zone_info_phys_t sdz_allocation_zones[SD_COUNT][SM_DATAZONE_ALLOCZONE_COUNT];
};
typedef struct spaceman_datazone_info_phys_t spaceman_datazone_info_phys_t;

struct spaceman_phys_t {
	obj_phys_t sm_o;
	le_uint32_t sm_block_size;
	le_uint32_t sm_blocks_per_chunk;
	le_uint32_t sm_chunks_per_cib;
	le_uint32_t sm_cibs_per_cab;
	spaceman_device_t sm_dev[SD_COUNT];
	le_uint32_t sm_flags;
	le_uint32_t sm_ip_bm_tx_multiplier;
	le_uint64_t sm_ip_block_count;
	le_uint32_t sm_ip_bm_size_in_blocks;
	le_uint32_t sm_ip_bm_block_count;
	le_paddr_t sm_ip_bm_base;
	le_paddr_t sm_ip_base;
	le_uint64_t sm_fs_reserve_block_count;
	le_uint64_t sm_fs_reserve_alloc_count;
	spaceman_free_queue_t sm_fq[SFQ_COUNT];
	le_uint16_t sm_ip_bm_free_head;
	le_uint16_t sm_ip_bm_free_tail;
	le_uint32_t sm_ip_bm_xid_offset;
	le_uint32_t sm_ip_bitmap_offset;
	le_uint32_t sm_ip_bm_free_next_offset;
	le_uint32_t sm_version;
	le_uint32_t sm_struct_size;
	spaceman_datazone_info_phys_t sm_datazone;
};

struct chunk_info_t {
	le_uint64_t ci_xid;
	le_uint64_t ci_addr;
	le_uint32_t ci_block_count;
	le_uint32_t ci_free_count;
	le_paddr_t ci_bitmap_addr;
};

typedef struct chunk_info_t chunk_info_t;

struct chunk_info_block_t {
	obj_phys_t cib_o;
	le_uint32_t cib_index;
	le_uint32_t cib_chunk_info_count;
	chunk_info_t cib_chunk_info[];
};


struct checkpoint_mapping {
	le_uint32_t cpm_type;
	le_uint32_t cpm_subtype;
	le_uint32_t cpm_size;
	le_uint32_t cpm_pad;
	le_oid_t cpm_fs_oid;
	le_oid_t cpm_oid;
	le_oid_t cpm_paddr;
};
typedef struct checkpoint_mapping checkpoint_mapping_t;

struct checkpoint_map_phys {
	obj_phys_t cpm_o;
	le_uint32_t cpm_flags;
	le_uint32_t cpm_count;
	checkpoint_mapping_t cpm_map[];
};
typedef struct checkpoint_map_phys checkpoint_map_phys_t;

const uint32_t CHECKPOINT_MAP_LAST = 0x00000001;
