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


aal_device_t           *fs_device;
reiser4_fs_t           *fs = NULL;
reiser4_format_t       *format;
char *EXECNAME = "clone.reiser4";

/// open device
static void fs_open(char* device){
    int debug=1;

    if (libreiser4_init()) {
            log_mesg(0, 1, 1, debug, "Can't initialize libreiser4.\n");
    }

    if (!(fs_device = aal_device_open(&file_ops, device, 512, O_RDONLY)))
    {
            log_mesg(0, 1, 1, debug, "Cannot open the partition (%s).\n", device);
    }

    if (!(fs = reiser4_fs_open(fs_device, 0))) {
            log_mesg(0, 1, 1, debug, "Can't open reiser4 on %s", device);
    }

   //reiser4_opset_profile(fs->tree->ent.opset);

   if (!(fs->journal = reiser4_journal_open(fs, fs_device))) {
           log_mesg(0, 1, 1, debug, "Can't open journal on %s", device);
   }
   
   //reiser4_opset_profile(fs->tree->ent.opset);
   fs->format = reiser4_format_open(fs);
}

/// close device
static void fs_close(){
    reiser4_fs_close(fs);
    aal_device_close(fs_device);
}

/// readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char*bitmap)
{
    reiser4_bitmap_t       *fs_bitmap;
    unsigned long long     bit, block, bused = 0, bfree = 0;
    int                    debug;
    
    fs_open(device);
    fs_bitmap = reiser4_bitmap_create(reiser4_format_get_len(fs->format));
    reiser4_alloc_extract(fs->alloc, fs_bitmap);

    for(bit = 0; bit < reiser4_format_get_len(fs->format); bit++){
	block = bit ;
        if(reiser4_bitmap_test(fs_bitmap, bit)){
            bused++;
	    bitmap[block] = 1;
        } else {
	    bitmap[block] = 0;
	    bfree++;
        }
    }

    if(bfree != reiser4_format_get_free(fs->format))
	log_mesg(0, 1, 1, debug, "bitmap free count err, bfree:%lli, sfree=%lli\n", bfree, reiser4_format_get_free(fs->format));

    fs_close();
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    int                    debug=1;
    reiser4_bitmap_t       *fs_bitmap;
    unsigned long long free_blocks=0;
    
    fs_open(device);
    fs_bitmap = reiser4_bitmap_create(reiser4_format_get_len(fs->format));
    reiser4_alloc_extract(fs->alloc, fs_bitmap);
    free_blocks = reiser4_format_get_free(fs->format);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, reiser4_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = get_ms_blksize(SUPER(fs->master));
    image_hdr->totalblock = reiser4_format_get_len(fs->format);
    image_hdr->usedblocks = reiser4_format_get_len(fs->format) - free_blocks;
    image_hdr->device_size = (get_ms_blksize(SUPER(fs->master))*reiser4_format_get_len(fs->format));
    fs_close();
}

