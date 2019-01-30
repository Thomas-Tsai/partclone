/**
 * apfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read apfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/types.h>

#include "partclone.h"
#include "apfsclone.h"
#include "progress.h"
#include "fs_common.h"

int APFSDEV;

/// open device
static void fs_open(char* device){

    int block = 0;
    unsigned long *buf;
    unsigned long *buf_id_mapping;
    unsigned long *buf_4_7;
    size_t buffer_size = 4096;
    size_t size = 0;
    char *file;
    struct APFS_Superblock_NXSB nxsb;

    APFSDEV = open(device, O_RDONLY);
    if (APFSDEV == -1){printf("open error");}
    buf = (char *)malloc(buffer_size);
    memset(buf, 0, buffer_size);
    size = read(APFSDEV, buf, buffer_size);
    memcpy(&nxsb, buf, sizeof(nxsb));
    printf("0x%jX\n", nxsb.hdr.checksum);
    printf("block size %x\n", nxsb.block_size);
    printf("block count %x\n", nxsb.block_count);
    printf("next_xid %x\n", nxsb.next_xid);
    printf("current_sb_start %x\n", nxsb.current_sb_start);
    printf("current_sb_len %x\n", nxsb.current_sb_len);
    printf("current_spaceman_start %x\n", nxsb.current_spaceman_start);
    printf("current_spaceman_len %x\n", nxsb.current_spaceman_len);
    printf("bid_spaceman_area_start  %x\n", nxsb.bid_spaceman_area_start);

}

/// close device
static void fs_close(){
    close(APFSDEV);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    unsigned long          *fs_bitmap;
    unsigned long long     bit, block, bused = 0, bfree = 0;
    int start = 0;
    int bit_size = 1;

    fs_open(device);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    int block = 0;
    unsigned long *buf;
    unsigned long *buf_id_mapping;
    unsigned long *buf_4_7;
    size_t buffer_size = 4096;
    size_t size = 0;
    char *file;
    int ret;
    struct APFS_Superblock_NXSB nxsb;

    file = argv[1];
    ret = open(file, O_RDONLY);
    if (ret == -1){printf("open error");}
    buf = (char *)malloc(buffer_size);
    memset(buf, 0, buffer_size);
    size = read(ret, buf, buffer_size);
    memcpy(&nxsb, buf, sizeof(nxsb));
    printf("0x%jX\n", nxsb.hdr.checksum);
    printf("block size %x\n", nxsb.block_size);
    printf("block count %x\n", nxsb.block_count);
    printf("next_xid %x\n", nxsb.next_xid);
    printf("current_sb_start %x\n", nxsb.current_sb_start);
    printf("current_sb_len %x\n", nxsb.current_sb_len);
    printf("current_spaceman_start %x\n", nxsb.current_spaceman_start);
    printf("current_spaceman_len %x\n", nxsb.current_spaceman_len);
    printf("bid_spaceman_area_start  %x\n", nxsb.bid_spaceman_area_start);

    size_t id_mapping_block = nxsb.bid_spaceman_area_start;

    buf_id_mapping = (char *)malloc(buffer_size);
    memset(buf_id_mapping, 0, buffer_size);

    size = pread(ret, buf_id_mapping, buffer_size, id_mapping_block*buffer_size);
    
    struct APFS_BlockHeader apfs_bh;
    struct APFS_TableHeader apfs_th;
    struct APFS_Block_8_5_Spaceman apfs_spaceman;

    memcpy(&apfs_bh, buf_id_mapping, sizeof(APFS_BlockHeader));
    memcpy(&apfs_th, buf_id_mapping+sizeof(APFS_BlockHeader), sizeof(APFS_TableHeader));
    memcpy(&apfs_spaceman, buf_id_mapping, sizeof(APFS_Block_8_5_Spaceman));

    printf("block nid %x\n", apfs_bh.nid);
    printf("block xid %x\n", apfs_bh.xid);
    printf("block type %x\n", apfs_bh.type);

    printf("block table entries_cnt %x\n", apfs_th.entries_cnt);

    printf("blocks_total %lld\n", apfs_spaceman.blocks_total);
    printf("blocks_free %lld\n", apfs_spaceman.blocks_free);
    printf("blocks blockid_vol_bitmap_hdr %X\n", apfs_spaceman.blockid_vol_bitmap_hdr);

    // read bitmap
    struct APFS_Block_4_7_Bitmaps apfs_4_7;
    buf_4_7 = (char *)malloc(buffer_size);
    size = pread(ret, buf_4_7, buffer_size, apfs_spaceman.blockid_vol_bitmap_hdr*buffer_size);
    memcpy(&apfs_4_7, buf_4_7, sizeof(APFS_Block_4_7_Bitmaps));
    printf("block type %x\n", apfs_4_7.hdr.type);
    printf("block type %x\n", apfs_4_7.hdr.nid);

    size_t k; 

    for (k = 0; k < apfs_4_7.tbl.entries_cnt; k++) 
        { 
            printf("%X, %X, %X, %X, %X\n", apfs_4_7.bmp[k].xid, apfs_4_7.bmp[k].offset, apfs_4_7.bmp[k].bits_total, apfs_4_7.bmp[k].bits_avail, apfs_4_7.bmp[k].block);
        } 


    //for(bit = 0; bit < reiser4_format_get_len(fs->format); bit++){
    //    block = bit ;
    //    if(reiser4_bitmap_test(fs_bitmap, bit)){
    //        bused++;
    //        pc_set_bit(block, bitmap, fs_info.totalblock);
    //        log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);
    //    } else {
    //        pc_clear_bit(block, bitmap, fs_info.totalblock);
    //        bfree++;
    //        log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %llu", __FILE__, block);
    //    }
    //    /// update progress
        update_pui(&prog, bit, bit, 0);

    //}

    //if(bfree != reiser4_format_get_free(fs->format))
    //    log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, bfree:%llu, sfree=%llu\n", __FILE__, bfree, reiser4_format_get_free(fs->format));

    fs_close();
    ///// update progress
    update_pui(&prog, 1, 1, 1);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{
    unsigned long      *fs_bitmap;
    unsigned long long free_blocks=0;

    fs_open(device);

    strncpy(fs_info->fs, apfs_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = 0;
    fs_info->totalblock  = 0;
    fs_info->usedblocks  = 0;
    fs_info->device_size = 0;
    fs_close();
}

