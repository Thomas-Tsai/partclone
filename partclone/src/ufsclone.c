/**
 * ufsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read ufs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/disklabel.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <libufs.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define afs     disk.d_fs
#define acg     disk.d_cg
struct uufsd disk;

#include "partclone.h"
#include "ufsclone.h"
#include "progress.h"

char *EXECNAME = "partclone.ufs";

/// open device
static void fs_open(char* device){
    int debug = 3;

    log_mesg(3, 0, 0, debug, "UFS partition Open\n");
    if (ufs_disk_fillout(&disk, device) == -1){
        log_mesg(3, 0, 0, debug, "UFS open fail\n");
    }
    log_mesg(3, 0, 0, debug, "UFS open well\n");

    switch (disk.d_ufs) {
        case 2:
            log_mesg(3, 0, 0, debug, "magic = %x (UFS2)\n", afs.fs_magic);
            log_mesg(3, 0, 0, debug, "superblock location = %lld\nid = [ %x %x ]\n", afs.fs_sblockloc, afs.fs_id[0], afs.fs_id[1]);
            log_mesg(3, 0, 0, debug, "group (ncg) = %d\n", afs.fs_ncg);
            log_mesg(3, 0, 0, debug, "UFS size (tblocks) = %lld\n", afs.fs_dsize);
            log_mesg(3, 0, 0, debug, "Blocksize fs_fsize = %i\n", afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "partition size = %lld\n", afs.fs_dsize*afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "block per group %i\n", afs.fs_fpg);
            break;
        case 1:
            log_mesg(3, 0, 0, debug, "magic = %x (UFS1)\n", afs.fs_magic);
            log_mesg(3, 0, 0, debug, "id = [ %x %x ]\n", afs.fs_id[0], afs.fs_id[1]);
            break;
        default:
            log_mesg(3, 0, 0, debug, "disk.d_ufs = %c", disk.d_ufs);
            break;
    }

}

/// close device
static void fs_close(){
    log_mesg(3, 0, 0, debug, "close\n");
    ufs_disk_close(&disk);
    log_mesg(3, 0, 0, debug, "done\n\n");
}

/// readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui)
{
    unsigned long long     block, bused = 0, bfree = 0;
    int                    debug = 3, done = 0;

    fs_open(device);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, bit_size);

    /// read group
    while ((i = cgread(&disk)) != 0) {
        log_mesg(3, 0, 0, debug, "\ncg = %d\n", disk.d_lcg);
        log_mesg(3, 0, 0, debug, "blocks = %i\n", acg.cg_ndblk);
        p = cg_blksfree(&acg);

        for (block = 0; block < afs.fs_fpg; block++){
            if (isset(p, block)) {
                bitmap[block] = 1;
                log_mesg(3, 0, 0, debug, "bitmap is used %lli", block);
            } else {
                bitmap[block] = 0;
                bfree++;
                log_mesg(3, 0, 0, debug, "bitmap is free %lli", block);
            }
            update_pui(&bprog, block ,done);
        }
        log_mesg(3, 0, 0, debug, "\n");

    }

    fs_close();
    
    done = 1;
    update_pui(&bprog, 1, done);

}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    int                    debug=1;

    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, ufs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = afs.fs_fsize;
    image_hdr->totalblock  = afs.fs_dsize;
    image_hdr->usedblocks  = 0;
    image_hdr->device_size = afs.fs_fsize*afs.fs_dsize;
    fs_close();
}

