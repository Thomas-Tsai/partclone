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
#include <inttypes.h>
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
#ifndef VMFS5_ZLA_BASE
    vmfs_lvm_t *lvm;
#endif
    vmfs_flags_t flags;
    char *mdev[] = {device, NULL};

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: device %s\n", __FILE__, device);

#ifdef VMFS5_ZLA_BASE
    if (!(fs=vmfs_fs_open(mdev, flags))) {
#else
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
#endif
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open volume.\n", __FILE__);
    }

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0,0)))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open root directory\n", __FILE__);
    }

}

/// close device
static void fs_close(){
    vmfs_dir_close(root_dir);
    vmfs_fs_close(fs);
}

/// calculate offset of logical volume
static uint32_t logical_volume_offset(vmfs_fs_t *fs)
{
    /* reference vmfs-tools/libvmfs/vmfs_volume.c 
       pos += vol->vmfs_base + 0x1000000;
     */
    return (VMFS_VOLINFO_BASE + 0x1000000) / vmfs_fs_get_blocksize(fs);
}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    uint32_t offset, total, current;
    uint32_t used_block = 0, free_block = 0, err_block = 0;
    int status = 0;
    int start = 0;
    int bit_size = 1;

    fs_open(device);
    /// init progress
    progress_bar   prog;        /// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    offset = logical_volume_offset(fs);
    total = fs->fbb->bmh.total_items + offset;

    // containing the volume information and the LVM information
    for(current = 0; current < offset; current++){
        pc_set_bit(current, bitmap);
        used_block++;
        update_pui(&prog, current, current, 0);
    }

    // the logical volume
    for(current = offset; current < total; current++){
	status = vmfs_block_get_status(fs, VMFS_BLK_FB_BUILD(current-offset,0));
	if (status == -1) {
	    err_block++;
	    pc_clear_bit(current, bitmap);
	} else if (status == 1){
	    used_block++;
	    pc_set_bit(current, bitmap);
	} else if (status == 0){
	    free_block++;
	    pc_clear_bit(current, bitmap);
	}

	log_mesg(2, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x status: %i\n", __FILE__, current, status);
	update_pui(&prog, current, current, 0);

    }
    fs_close();
    update_pui(&prog, 1, 1, 1);

    log_mesg(0, 0, 0, fs_opt.debug, "%s: Used:%"PRIu32", Free:%"PRIu32", Status err:%"PRIu32"\n", __FILE__, used_block, free_block, err_block);

}

void initial_image_hdr(char* device, image_head* image_hdr)
{
    uint32_t offset, total, alloc;

    fs_open(device);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, vmfs_MAGIC, FS_MAGIC_SIZE);
    offset = logical_volume_offset(fs);
    total = fs->fbb->bmh.total_items + offset;
    alloc = vmfs_bitmap_allocated_items(fs->fbb) + offset;

    image_hdr->block_size  = vmfs_fs_get_blocksize(fs);
    image_hdr->totalblock  = total;
    image_hdr->usedblocks  = alloc;
    image_hdr->device_size = (vmfs_fs_get_blocksize(fs)*total);
    fs_close();
}

