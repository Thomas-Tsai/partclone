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

#define MAX_VMFS_TOTAL_BLOCKS (1ULL << 32) // Max total blocks (approx 4.3 billion) for VMFS total_items
#define MIN_VMFS_BLOCK_SIZE (1024 * 1024) // Smallest supported VMFS block size (1MB)
#define MAX_VMFS_BLOCK_SIZE (64 * 1024 * 1024) // Largest supported VMFS block size (64MB)

vmfs_fs_t *fs = NULL;
vmfs_dir_t *root_dir = NULL;

/// open device
static int fs_open(char* device){
#ifndef VMFS5_ZLA_BASE
    vmfs_lvm_t *lvm = NULL;
    vmfs_volume_t *vol = NULL;
#endif
    vmfs_flags_t flags;
    char *mdev[] = {device, NULL};

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: device %s\n", __FILE__, device);

#ifdef VMFS5_ZLA_BASE
    if (!(fs=vmfs_fs_open(mdev, flags))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to open volume (VMFS5_ZLA_BASE).\n", __FILE__);
        return -1;
    }
#else
    if (!(lvm = vmfs_lvm_create(flags))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to create LVM structure.\n", __FILE__);
        return -1;
    }

    if (!(vol = vmfs_vol_open(device, flags))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to open device/file \"%s\".\n", __FILE__, device);
        vmfs_lvm_destroy(lvm);
        return -1;
    }

    if (vmfs_lvm_add_extent(lvm, vol) == -1) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to add extent to LVM for \"%s\".\n", __FILE__, device);
        vmfs_vol_close(vol);
        vmfs_lvm_destroy(lvm);
        return -1;
    }

    if (!(fs = vmfs_fs_create(lvm))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to create filesystem from LVM.\n", __FILE__);
        // vmfs_lvm_destroy(lvm) implicitly handles vol closure
        return -1;
    }

    if (vmfs_fs_open(fs) == -1) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to open filesystem volume.\n", __FILE__);
        vmfs_fs_destroy(fs);
        fs = NULL;
        return -1;
    }
#endif
    // vmfs_lvm_destroy(lvm) is implicitly called when vmfs_fs_destroy(fs) is called via vmfs_fs_close(fs) in fs_close.
    // vol is implicitly closed/handled by vmfs_lvm_destroy.

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0,0)))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to open root directory.\n", __FILE__);
        vmfs_fs_close(fs);
        fs = NULL;
        return -1;
    }
    return 0; // Success
}
/// close device
static void fs_close(){
    if (root_dir) {
        vmfs_dir_close(root_dir);
        root_dir = NULL;
    }
    if (fs) {
        vmfs_fs_close(fs);
        fs = NULL;
    }
}

