/**
 * extfsclone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read ext2/3 super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ext2fs/ext2fs.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>

#define in_use(m, x)    (ext2fs_test_bit ((x), (m)))

#include "partclone.h"
#include "extfsclone.h"
#include "progress.h"
#include "fs_common.h"

ext2_filsys  fs;
char *EXECNAME = "partclone.extfs";
extern fs_cmd_opt fs_opt;

/// open device
static void fs_open(char* device){
    errcode_t retval;
    int use_superblock = 0;
    int use_blocksize = 0;
    int flags;

    flags = EXT2_FLAG_JOURNAL_DEV_OK | EXT2_FLAG_SOFTSUPP_FEATURES;
    if (use_superblock && !use_blocksize) {
	for (use_blocksize = EXT2_MIN_BLOCK_SIZE; use_blocksize <= EXT2_MAX_BLOCK_SIZE; use_blocksize *= 2) {
	    retval = ext2fs_open (device, flags, use_superblock, use_blocksize, unix_io_manager, &fs);
	    if (!retval)
		break;
	}
    } else
	retval = ext2fs_open (device, flags, use_superblock, use_blocksize, unix_io_manager, &fs);

    if (retval) 
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Couldn't find valid filesystem superblock.\n", __FILE__);

    ext2fs_mark_valid(fs);

    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    } else {
        if ((fs->super->s_state & EXT2_ERROR_FS) || !ext2fs_test_valid(fs))
            log_mesg(0, 1, 1, fs_opt.debug, "%s: FS contains a file system with errors\n", __FILE__);
        else if ((fs->super->s_state & EXT2_VALID_FS) == 0)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: FS was not cleanly unmounted\n", __FILE__);
        else if ((fs->super->s_max_mnt_count > 0) && (fs->super->s_mnt_count >= (unsigned) fs->super->s_max_mnt_count)) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: FS has been mounted %u times without being checked\n", __FILE__);
        }
    }

}

/// close device
static void fs_close(){
    ext2fs_close(fs);
}

/// get block size from super block
static int block_size(){
    return EXT2_BLOCK_SIZE(fs->super);
}

/// get device size
static unsigned long long device_size(char* device){
    int size;
    unsigned long long dev_size;
    ext2fs_get_device_size(device, EXT2_BLOCK_SIZE(fs->super), &size);
    dev_size = (unsigned long long)(size * EXT2_BLOCK_SIZE(fs->super));
    return dev_size;
}

/// get total block from super block
static unsigned long long block_count(){
    return (unsigned long long)fs->super->s_blocks_count;
}

/// get used blocks ( total - free ) from super block
static unsigned long long get_used_blocks(){
    return (unsigned long long)(fs->super->s_blocks_count - fs->super->s_free_blocks_count);
}

/// readbitmap - cread and heck bitmap, reference dumpe2fs
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui){
    errcode_t retval;
    unsigned long group;
    unsigned long long current_block, block;
    unsigned long long free, used, gfree, gused;
    char *block_bitmap=NULL;
    int block_nbytes;
    unsigned long long blk_itr;
    int bg_flags = 0;
    int start = 0;
    int bit_size = 1;

    log_mesg(2, 0, 0, fs_opt.debug, "%s: readbitmap %i\n", __FILE__, bitmap);

    fs_open(device);
    retval = ext2fs_read_bitmaps(fs); /// open extfs bitmap
    if (retval)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Couldn't find valid filesystem bitmap.\n", __FILE__);

    block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
    if (fs->block_map)
	block_bitmap = malloc(block_nbytes);

    /// initial image bitmap as 1 (all block are used)
    for(block = 0; block < image_hdr.totalblock; block++)
	bitmap[block] = 1; 

    free = 0;
    used = 0;
    current_block = 0;
    blk_itr = fs->super->s_first_data_block;

    /// init progress
    progress_bar	prog;		/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, bit_size);

    /// each group
    for (group = 0; group < fs->group_desc_count; group++) {

	gfree = 0;
	gused = 0;

	if (block_bitmap) {
	    ext2fs_get_block_bitmap_range(fs->block_map, blk_itr, block_nbytes << 3, block_bitmap);

	    /// each block in group
	    for (block = 0; ((block < fs->super->s_blocks_per_group) && (current_block < (image_hdr.totalblock-1))); block++) {
		current_block = block + blk_itr;
		if (fs->super->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM)
		    bg_flags = fs->group_desc[group].bg_flags;

		/// check block is used or not
		if ((!in_use (block_bitmap, block)) || (bg_flags&EXT2_BG_BLOCK_UNINIT)) {
		    free++;
		    gfree++;
		    bitmap[current_block] = 0;
		    log_mesg(3, 0, 0, fs_opt.debug, "%s: free block %lu at group %i\n", __FILE__, (current_block), group);
		} else {
		    used++;
		    gused++;
		    bitmap[current_block] = 1;
		    log_mesg(3, 0, 0, fs_opt.debug, "%s: used block %lu at group %i\n", __FILE__, (current_block), group);
		}
		/// update progress
		update_pui(&prog, current_block, 0);//keep update
	    }
	    blk_itr += fs->super->s_blocks_per_group;
	}
	/// check free blocks in group
	if (gfree != fs->group_desc[group].bg_free_blocks_count)
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap erroe at %i group.\n", __FILE__, group);
    }
    /// check all free blocks in partition
    if (free != fs->super->s_free_blocks_count)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, free:%i\n", __FILE__, free);
    fs_close();
    /// update progress
    update_pui(&prog, 1, 1);//finish
}

/// get extfs type
static int test_extfs_type(char* device){
    int ext2 = 1;
    int ext3 = 2;
    int ext4 = 3;
    fs_open(device);
    if(fs->super->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM){
	log_mesg(1, 0, 0, fs_opt.debug, "%s: test feature as EXT4\n", __FILE__);
	return ext4;
    } else if (fs->super->s_feature_compat & EXT3_FEATURE_COMPAT_HAS_JOURNAL){
	log_mesg(1, 0, 0, fs_opt.debug, "%s: test feature as EXT3\n", __FILE__);
	return ext3;
    } else {
	log_mesg(1, 0, 0, fs_opt.debug, "%s: test feature as EXT2\n", __FILE__);
	return ext2;
    }
    fs_close();
    return 0;
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    int fs_type = 0;
    fs_type = test_extfs_type(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, extfs_MAGIC, FS_MAGIC_SIZE);
    fs_open(device);
    image_hdr->block_size = (int)block_size();
    image_hdr->totalblock = (unsigned long long)block_count();
    image_hdr->usedblocks = (unsigned long long)get_used_blocks();
    //image_hdr->device_size = (unsigned long long)device_size(device);
    image_hdr->device_size = (unsigned long long)(image_hdr->block_size * image_hdr->totalblock);
    fs_close();
}

