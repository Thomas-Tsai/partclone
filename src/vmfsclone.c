/**
 * vmfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read vmfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdio.h>
#include <vmfs/vmfs.h>

#include "partclone.h"
#include "reiser4clone.h"
#include "progress.h"
#include "fs_common.h"

vmfs_fs_t *fs;
vmfs_dir_t *root_dir;
char *EXECNAME = "partclone.vmfs";
extern fs_cmd_opt fs_opt;

/// open device
static void fs_open(char* device){
    vmfs_lvm_t *lvm;
    vmfs_flags_t flags;

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

    //log_mesg(3, 0, 0, fs_opt.debug, "%s: device %s\n", __FILE__, device);
    if (!(lvm = vmfs_lvm_create(flags))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to create LVM structure\n", __FILE__);
    }

    if (vmfs_lvm_add_extent(lvm, vmfs_vol_open(device, flags)) == -1) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open device/file \"%s\".\n", __FILE__, device);
    }

    if (!(fs = vmfs_fs_create(lvm))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open filesystem\n", __FILE__);
    }

    if (vmfs_fs_open(fs) == -1) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open volume.\n", __FILE__);
    }

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0)))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open root directory\n", __FILE__);
    }

}

/// close device
static void fs_close(){
    vmfs_dir_close(root_dir);
    vmfs_fs_close(fs);
}

/// readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char*bitmap, int pui)
{
    uint32_t current, used_block, free_block, err_block, total, alloc;
    int status = 0;
    int start = 0;
    int bit_size = 1;

    fs_open(device);
    /// init progress
    progress_bar   prog;        /// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, bit_size);

    total = fs->fbb->bmh.total_items;
    alloc = vmfs_bitmap_allocated_items(fs->fbb);

    for(current = 0; current < total; current++){
	status = vmfs_block_get_status(fs, VMFS_BLK_FB_BUILD(current));
	if (status == -1) {
	    err_block++;
	    bitmap[current] = 0;
	} else if (status == 1){
	    used_block++;
	    bitmap[current] = 1;
	} else if (status == 0){
	    free_block++;
	    bitmap[current] = 0;
	}

	log_mesg(2, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x status:", __FILE__, current, status);
	update_pui(&prog, current, 0);

    }
    update_pui(&prog, current, 1);
    fs_close();

    log_mesg(0, 0, 0, fs_opt.debug, "%s: Used:%u, Free:%u, Status err:%u\n", __FILE__, used_block, free_block, err_block);

}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{

    uint32_t alloc,total;

    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, vmfs_MAGIC, FS_MAGIC_SIZE);

    total = fs->fbb->bmh.total_items;
    alloc = vmfs_bitmap_allocated_items(fs->fbb);

    image_hdr->block_size  = vmfs_fs_get_blocksize(fs);
    image_hdr->totalblock  = total;
    image_hdr->usedblocks  = alloc;
    image_hdr->device_size = (vmfs_fs_get_blocksize(fs)*total);
    fs_close();
}

