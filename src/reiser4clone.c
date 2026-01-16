/**
 * reiser4clone.c - part of Partclone project
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

#include <aal/types.h>
#include <reiser4/libreiser4.h>
#include <reiser4/factory.h>
#include <reiser4/types.h>

#include "partclone.h"
#include "reiser4clone.h"
#include "progress.h"
#include "fs_common.h"

#define MAX_REISER4_TOTAL_BLOCKS (1ULL << 40) // Max total blocks (approx 512TB @ 512B/block) to prevent DoS/memory exhaustion
#define MIN_REISER4_BLOCK_SIZE 512 // Smallest supported Reiser4 block size
#define MAX_REISER4_BLOCK_SIZE 65536 // Largest supported Reiser4 block size (64KB)

aal_device_t           *fs_device;
reiser4_fs_t           *fs = NULL;
reiser4_format_t       *format;

/// open device
static int fs_open(char* device){
    unsigned long long int state, extended;

    if (libreiser4_init()) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Can't initialize libreiser4.\n", __FILE__);
        return -1;
    }

    if (!(fs_device = aal_device_open(&file_ops, device, 512, O_RDONLY)))
    {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Cannot open the partition (%s).\n", __FILE__, device);
        return -1;
    }

    if (!(fs = reiser4_fs_open(fs_device, 0))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Can't open reiser4 on %s\n", __FILE__, device);
        aal_device_close(fs_device);
        return -1;
    }

    if (!(fs->journal = reiser4_journal_open(fs, fs_device))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Can't open journal on %s\n", __FILE__, device);
        reiser4_fs_close(fs);
        aal_device_close(fs_device);
        return -1;
    }

    state = get_ss_status(STATUS(fs->status));
    extended = get_ss_extended(STATUS(fs->status));
    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    }else{

        if (!state)
            log_mesg(1, 0, 0, fs_opt.debug, "%s: REISER4: FS marked consistent\n", __FILE__);

        if (state) 
            log_mesg(3, 0, 0, fs_opt.debug, "%s: REISER4: stat : %i\n", __FILE__, state);

        if (state != FS_OK) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);
        }

        if (extended)
            log_mesg(3, 0, 0, fs_opt.debug, "%s: Extended status: %0xllx\n", extended, __FILE__);

    }
    fs->format = reiser4_format_open(fs);
    if (!fs->format) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Can't open format on %s\n", __FILE__, device);
        reiser4_journal_close(fs->journal);
        reiser4_fs_close(fs);
        aal_device_close(fs_device);
        return -1;
    }
    return 0; // Success
}

/// close device
static void fs_close(){
    reiser4_fs_close(fs);
    aal_device_close(fs_device);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    reiser4_bitmap_t       *fs_bitmap = NULL;
    unsigned long long     bit, block, bused = 0, bfree = 0;
    int start = 0;
    int bit_size = 1;

    if (fs_open(device) != 0) {
        return; // Abort
    }

    // Initialize partclone's bitmap based on fs_info.totalblock (which is already validated in read_super_blocks)
    pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);

    unsigned long long total_blocks_from_format = reiser4_format_get_len(fs->format);
    if (total_blocks_from_format == 0 || total_blocks_from_format > MAX_REISER4_TOTAL_BLOCKS || total_blocks_from_format != fs_info.totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large, zero, or inconsistent total blocks from format: %llu. Expected: %llu\n",
                 total_blocks_from_format, fs_info.totalblock);
        fs_close();
        return;
    }

    fs_bitmap = reiser4_bitmap_create(total_blocks_from_format);
    if (!fs_bitmap) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to create Reiser4 bitmap.\n");
        fs_close();
        return;
    }

    if (reiser4_alloc_extract(fs->alloc, fs_bitmap) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to extract Reiser4 allocation bitmap.\n");
        free(fs_bitmap);
        fs_close();
        return;
    }

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);


    for(bit = 0; bit < total_blocks_from_format; bit++){
        block = bit ;
        if(reiser4_bitmap_test(fs_bitmap, bit)){
            bused++;
            pc_set_bit(block, bitmap, fs_info.totalblock);
            log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);
        } else {
            pc_clear_bit(block, bitmap, fs_info.totalblock);
            bfree++;
            log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %llu", __FILE__, block);
        }
        /// update progress
        update_pui(&prog, bit, bit, 0);

    }

    if(bfree != reiser4_format_get_free(fs->format))
        log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, bfree:%llu, sfree=%llu\n", __FILE__, bfree, reiser4_format_get_free(fs->format));

    free(fs_bitmap);
    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{
    reiser4_bitmap_t       *fs_bitmap = NULL;
    unsigned long long free_blocks=0;

    if (fs_open(device) != 0) {
        return; // Abort
    }
    fs_bitmap = reiser4_bitmap_create(reiser4_format_get_len(fs->format));
    reiser4_alloc_extract(fs->alloc, fs_bitmap);
    free_blocks = reiser4_format_get_free(fs->format);
    strncpy(fs_info->fs, reiser4_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = get_ms_blksize(SUPER(fs->master));

    if (fs_info->block_size < MIN_REISER4_BLOCK_SIZE || fs_info->block_size > MAX_REISER4_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %lu.\n", fs_info->block_size);
        fs_close();
        return;
    }

    fs_info->totalblock  = reiser4_format_get_len(fs->format);

    if (fs_info->totalblock == 0 || fs_info->totalblock > MAX_REISER4_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks detected in superblock: %llu. Max allowed: %llu\n", fs_info->totalblock, MAX_REISER4_TOTAL_BLOCKS);
        fs_close();
        return;
    }
    
    if (fs_info->block_size == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow or division by zero in device size calculation. totalblock: %llu, block_size: %lu\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->block_size * fs_info->totalblock;

    fs_info->usedblocks  = reiser4_format_get_len(fs->format) - free_blocks;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    free(fs_bitmap);
    fs_close();
}