/// calculate offset of logical volume
static uint32_t logical_volume_offset(vmfs_fs_t *fs)
{
    /* reference vmfs-tools/libvmfs/vmfs_volume.c 
       pos += vol->vmfs_base + 0x1000000;
     */
    return (VMFS_VOLINFO_BASE + 0x1000000) / vmfs_fs_get_blocksize(fs);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    uint32_t offset;
    unsigned long long total_blocks_to_process;
    uint32_t current;
    uint32_t used_block = 0, free_block = 0, err_block = 0;
    int status = 0;
    int start = 0;
    int bit_size = 1;

    if (fs_open(device) != 0) {
        return;
    }

    // Initialize bitmap based on fs_info.totalblock (which is already validated in read_super_blocks)
    pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);

    /// init progress
    extern progress_bar   prog;        /// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    offset = logical_volume_offset(fs);

    // Validate offset against fs_info.totalblock
    if ((unsigned long long)offset > fs_info.totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Logical volume offset %u exceeds total blocks %llu.\n", offset, fs_info.totalblock);
        fs_close();
        return;
    }

    total_blocks_to_process = fs_info.totalblock; // Use the validated totalblock from fs_info

    // containing the volume information and the LVM information
    for(current = 0; current < offset; current++){
        // Check current against fs_info.totalblock to prevent OOB
        if ((unsigned long long)current >= fs_info.totalblock) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Block index %u exceeds total filesystem blocks %llu during initial offset processing.\n", current, fs_info.totalblock);
            fs_close();
            return;
        }
        pc_set_bit(current, bitmap, fs_info.totalblock);
        used_block++;
        update_pui(&prog, current, current, 0);
    }

    // the logical volume
    for(current = offset; (unsigned long long)current < total_blocks_to_process; current++){
        status = vmfs_block_get_status(fs, VMFS_BLK_FB_BUILD(current-offset,0));
	if (status == -1) {
	    err_block++;
	    pc_clear_bit(current, bitmap, fs_info.totalblock);
            log_mesg(2, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x status: ERROR\n", __FILE__, current);
	} else if (status == 1){
	    used_block++;
	    pc_set_bit(current, bitmap, fs_info.totalblock);
            log_mesg(2, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x status: USED\n", __FILE__, current);
	} else if (status == 0){
	    free_block++;
	    pc_clear_bit(current, bitmap, fs_info.totalblock);
            log_mesg(2, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x status: FREE\n", __FILE__, current);
	} else {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unknown block status %d for Block 0x%8.8x.\n", __FILE__, status, current);
            fs_close();
            return;
        }

	update_pui(&prog, current, current, 0);

    }
    fs_close();
    update_pui(&prog, 1, 1, 1);

    log_mesg(0, 0, 0, fs_opt.debug, "%s: Used:%"PRIu32", Free:%"PRIu32", Status err:%"PRIu32"\n", __FILE__, used_block, free_block, err_block);

}
void read_super_blocks(char* device, file_system_info* fs_info)
{
    uint32_t offset, total_items_from_lib, alloc;
    unsigned long long calculated_totalblock;

    if (fs_open(device) != 0) {
        return;
    }
    strncpy(fs_info->fs, vmfs_MAGIC, FS_MAGIC_SIZE);
    offset = logical_volume_offset(fs);
    total_items_from_lib = fs->fbb->bmh.total_items;
    
    // Validate total_items_from_lib
    if (total_items_from_lib == 0 || (unsigned long long)total_items_from_lib > MAX_VMFS_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total items from libvmfs: %u. Max allowed: %llu\n", total_items_from_lib, MAX_VMFS_TOTAL_BLOCKS);
        fs_close();
        return;
    }

    if (offset > (UINT32_MAX - total_items_from_lib)) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Overflow in totalblock calculation (total_items_from_lib + offset).\n");
        fs_close();
        return;
    }
    calculated_totalblock = (unsigned long long)total_items_from_lib + offset;

    if (calculated_totalblock > MAX_VMFS_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Calculated total blocks %llu exceeds MAX_VMFS_TOTAL_BLOCKS %llu.\n", calculated_totalblock, MAX_VMFS_TOTAL_BLOCKS);
        fs_close();
        return;
    }
    fs_info->totalblock  = calculated_totalblock;

    alloc = vmfs_bitmap_allocated_items(fs->fbb) + offset; // Needs validation too

    // Validate block_size
    fs_info->block_size  = vmfs_fs_get_blocksize(fs);
    if (fs_info->block_size < MIN_VMFS_BLOCK_SIZE || fs_info->block_size > MAX_VMFS_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %u.\n", fs_info->block_size);
        fs_close();
        return;
    }
    
    // Ensure fs_info->device_size consistency with totalblock * block_size, with overflow check
    if (fs_info->block_size == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow or division by zero in device size calculation. totalblock: %llu, block_size: %u\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->totalblock * fs_info->block_size;

    // Validate alloc as well
    if (alloc > fs_info->totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Allocated blocks (%u) exceeds total blocks (%llu).\n", alloc, fs_info->totalblock);
        fs_close();
        return;
    }
    fs_info->usedblocks  = alloc;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_close();
}
