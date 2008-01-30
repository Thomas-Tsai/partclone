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

    if (use_superblock && !use_blocksize)
    	use_blocksize = 1024;
    flags = EXT2_FLAG_JOURNAL_DEV_OK;

    retval = ext2fs_open (device, flags, use_superblock, use_blocksize, unix_io_manager, &fs);
    if (retval) 
        log_mesg(0, 1, 1, debug, "Couldn't find valid filesystem superblock.\n");
    
}

/// close device
static void fs_close(){
    ext2fs_close (fs);
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
    unsigned long long offset, current_block, block;
    unsigned long long free, used, gfree, gused;
    char *block_bitmap=NULL;
    int debug = 1;

    log_mesg(2, 0, 0, debug, "readbitmap %i\n",bitmap);

    fs_open(device);

    retval = ext2fs_read_bitmaps (fs); /// open extfs bitmap
    if (retval)
        log_mesg(0, 1, 1, debug, "Couldn't find valid filesystem bitmap.\n");

    if (fs->block_map)
        block_bitmap = fs->block_map->bitmap; /// read extfs bitmap
    
    /// initial image bitmap as 1 (all block are used)
    for(block = 0; block < image_hdr.totalblock; block++)
	bitmap[block] = 1; 

    free = 0;
    used = fs->super->s_first_data_block; /// for first data block not zero(small partition) 
    current_block = 0;

    /// each group
    for (group = 0; group < fs->group_desc_count; group++) {
	offset = fs->super->s_first_data_block;
	//printf("offset %i\n", offset);
	gfree = 0;
	gused = 0;
	
        if (block_bitmap) {
            offset += group * fs->super->s_blocks_per_group;

	    /// each block in group
            for (block = 0; ((block < fs->super->s_blocks_per_group) && (current_block < (image_hdr.totalblock-1))); block++) {
		current_block = block + offset;

		/// check block is used or not
                if (!in_use (block_bitmap, block)) {
                    free++;
		    gfree++;
                    bitmap[current_block] = 0;
		    log_mesg(3, 0, 0, 1, "free block %lu at group %i\n", (current_block), group);
                } else {
                    used++;
		    gused++;
                    bitmap[current_block] = 1;
		    log_mesg(3, 0, 0, 1, "used block %lu at group %i\n", (current_block), group);
                }
            }
        block_bitmap += fs->super->s_blocks_per_group / 8; /// update extfs bitmap
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
    image_hdr->device_size = (unsigned long long)device_size(device);
    image_hdr->totalblock = (unsigned long long)block_count();
    image_hdr->usedblocks = (unsigned long long)get_used_blocks();
    fs_close();
}

