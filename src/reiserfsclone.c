/**
 * reiserfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read reiserfs super block and bitmap
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
#include <reiserfs/reiserfs.h>
#include <dal/file_dal.h>
#include "partclone.h"
#include "reiserfsclone.h"
#include "progress.h"
#include "fs_common.h"

#define MIN_REISERFS_BLOCK_SIZE 512 // Smallest supported ReiserFS block size
#define MAX_REISERFS_BLOCK_SIZE 4096 // Largest supported ReiserFS block size

dal_t		 *dal;
reiserfs_fs_t	 *fs;

/// open device
static int fs_open(char* device){

    if (!(dal = (dal_t*)file_dal_open(device, DEFAULT_BLOCK_SIZE, O_RDONLY))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Couldn't create device abstraction for %s.\n", __FILE__, device);
        return -1;
    }

    if (!(fs = reiserfs_fs_open(dal, dal))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Couldn't open filesystem on %s.\n", __FILE__, device);
        file_dal_close(dal);
        dal = NULL;
        return -1;
    }

    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    }else{
        if (get_sb_umount_state(fs->super) != FS_CLEAN) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);
            // This is a warning, not a fatal error for partclone. Continue.
        }
    }
    return 0; // Success
}

/// close device
static void fs_close(){

    if (fs) {
        reiserfs_fs_close(fs);
        fs = NULL;
    }
    if (dal) {
        file_dal_close(dal);
        dal = NULL;
    }
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    reiserfs_bitmap_t    *fs_bitmap;
    reiserfs_tree_t	 *tree;
    unsigned long long	 blk = 0;
    unsigned long long 	 bused = 0, bfree = 0;
    int start = 0;
    int bit_size = 1;
    int done = 0;

    if (fs_open(device) != 0) {
        return; // Abort
    }

    // Initialize bitmap based on fs_info.totalblock
    pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);

    tree = reiserfs_fs_tree(fs);
    if (!tree) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get ReiserFS tree.\n");
        fs_close();
        return;
    }
    fs_bitmap = tree->fs->bitmap;
    if (!fs_bitmap) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get ReiserFS bitmap.\n");
        fs_close();
        return;
    }

    if (fs_info.totalblock == 0 || (unsigned long long)fs->super->s_v1.sb_block_count != fs_info.totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Total block count mismatch or invalid: fs_info.totalblock=%llu, sb_block_count=%u.\n",
                 fs_info.totalblock, fs->super->s_v1.sb_block_count);
        fs_close();
        return;
    }

    /// init progress
    progress_bar   bprog;	/// progress_bar structure defined in progress.h
    progress_init(&bprog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size); // Use fs_info.totalblock for progress

    for( blk = 0; blk < fs_info.totalblock; blk++ ){
	
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block totalblock %llu\n", __FILE__, fs_info.totalblock);
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block bitmap check %llu\n", __FILE__, blk);
	if(reiserfs_tools_test_bit(blk, fs_bitmap->bm_map)){
	    bused++;
	    pc_set_bit(blk, bitmap, fs_info.totalblock);
	}else{
	    bfree++;
	    pc_clear_bit(blk, bitmap, fs_info.totalblock);
	}
	/// update progress
	update_pui(&bprog, blk, blk, done);

    }

    if(bfree != fs->super->s_v1.sb_free_blocks)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, free:%llu (from map), expected:%u (from sb)\n", __FILE__, bfree, fs->super->s_v1.sb_free_blocks);

    fs_close();
    /// update progress
    update_pui(&bprog, 1, 1, 1);
}
void read_super_blocks(char* device, file_system_info* fs_info)
{
    if (fs_open(device) != 0) {
        return; // Abort
    }
    strncpy(fs_info->fs, reiserfs_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = fs->super->s_v1.sb_block_size;

    if (fs_info->block_size < MIN_REISERFS_BLOCK_SIZE || fs_info->block_size > MAX_REISERFS_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %u.\n", fs_info->block_size);
        fs_close();
        return;
    }

    fs_info->totalblock  = fs->super->s_v1.sb_block_count;

    if (fs_info->totalblock == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks detected in superblock: %llu. Max allowed for device size: %llu\n", fs_info->totalblock, ULLONG_MAX / fs_info->block_size);
        fs_close();
        return;
    }

    fs_info->device_size = fs_info->block_size * fs_info->totalblock;
    fs_info->usedblocks  = fs->super->s_v1.sb_block_count - fs->super->s_v1.sb_free_blocks;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_close();
}
