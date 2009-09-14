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

#include "ufs/ufs/dinode.h"
#include "ufs/ffs/fs.h"
#include "ufs/sys/disklabel.h"
#include "ufs/libufs.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
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
int debug = 3;

/// open device
static void fs_open(char* device){

    int fsflags;

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
            log_mesg(3, 0, 0, debug, "UFS size = %lld\n", afs.fs_size);
            log_mesg(3, 0, 0, debug, "Blocksize fs_fsize = %i\n", afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "partition size = %lld\n", afs.fs_size*afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "block per group %i\n", afs.fs_fpg);

            break;
        case 1:
            log_mesg(3, 0, 0, debug, "magic = %x (UFS1)\n", afs.fs_magic);
            log_mesg(3, 0, 0, debug, "superblock location = %lld\nid = [ %x %x ]\n", afs.fs_sblockloc, afs.fs_id[0], afs.fs_id[1]);
            log_mesg(3, 0, 0, debug, "group (ncg) = %d\n", afs.fs_ncg);
            log_mesg(3, 0, 0, debug, "UFS size = %lld\n", afs.fs_size);
            log_mesg(3, 0, 0, debug, "Blocksize fs_fsize = %i\n", afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "partition size = %lld\n", afs.fs_old_size*afs.fs_fsize);
            log_mesg(3, 0, 0, debug, "block per group %i\n", afs.fs_fpg);

            break;
        default:
            log_mesg(3, 0, 0, debug, "disk.d_ufs = %c", disk.d_ufs);
            break;
    }

    /// get ufs flags
    if (afs.fs_old_flags & FS_FLAGS_UPDATED)
	fsflags == afs.fs_flags;
    else
	fsflags == afs.fs_old_flags;

    /// check ufs status
    if (fsflags & FS_UNCLEAN)
	log_mesg(0, 1, 1, debug, "UFS flag FS_UNCLEAN\n\n");
    if (fsflags & FS_NEEDSFSCK)
	log_mesg(0, 1, 1, debug, "UFS flag: need fsck run first\n\n");

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
    unsigned long long     total_block, block, bused = 0, bfree = 0;
    int                    done = 0, i = 0, start = 0, bit_size = 1;
    char* p;


    fs_open(device);

    /// init progress
    progress_bar   bprog;	/// progress_bar structure defined in progress.h
    progress_init(&bprog, start, image_hdr.totalblock, bit_size);

    total_block = 0;
    /// read group
    while ((i = cgread(&disk)) != 0) {
        log_mesg(3, 0, 0, debug, "\ncg = %d\n", disk.d_lcg);
        log_mesg(3, 0, 0, debug, "blocks = %i\n", acg.cg_ndblk);
        p = cg_blksfree(&acg);

        for (block = 0; block < acg.cg_ndblk; block++){
            if (isset(p, block)) {
                bitmap[total_block] = 0;
                bfree++;
                log_mesg(3, 0, 0, debug, "bitmap is free %lli, global %lli\n", block, total_block);
            } else {
                bitmap[total_block] = 1;
		bused++;
                log_mesg(3, 0, 0, debug, "bitmap is used %lli, global %lli\n", block, total_block);
            }
	    total_block++;
            update_pui(&bprog, total_block ,done);
        }
        log_mesg(3, 0, 0, debug, "\n");

    }

    fs_close();
    
    log_mesg(3, 0, 0, debug, "total used = %lli, total free = %lli\n", bused, bfree);
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

    switch (disk.d_ufs) {
        case 2:
	    image_hdr->block_size  = afs.fs_fsize;
	    image_hdr->totalblock  = afs.fs_size;
	    image_hdr->device_size = afs.fs_fsize*afs.fs_size;
            break;
        case 1:
	    image_hdr->block_size  = afs.fs_fsize;
	    image_hdr->totalblock  = afs.fs_old_size;
	    image_hdr->device_size = afs.fs_fsize*afs.fs_old_size;
            break;
        default:
            break;
    }

    image_hdr->usedblocks  = get_used_block();
    fs_close();
}

/// get_used_block - get FAT used blocks
static unsigned long long get_used_block()
{
    unsigned long long     block, bused = 0, bfree = 0;
    int                    i = 0, start = 0, bit_size = 1;
    char		   *p;


    /// read group
    while ((i = cgread(&disk)) != 0) {
        log_mesg(3, 0, 0, debug, "\ncg = %d\n", disk.d_lcg);
        log_mesg(3, 0, 0, debug, "blocks = %i\n", acg.cg_ndblk);
        p = cg_blksfree(&acg);

        for (block = 0; block < acg.cg_ndblk; block++){
            if (isset(p, block)) {
                bfree++;
            } else {
		bused++;
            }
        }

    }
    log_mesg(3, 0, 0, debug, "total used = %lli, total free = %lli\n", bused, bfree);
    return bused;
}
