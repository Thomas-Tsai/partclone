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
	}
	strncpy(fs_info->fs, raw_MAGIC, FS_MAGIC_SIZE);
	fs_info->block_size  = PART_SECTOR_SIZE;
	fs_info->device_size = get_partition_size(&src);
	fs_info->totalblock  = fs_info->device_size / PART_SECTOR_SIZE;
	fs_info->usedblocks  = fs_info->device_size / PART_SECTOR_SIZE;
	close(src);
}
