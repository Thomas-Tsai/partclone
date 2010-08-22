/**
 * btrfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read btrfs super block and extent
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdio.h>
#include "btrfs/version.h"
#include "btrfs/ctree.h"
#include "btrfs/disk-io.h"

#include "partclone.h"
#include "btrfsclone.h"
#include "progress.h"
#include "fs_common.h"

char *EXECNAME = "partclone.btrfs";
extern fs_cmd_opt fs_opt;

struct btrfs_root *root;
struct btrfs_path *path;
int block_size = 0;

///set useb block
static void set_bitmap(char* bitmap, uint64_t pos, uint64_t length){
    uint64_t block;
    uint64_t pos_block;
    uint64_t count;

    pos_block = pos/block_size;
    count = length/block_size;

    for(block = pos_block; block < (pos_block+count); block++){
	bitmap[block] = 1;
	log_mesg(3, 0, 0, fs_opt.debug, "block %i is used\n", block);
    }
}

/// open device
static void fs_open(char* device){
    radix_tree_init();
    root = open_ctree(device, 0, 0);
    if(!root){
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open %s\n", __FILE__, device);	
    }
    path = btrfs_alloc_path();

}

/// close device
static void fs_close(){
    int ret = 0;
    btrfs_free_path(path);
    ret = close_ctree(root);

}

/// readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char*bitmap, int pui)
{
    uint64_t super_block = 0;
    struct btrfs_root *extent_root;
    struct extent_buffer *leaf;
    struct btrfs_key key;
    uint64_t bytenr;
    uint64_t num_bytes;
    int ret;


    fs_open(device);
    block_size = image_hdr.block_size;
    //initial all block as free
    memset(bitmap, 0, image_hdr.totalblock);

    // set super block as used
    super_block = BTRFS_SUPER_INFO_OFFSET / block_size;
    bitmap[super_block] = 1;

    extent_root = root->fs_info->extent_root;
    bytenr = BTRFS_SUPER_INFO_OFFSET + 4096;
    key.objectid = BTRFS_EXTENT_ITEM_KEY;
    key.offset = 0;

    ret = btrfs_search_slot(NULL, extent_root, &key, path, 0, 0);
    while(1){
	leaf = path->nodes[0];
	if (path->slots[0] >= btrfs_header_nritems(leaf)){
	    ret = btrfs_next_leaf(extent_root, path);
	    if(ret > 0)
		break;
	    leaf = path->nodes[0];
	}

	btrfs_item_key_to_cpu(leaf, &key, path->slots[0]);
	if(key.objectid < bytenr || key.type != BTRFS_EXTENT_ITEM_KEY){
	    path->slots[0]++;
	    continue;
	}

	bytenr = key.objectid;
	num_bytes = key.offset;
	set_bitmap(bitmap, bytenr, num_bytes);
	bytenr+=num_bytes;
    }
    fs_close();
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{    
    fs_open(device);

    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, btrfs_MAGIC, FS_MAGIC_SIZE);

    image_hdr->block_size  = btrfs_super_nodesize(&root->fs_info->super_copy);
    image_hdr->usedblocks  = (btrfs_super_bytes_used(&root->fs_info->super_copy)/image_hdr->block_size);
    image_hdr->device_size = btrfs_super_total_bytes(&root->fs_info->super_copy);
    image_hdr->totalblock  = (uint64_t)(image_hdr->device_size/image_hdr->block_size);
    log_mesg(0, 0, 0, fs_opt.debug, "block_size = %i\n", image_hdr->block_size);
    log_mesg(0, 0, 0, fs_opt.debug, "usedblock = %i\n", image_hdr->usedblocks);
    log_mesg(0, 0, 0, fs_opt.debug, "device_size = %i\n", image_hdr->device_size);
    log_mesg(0, 0, 0, fs_opt.debug, "totalblock = %i\n", image_hdr->totalblock);

    fs_close();
}

