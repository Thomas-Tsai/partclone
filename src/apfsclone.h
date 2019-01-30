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


typedef unsigned char apfs_uuid_t[16]; 

struct APFS_BlockHeader 
{ 
        uint64_t checksum; 
        uint64_t nid; 
        uint64_t xid; 
        uint16_t type; 
        uint16_t flags; // See below 
        uint32_t subtype; 
}; 
typedef struct APFS_BlockHeader APFS_BlockHeader;

struct APFS_TableHeader // at 0x20 
{ 
    uint16_t page; 
    uint16_t level; 
    uint32_t entries_cnt; 
}; 
typedef struct APFS_TableHeader APFS_TableHeader;

/* 
Known Flags: 
0x1000 Encrypted 
0x4000 Belongs to volume 
0x8000 Belongs to container 
*/ 
 
struct APFS_BTHeader // at 0x20 
{ 
        uint16_t flags; // Bit 0x4 : Fixed 
        uint16_t level; // 0 = Leaf 
        uint32_t entries_cnt; 
        uint16_t keys_offs; 
        uint16_t keys_len; 
        uint16_t free_offs; 
        uint16_t free_len; 
        uint16_t unk_30; 
        uint16_t unk_32; 
        uint16_t unk_34; 
        uint16_t unk_36; 
};
typedef struct  APFS_BTHeader APFS_BTHeader;

struct APFS_BTFooter 
{ 
        uint32_t unk_FD8; 
        uint32_t unk_FDC; // 0x1000 - Node Size? 
        uint32_t min_key_size; // Key Size - or 0 
        uint32_t min_val_size; // Value Size - or 0 
        uint32_t max_key_size; // (Max) Key Size - or 0 
        uint32_t max_val_size; // (Max) Value Size - or 0 
        uint64_t entries_cnt; // Total entries in B*-Tree 
        uint64_t nodes_cnt; // Total nodes in B*-Tree 
};
typedef struct APFS_BTFooter APFS_BTFooter; 
 
struct APFS_BTEntry 
{ 
        uint16_t key_offs; // offs = base + key_offs 
        uint16_t key_len; 
        uint16_t value_offs; // offs = 0x1000 - value_offs 
        uint16_t value_len; 
};
typedef struct APFS_BTEntry APFS_BTEntry;

struct APFS_Superblock_NXSB // Ab 0x20 
{ 
        APFS_BlockHeader hdr; 
        uint32_t signature; // 'NXSB' / 0x4253584E 
        uint32_t block_size; 
        uint64_t block_count; 
        uint64_t unk_30; 
        uint64_t unk_38; 
        uint64_t unk_40; 
        apfs_uuid_t container_guid; 
        uint64_t next_nid; // Next node id (?) 
        uint64_t next_xid; // Next transaction id (?) 
        uint32_t sb_area_cnt; // Number of blocks for NXSB + 4_C ? 
        uint32_t spaceman_area_cnt; // Number of blocks for the rest 
        uint64_t bid_sb_area_start; // Block-ID (0x4000000C) - No 
        uint64_t bid_spaceman_area_start; // Block-ID (0x80000005) => Node-ID 0x400 
        uint32_t next_sb; // Next 4_C + NXSB? (+sb_area_start) 
        uint32_t next_spaceman; // Next 8_5/2/11? (+blockid_spaceman_area_start) 
        uint32_t current_sb_start; // Start 4_C+NXSB block (+sb_area_start) 
        uint32_t current_sb_len; // Length 4_C+NXSB block 
        uint32_t current_spaceman_start; // Start 8_5/2/11 blocks (+blockid_spaceman_area_start) 
        uint32_t current_spaceman_len; // No of 8_5/2/11 blocks 
        uint64_t nid_spaceman;     // Node-ID (0x400) => (0x80000005) 
        uint64_t bid_nodemap; // Block-ID => (0x4000000B) => B*-Tree for mapping node-id -> volume APSB superblocks 
        uint64_t nid_8x11;    // Node-ID (0x401) => (0x80000011) 
        uint32_t unk_B0; 
        uint32_t unk_B4; 
        uint64_t nid_apsb[100]; // List of the node-id's of the volume superblocks (not sure about the length of this list though ...) 
        uint64_t unk_3D8[0x23]; 
        uint64_t unk_4F0[4]; 
        uint64_t keybag_blk_start; 
        uint64_t keybag_blk_count; 
        uint64_t unk_520; 
        // There's some more stuff here, but I have no idea about it's meaning ... 
};
typedef struct APFS_Superblock_NXSB APFS_Superblock_NXSB;

struct APFS_Superblock_APSB_AccessInfo 
{ 
    char accessor[0x20]; 
    uint64_t timestamp; 
    uint64_t xid; 
}; 
typedef struct  APFS_Superblock_APSB_AccessInfo APFS_Superblock_APSB_AccessInfo;

