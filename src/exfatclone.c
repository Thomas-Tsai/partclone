/**
 * exfatclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read exfat super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "exfat/exfat.h"

#include "partclone.h"
#include "exfatclone.h"
#include "progress.h"
#include "fs_common.h"

#define EXFAT_SECTOR_SIZE(sb) (1 << (sb).sector_bits)

char *EXECNAME = "partclone.exfat";

struct exfat ef;

/// open device
static void fs_open(char* device){
    log_mesg(2, 0, 0, fs_opt.debug, "%s: exfat_mount\n", __FILE__); 
    if (exfat_mount(&ef, device, "ro") != 0)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: File system exfat open fail\n", __FILE__);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: exfat_mount done\n", __FILE__);
}

/// close device
static void fs_close(){

    log_mesg(2, 0, 0, fs_opt.debug, "%s: exfat_umount\n", __FILE__); 
    exfat_unmount(&ef);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: exfat_umount done\n", __FILE__);
}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    off_t a = 0, b = 0;
    off_t block = 0;;
    int start = 0;
    int bit_size = 1;

    pc_init_bitmap(bitmap, 0x00, image_hdr.totalblock);

    fs_open(device);
    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);
    pc_init_bitmap(bitmap, 0x00, image_hdr.totalblock);
    while (exfat_find_used_sectors(&ef, &a, &b) == 0){
	printf("block %li %li \n", a, b);
	for (block = a; block <= b; block++){
	    pc_set_bit((uint64_t)block, bitmap);
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: used block %li \n", __FILE__, block);
	    /// update progress
	    update_pui(&prog, block, block, 0);
	}
    }

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

void initial_image_hdr(char* device, image_head* image_hdr)
{
    struct exfat_super_block* sb;
    uint64_t free_sectors, free_clusters;

    fs_open(device);
    free_clusters = exfat_count_free_clusters(&ef);
    free_sectors = (uint64_t) free_clusters << ef.sb->spc_bits;
    sb = ef.sb;

    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, exfat_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = EXFAT_SECTOR_SIZE(*sb);
    image_hdr->totalblock  = le64_to_cpu(sb->sector_count);
    image_hdr->usedblocks  = (le64_to_cpu(sb->sector_count) - free_sectors);
    image_hdr->device_size = (image_hdr->totalblock*image_hdr->block_size);
    fs_close();
}

