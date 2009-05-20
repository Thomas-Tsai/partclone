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
#include "partclone.h"
#include "reiserfsclone.h"
#include "progress.h"

dal_t		 *dal;
reiserfs_fs_t	 *fs;
char *EXECNAME = "partclone.reiserfs";

/// open device
static void fs_open(char* device){
    int debug = 2;

    if (!(dal = (dal_t*)file_dal_open(device, DEFAULT_BLOCK_SIZE, O_RDONLY))) {
	log_mesg(0, 1, 1, debug, "Couldn't create device abstraction for %s.\n", device);
    }

    if (!(fs = reiserfs_fs_open(dal, dal))) {
	log_mesg(0, 1, 1, debug, "Couldn't open filesystem on %s.\n",device);
    }

    if (get_sb_umount_state(fs->super) != FS_CLEAN)
	log_mesg(0, 1, 1, debug, "Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n");

}

/// close device
static void fs_close(){

    reiserfs_fs_close(fs);
    file_dal_close(dal);

}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui)
{
    reiserfs_bitmap_t    *fs_bitmap;
    reiserfs_tree_t	 *tree;
    reiserfs_block_t	 *node;
    blk_t		 blk;
    unsigned long long 	 bused = 0, bfree = 0;
    int debug = 1;
    int	start, res, stop;	/// start, range, stop number for progre
    //printf("start initial image hdr\n");
    
    fs_open(device);
    tree = reiserfs_fs_tree(fs);
    fs_bitmap = tree->fs->bitmap;
    
    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    start = 0;		    /// start number of progress bar
    stop = (int)image_hdr.totalblock;	/// get the end of progress number, only used block
    res = image_hdr.totalblock>>10;		    /// the end of progress number
    progress_init(&prog, start, stop, res, 1);

    for(blk = 0 ; (int)blk < fs->super->s_v1.sb_block_count; blk++){
	if(reiserfs_tools_test_bit(blk, fs_bitmap->bm_map)){
	    bused++;
	    bitmap[blk] = 1;
	}else{
	    bfree++;
	    bitmap[blk] = 0;
	}
	/// update progress
	update_pui(&prog, blk, 0);

    }

    if(bfree != fs->super->s_v1.sb_free_blocks)
	log_mesg(0, 1, 1, debug, "bitmap free count err, free:%i\n", bfree);

    fs_close();
    /// update progress
    update_pui(&prog, blk, 1);

}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, reiserfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = (int)fs->super->s_v1.sb_block_size;
    image_hdr->totalblock = (unsigned long long)fs->super->s_v1.sb_block_count;
    image_hdr->usedblocks = (unsigned long long)(fs->super->s_v1.sb_block_count - fs->super->s_v1.sb_free_blocks);
    image_hdr->device_size = (unsigned long long)(image_hdr->block_size * image_hdr->totalblock);
    fs_close();
}