struct APFS_Superblock_APSB 
{ 
        APFS_BlockHeader hdr; 
        uint32_t signature; // APSB, 0x42535041 
        uint32_t unk_24; 
        uint64_t features_28; // Features 3 
        uint64_t features_30; // Features 2 
	uint64_t features_38; // Features: & 0x09: 0x00 = old format (iOS), 0x01 = case-insensitive, 0x08 = case-sensitive 
        uint64_t unk_40; 
        uint64_t blocks_reserved; 
        uint64_t blocks_quota; 
        uint64_t unk_58; // Node-ID 
        uint64_t unk_60; 
        uint32_t unk_68; 
        uint32_t unk_6C; 
        uint32_t unk_70; 
        uint32_t unk_74; 
        uint32_t unk_78; // 40000002 
        uint32_t unk_7C; // 40000002 
        uint64_t blockid_nodemap; // Block ID -> 4000000B -> Node Map Tree Root (40000002/B) - Node Map only for Directory! 
        uint64_t nodeid_rootdir; // Node ID -> Root Directory 
        uint64_t blockid_blockmap; // Block ID -> 40000002/F Block Map Tree Root 
        uint64_t blockid_4xBx10_map; // Block ID -> Root of 40000002/10 Tree 
        uint64_t unk_A0; 
        uint64_t unk_A8; 
        uint64_t unk_B0; 
        uint64_t unk_B8; 
        uint64_t unk_C0; 
        uint64_t unk_C8; 
        uint64_t unk_D0; 
        uint64_t unk_D8; 
        uint64_t unk_E0; 
        uint64_t unk_E8; 
        apfs_uuid_t guid; 
        uint64_t timestamp_100; 
	uint64_t flags_108; // TODO: No version, but flags: Bit 0x1: 0 = Encrypted, 1 = Unencrypted. Bit 0x2: Effaceable 
        APFS_Superblock_APSB_AccessInfo access_info[9]; 
        char vol_name[0x100]; 
        uint64_t unk_3C0; 
        uint64_t unk_3C8; 
};
typedef struct APFS_Superblock_APSB structAPFS_Superblock_APSB;
 
struct APFS_Block_8_5_Spaceman 
{ 
        APFS_BlockHeader hdr; 
        uint32_t unk_20; 
        uint32_t unk_24; 
        uint32_t unk_28; 
        uint32_t unk_2C; 
        uint64_t blocks_total; 
        uint64_t unk_38; 
        uint64_t unk_40; 
        uint64_t blocks_free; 
        uint64_t unk_50; 
        uint64_t unk_58; 
        uint64_t unk_60; 
        uint64_t unk_68; 
        uint64_t unk_70; 
        uint64_t unk_78; 
        uint64_t unk_80; 
        uint64_t unk_88; 
        uint32_t unk_90; 
        uint32_t unk_94; 
        uint64_t blockcnt_bitmaps_98; // Container-Bitmaps 
        uint32_t unk_A0; 
        uint32_t blockcnt_bitmaps_A4; // Mgr-Bitmaps? 
        uint64_t blockid_begin_bitmaps_A8; // Mgr-Bitmaps 
        uint64_t blockid_bitmaps_B0; // Container-Bitmaps 
        uint64_t unk_B8; 
        uint64_t unk_C0; 
        uint64_t unk_C8; 
        uint64_t nodeid_obsolete_1; // Obsolete B*-Tree fuer Mgr-Bitmap-Blocks 
        uint64_t unk_D8; 
        uint64_t unk_E0; 
        uint64_t unk_E8; 
        uint64_t unk_F0; 
        uint64_t nodeid_obsolete_2; // Obsolete B*-Tree fuer Volume; 
        uint64_t unk_100; 
        uint64_t unk_108; 
        uint64_t unk_110; 
        uint64_t unk_118; 
        uint64_t unk_120; 
        uint64_t unk_128; 
        uint64_t unk_130; 
        uint64_t unk_138; 
        uint16_t unk_140; 
        uint16_t unk_142; 
        uint32_t unk_144; 
        uint32_t unk_148; 
        uint32_t unk_14C; 
        uint64_t unk_150; 
        uint64_t unk_158; 
        uint16_t unk_160[0x10]; 
        uint64_t blockid_vol_bitmap_hdr; 
        // Ab A08 bid bitmap-header 
}; 
typedef struct APFS_Block_8_5_Spaceman APFS_Block_8_5_Spaceman;

struct APFS_BitmapPtr 
{ 
    uint64_t xid; 
    uint64_t offset; 
    uint32_t bits_total; 
    uint32_t bits_avail; 
    uint64_t block; 
}; 
typedef struct APFS_BitmapPtr APFS_BitmapPtr;


struct APFS_Block_4_7_Bitmaps 
{ 
    APFS_BlockHeader hdr; 
    APFS_TableHeader tbl; 
    APFS_BitmapPtr bmp[0x7E]; 
}; 
typedef struct APFS_Block_4_7_Bitmaps APFS_Block_4_7_Bitmaps;


