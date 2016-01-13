/**
 * f2fsclone.c - part of Partclone project
 *
 * Copyright (c) 2014~ Thomas Tsai <thomas at nchc org tw>
 *
 * read f2fs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <libgen.h>
#include "f2fs/fsck.h"

#include "partclone.h"
#include "f2fsclone.h"
#include "progress.h"
#include "fs_common.h"

char *EXECNAME = "partclone.f2fs";
extern fs_cmd_opt fs_opt;

struct f2fs_fsck gfsck = {
    .sbi.fsck = &gfsck,
};

struct f2fs_sb_info *sbi = &gfsck.sbi;
extern struct f2fs_configuration config;

/// open device
static void fs_open(char* device){

    int ret = 0;
    f2fs_init_configuration(&config);
    config.device_name = device;


    if (f2fs_dev_is_umounted(&config) < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_dev_is_umounted\n", __FILE__);

    /* Get device */
    if (f2fs_get_device_info(&config) < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_get_device_info fail\n", __FILE__);

    if (f2fs_do_mount(sbi) < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_do_mount fail\n", __FILE__);

    ret = fsck_init(sbi);
    if (ret < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: fsck_init init fail\n", __FILE__);

    fsck_chk_orphan_node(sbi);

}

/// close device
static void fs_close(){

    fsck_free(sbi);
    f2fs_do_umount(sbi);
}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    off_t block = 0;;
    int start = 0;
    int bit_size = 1;
    unsigned long long bused, bfree;

    fs_open(device);
    struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    /// bitmap test
    log_mesg(1, 0, 0, fs_opt.debug, "%s: start f2fs bitmap dump\n", __FILE__);
    struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: start fsck sit\n", __FILE__);
    for ( block = 0; block <= sb->main_blkaddr ; block++ ){
    
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x1. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    bused++;
	    pc_set_bit(block, bitmap, image_hdr.totalblock);
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);
    }

    for ( block = sb->main_blkaddr ; block < sb->block_count ; block++ ){
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block = %i\n", __FILE__, block);
	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, block), fsck->sit_area_bitmap) == 0x0) {
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x0. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    pc_clear_bit(block, bitmap, image_hdr.totalblock);
	    bfree++;
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %llu", __FILE__, block);
	}else{
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x1. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    bused++;
	    pc_set_bit(block, bitmap, image_hdr.totalblock);
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);

	}

	/// update progress
	update_pui(&prog, block, block, 0);
    }

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{

    fs_open(device);

    struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);
    struct f2fs_checkpoint *cp = F2FS_CKPT(sbi);

    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, f2fs_MAGIC, FS_MAGIC_SIZE);

    image_hdr->block_size  = F2FS_BLKSIZE;
    image_hdr->totalblock  = sb->block_count;
    image_hdr->usedblocks  = (sb->segment_count-cp->free_segment_count)*DEFAULT_BLOCKS_PER_SEGMENT;
    image_hdr->device_size = config.total_sectors*config.sector_size;
    fs_close();
}

