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

ext2_filsys  fs;
char *EXECNAME = "clone.extfs";

/// open device
static void fs_open(char* device){
    errcode_t retval;
    int use_superblock = 0;
    int use_blocksize = 0;
    int flags;
    int debug = 2;

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
	log_mesg(0, 1, 1, debug, "Couldn't find valid filesystem superblock.\n");

    ext2fs_mark_valid (fs);

    if ((fs->super->s_state & EXT2_ERROR_FS) || !ext2fs_test_valid(fs))
	log_mesg(0, 1, 1, debug, "%s contains a file system with errors\n", device);
    else if ((fs->super->s_state & EXT2_VALID_FS) == 0)
	log_mesg(0, 1, 1, debug, "%s was not cleanly unmounted\n", device);
    else if ((fs->super->s_max_mnt_count > 0) && (fs->super->s_mnt_count >= (unsigned) fs->super->s_max_mnt_count)) {
	log_mesg(0, 1, 1, debug, "%s has been mounted %u times without being checked\n", device);
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
extern void readbitmap(char* device, image_head image_hdr, char* bitmap){
    errcode_t retval;
    unsigned long group;
    unsigned long long current_block, block;
    unsigned long long free, used, gfree, gused;
    char *block_bitmap=NULL;
    int block_nbytes;
    blk_t blk_itr;
    blk_t first_block, last_block;
    blk_t super_blk, old_desc_blk, new_desc_blk;
    int bg_flags = 0;

    int debug = 3;

    log_mesg(2, 0, 0, debug, "readbitmap %i\n",bitmap);

    fs_open(device);
    retval = ext2fs_read_bitmaps(fs); /// open extfs bitmap
    if (retval)
	log_mesg(0, 1, 1, debug, "Couldn't find valid filesystem bitmap.\n");

    block_nbytes = EXT2_BLOCKS_PER_GROUP(fs->super) / 8;
    if (fs->block_map)
	block_bitmap = malloc(block_nbytes);

    first_block = fs->super->s_first_data_block;

    /// initial image bitmap as 1 (all block are used)
    for(block = 0; block < image_hdr.totalblock; block++)
	bitmap[block] = 1; 

    free = 0;
    used = 0;
    current_block = 0;
    blk_itr = fs->super->s_first_data_block;

    /// each group
    for (group = 0; group < fs->group_desc_count; group++) {

	gfree = 0;
	gused = 0;

	if (block_bitmap) {
	    ext2fs_get_block_bitmap_range(fs->block_map, blk_itr, block_nbytes << 3, block_bitmap);

	    /// each block in group
	    for (block = 0; block < fs->super->s_blocks_per_group; block++) {
		current_block = block + blk_itr;
		if (fs->super->s_feature_ro_compat & EXT4_FEATURE_RO_COMPAT_GDT_CSUM)
		    bg_flags = fs->group_desc[group].bg_flags;

		/// check block is used or not
		if ((!in_use (block_bitmap, block)) || (bg_flags&EXT2_BG_BLOCK_UNINIT)) {
		    free++;
		    gfree++;
		    bitmap[current_block] = 0;
		    log_mesg(3, 0, 0, debug, "free block %lu at group %i\n", (current_block), group);
		} else {
		    used++;
		    gused++;
		    bitmap[current_block] = 1;
		    log_mesg(3, 0, 0, debug, "used block %lu at group %i\n", (current_block), group);
		}
	    }
	    blk_itr += fs->super->s_blocks_per_group;
	}
	/// check free blocks in group
	if (gfree != fs->group_desc[group].bg_free_blocks_count)
	    log_mesg(0, 1, 1, debug, "bitmap erroe at %i group.\n", group);
    }
    /// check all free blocks in partition
    if (free != fs->super->s_free_blocks_count)
	log_mesg(0, 1, 1, debug, "bitmap free count err, free:%i\n", free);
    fs_close();
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{

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

