/**
 * ddclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 * Copyright (c) 2013~ Raman Shishnew <rommer at active by>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <string.h>
#include <unistd.h>
#include "partclone.h"

#define MAX_DD_TOTAL_BLOCKS (1ULL << 50) // Max total blocks (approx 1 Exabyte @ 512B/block) to prevent memory exhaustion

extern cmd_opt opt;

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
	/// initial image bitmap as 1 (all block are used)
	pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{
	int src;
	if ((src = open_source(device, &opt)) == -1) {
	    log_mesg(0, 1, 1, opt.debug, "Error exit\n");
	    return;
	}
	strncpy(fs_info->fs, raw_MAGIC, FS_MAGIC_SIZE);
	fs_info->block_size  = PART_SECTOR_SIZE; // Always 512 bytes for ddclone

	unsigned long long initial_device_size = get_partition_size(&src); // Get size first
	if (opt.device_size > 0){
	    fs_info->device_size = opt.device_size;
	    log_mesg(2, 0, 0, opt.debug, "define device size from option -S  %llu\n", opt.device_size);
	} else {
	    fs_info->device_size = initial_device_size;
	}

	log_mesg(2, 0, 0, opt.debug, "test device size %llu\n", fs_info->device_size);

    if (fs_info->device_size == 0) {
        log_mesg(0, 1, 1, opt.debug, "ERROR: Device size is zero.\n");
        close(src);
        return;
    }

    if (PART_SECTOR_SIZE == 0) {
        log_mesg(0, 1, 1, opt.debug, "ERROR: PART_SECTOR_SIZE is zero, cannot calculate total blocks.\n");
        close(src);
        return;
    }
    fs_info->totalblock  = fs_info->device_size / PART_SECTOR_SIZE;

    if (fs_info->totalblock == 0 || fs_info->totalblock > MAX_DD_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, opt.debug, "ERROR: Maliciously large or zero total blocks detected: %llu. Max allowed: %llu\n", fs_info->totalblock, MAX_DD_TOTAL_BLOCKS);
        close(src);
        return;
    }

	fs_info->usedblocks  = fs_info->totalblock; // All blocks are considered used for raw clone
	fs_info->superBlockUsedBlocks = fs_info->usedblocks;
	close(src);
}
