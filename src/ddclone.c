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

/// readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
	/// initial image bitmap as 1 (all block are used)
	pc_init_bitmap(bitmap, 0xFF, image_hdr.totalblock);
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
	int src;
	if ((src = open_source(device, &opt)) == -1) {
		log_mesg(0, 1, 1, opt.debug, "Error exit\n");
	}
	strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
	strncpy(image_hdr->fs, raw_MAGIC, FS_MAGIC_SIZE);
	image_hdr->block_size  = PART_SECTOR_SIZE;
	image_hdr->device_size = get_partition_size(&src);
	image_hdr->totalblock  = image_hdr->device_size / PART_SECTOR_SIZE;
	image_hdr->usedblocks  = image_hdr->device_size / PART_SECTOR_SIZE;
	close(src);
}
