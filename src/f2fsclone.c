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
#include <time.h>
#include "f2fs/f2fs_fs.h"
#include "f2fs/fsck.h"

#include "partclone.h"
#include "f2fsclone.h"
#include "progress.h"
#include "fs_common.h"

struct f2fs_fsck gfsck;
struct f2fs_sb_info *sbi;

/// open device
static void fs_open(char* device){

    int ret = 0;

    f2fs_init_configuration();
    c.devices[0].path = device;
    //FS only on RO mounted device
    c.force = 1;
    c.ro = 1;
    c.fix_on = 0; 
    c.auto_fix = 0;
    if (fs_opt.debug >= 1){
        c.dbg_lv = fs_opt.debug - 1;
    } else {
        c.dbg_lv = fs_opt.debug;
    }

    memset(&gfsck, 0, sizeof(gfsck)); 
    gfsck.sbi.fsck = &gfsck; 
    sbi = &gfsck.sbi; 

    if (f2fs_devs_are_umounted() < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_dev_is_umounted\n", __FILE__);

    /* Get device */
    if (f2fs_get_device_info() < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_get_device_info fail\n", __FILE__);

    if (f2fs_do_mount(sbi) < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: f2fs_do_mount fail\n", __FILE__);

    fsck_init(sbi);
    if (ret < 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: fsck_init init fail\n", __FILE__);
}

/// close device
static void fs_close(){

    struct stat *st_buf;
    char *path = c.devices[0].path;
    f2fs_do_umount(sbi);
    st_buf = malloc(sizeof(struct stat));
    if (stat(path, st_buf) == 0 && S_ISBLK(st_buf->st_mode)) {
        int fd = open(path, O_RDONLY | O_EXCL);
        if (fd >= 0) {
             close(fd);
        } else if (errno == EBUSY) { 
            free(st_buf);
        }
    }
    sleep(5);
    //int ret = f2fs_finalize_device();

    //if (sbi->ckpt)
    //    free(sbi->ckpt);
    //if (sbi->raw_super)
    //    free(sbi->raw_super);
    //f2fs_release_sparse_resource();

}

///  readbitmap - read bitmap
extern void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    off_t block = 0;;
    int start = 0;
    int bit_size = 1;

    fs_open(device);
    struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    /// bitmap test
    log_mesg(1, 0, 0, fs_opt.debug, "%s: start f2fs bitmap dump\n", __FILE__);
    struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: start fsck sit\n", __FILE__);
    for ( block = 0; block <= sb->main_blkaddr ; block++ ){

	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x1. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    pc_set_bit(block, bitmap, fs_info.totalblock);
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);
    }

    for ( block = sb->main_blkaddr ; block < sb->block_count ; block++ ){
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block = %i\n", __FILE__, block);
	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, block), fsck->sit_area_bitmap) == 0x0) {
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x0. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    pc_clear_bit(block, bitmap, fs_info.totalblock);
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %llu", __FILE__, block);
	}else{
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: test SIT bitmap is 0x1. blk_addr[0x%x] %i\n", __FILE__, block, block);
	    pc_set_bit(block, bitmap, fs_info.totalblock);
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
extern void read_super_blocks(char* device, file_system_info* fs_info)
{

    fs_open(device);

    struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);
    struct f2fs_checkpoint *cp = F2FS_CKPT(sbi);

    strncpy(fs_info->fs, f2fs_MAGIC, FS_MAGIC_SIZE);

    fs_info->block_size  = F2FS_BLKSIZE;
    fs_info->totalblock  = sb->block_count;
    fs_info->usedblocks  = (sb->segment_count-cp->free_segment_count)*DEFAULT_BLOCKS_PER_SEGMENT;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_info->device_size = c.total_sectors*c.sector_size;
    log_mesg(2, 0, 0, fs_opt.debug, "%s: block_size %lld \n", __FILE__, fs_info->block_size );
    fs_close();

}

