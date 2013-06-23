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

dal_t		 *dal;
reiserfs_fs_t	 *fs;
char *EXECNAME = "partclone.reiserfs";

/// open device
static void fs_open(char* device){

    if (!(dal = (dal_t*)file_dal_open(device, DEFAULT_BLOCK_SIZE, O_RDONLY))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Couldn't create device abstraction for %s.\n", __FILE__, device);
    }

    if (!(fs = reiserfs_fs_open(dal, dal))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Couldn't open filesystem on %s.\n", __FILE__, device);
    }

    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    }else{
        if (get_sb_umount_state(fs->super) != FS_CLEAN)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);
    }

}

/// close device
static void fs_close(){

    reiserfs_fs_close(fs);
    file_dal_close(dal);

}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    reiserfs_bitmap_t    *fs_bitmap;
    reiserfs_tree_t	 *tree;
    unsigned long long	 blk = 0;
    unsigned long long 	 bused = 0, bfree = 0;
    int start = 0;
    int bit_size = 1;
    int done = 0;
    
    fs_open(device);
    tree = reiserfs_fs_tree(fs);
    fs_bitmap = tree->fs->bitmap;
    
    /// init progress
    progress_bar   bprog;	/// progress_bar structure defined in progress.h
    progress_init(&bprog, start, fs->super->s_v1.sb_block_count, fs->super->s_v1.sb_block_count, BITMAP, bit_size);

    for( blk = 0; blk < (unsigned long long)fs->super->s_v1.sb_block_count; blk++ ){
	
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block sb_block_count %llu\n", __FILE__, fs->super->s_v1.sb_block_count);
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block bitmap check %llu\n", __FILE__, blk);
	if(reiserfs_tools_test_bit(blk, fs_bitmap->bm_map)){
	    bused++;
	    pc_set_bit(blk, bitmap);
	}else{
	    bfree++;
	    pc_clear_bit(blk, bitmap);
	}
	/// update progress
	update_pui(&bprog, blk, blk, done);

    }

    if(bfree != fs->super->s_v1.sb_free_blocks)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, free:%i\n", __FILE__, bfree);

    fs_close();
    /// update progress
    update_pui(&bprog, 1, 1, 1);

}

void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, reiserfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = (int)fs->super->s_v1.sb_block_size;
    image_hdr->totalblock = (unsigned long long)fs->super->s_v1.sb_block_count;
    image_hdr->usedblocks = (unsigned long long)(fs->super->s_v1.sb_block_count - fs->super->s_v1.sb_free_blocks);
    image_hdr->device_size = (unsigned long long)(image_hdr->block_size * image_hdr->totalblock);
    fs_close();
}

