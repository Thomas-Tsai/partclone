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

aal_device_t           *fs_device;
reiser4_fs_t           *fs = NULL;
reiser4_format_t       *format;
char *EXECNAME = "partclone.reiser4";
extern fs_cmd_opt fs_opt;

/// open device
static void fs_open(char* device){
    unsigned long long int state, extended;

    if (libreiser4_init()) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Can't initialize libreiser4.\n", __FILE__);
    }

    if (!(fs_device = aal_device_open(&file_ops, device, 512, O_RDONLY)))
    {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Cannot open the partition (%s).\n", __FILE__, device);
    }

    if (!(fs = reiser4_fs_open(fs_device, 0))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Can't open reiser4 on %s\n", __FILE__, device);
    }

    //reiser4_opset_profile(fs->tree->ent.opset);

    if (!(fs->journal = reiser4_journal_open(fs, fs_device))) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Can't open journal on %s", __FILE__, device);
    }

    state = get_ss_status(STATUS(fs->status));
    extended = get_ss_extended(STATUS(fs->status));
    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    }else{

        if (!state)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: REISER4 can't get status\n", __FILE__);

        if (state) 
            log_mesg(3, 0, 0, fs_opt.debug, "%s: REISER4 stat : %i\n", __FILE__, state);

        if (state != FS_OK)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);

        if (extended)
            log_mesg(3, 0, 0, fs_opt.debug, "%s: Extended status: %0xllx\n", extended, __FILE__);

    }
    //reiser4_opset_profile(fs->tree->ent.opset);
    fs->format = reiser4_format_open(fs);
}

/// close device
static void fs_close(){
    reiser4_fs_close(fs);
    aal_device_close(fs_device);
}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    reiser4_bitmap_t       *fs_bitmap;
    unsigned long long     bit, block, bused = 0, bfree = 0;
    int start = 0;
    int bit_size = 1;

    fs_open(device);
    fs_bitmap = reiser4_bitmap_create(reiser4_format_get_len(fs->format));
    reiser4_alloc_extract(fs->alloc, fs_bitmap);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);


    for(bit = 0; bit < reiser4_format_get_len(fs->format); bit++){
        block = bit ;
        if(reiser4_bitmap_test(fs_bitmap, bit)){
            bused++;
            pc_set_bit(block, bitmap);
            log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %llu", __FILE__, block);
        } else {
            pc_clear_bit(block, bitmap);
            bfree++;
            log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %llu", __FILE__, block);
        }
        /// update progress
        update_pui(&prog, bit, bit, 0);

    }

    if(bfree != reiser4_format_get_free(fs->format))
        log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap free count err, bfree:%llu, sfree=%llu\n", __FILE__, bfree, reiser4_format_get_free(fs->format));

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

void initial_image_hdr(char* device, image_head* image_hdr)
{
    reiser4_bitmap_t       *fs_bitmap;
    unsigned long long free_blocks=0;

    fs_open(device);
    fs_bitmap = reiser4_bitmap_create(reiser4_format_get_len(fs->format));
    reiser4_alloc_extract(fs->alloc, fs_bitmap);
    free_blocks = reiser4_format_get_free(fs->format);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, reiser4_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = (int)get_ms_blksize(SUPER(fs->master));
    image_hdr->totalblock = (unsigned long long)reiser4_format_get_len(fs->format);
    image_hdr->usedblocks = (unsigned long long)(reiser4_format_get_len(fs->format) - free_blocks);
    image_hdr->device_size =(unsigned long long)(image_hdr->block_size * image_hdr->totalblock);
    fs_close();
}

