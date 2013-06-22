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

#include <string.h>
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
#include "fs_common.h"

char *EXECNAME = "partclone.ufs";
extern fs_cmd_opt fs_opt;

/// get_used_block - get FAT used blocks
static unsigned long long get_used_block()
{
    unsigned long long     block, bused = 0, bfree = 0;
    int                    i = 0;
    unsigned char	   *p;


    /// read group
    while ((i = cgread(&disk)) != 0) {
        log_mesg(2, 0, 0, fs_opt.debug, "%s: \ncg = %d\n", __FILE__, disk.d_lcg);
        log_mesg(2, 0, 0, fs_opt.debug, "%s: blocks = %i\n", __FILE__, acg.cg_ndblk);
        p = cg_blksfree(&acg);

        for (block = 0; block < acg.cg_ndblk; block++){
            if (isset(p, block)) {
                bfree++;
            } else {
                bused++;
            }
        }

    }
    log_mesg(1, 0, 0, fs_opt.debug, "%s: total used = %lli, total free = %lli\n", __FILE__, bused, bfree);
    return bused;
}
/// open device
static void fs_open(char* device){

    int fsflags = 0;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: UFS partition Open\n", __FILE__);
    if (ufs_disk_fillout(&disk, device) == -1){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: UFS open fail\n", __FILE__);
    } else {
	log_mesg(0, 0, 0, fs_opt.debug, "%s: UFS open well\n", __FILE__);
    }

    switch (disk.d_ufs) {
        case 2:
            log_mesg(1, 0, 0, fs_opt.debug, "%s: magic = %x (UFS2)\n", __FILE__, afs.fs_magic);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: superblock location = %lld\nid = [ %x %x ]\n", __FILE__, afs.fs_sblockloc, afs.fs_id[0], afs.fs_id[1]);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: group (ncg) = %d\n", __FILE__, afs.fs_ncg);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: UFS size = %lld\n", __FILE__, afs.fs_size);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: Blocksize fs_fsize = %i\n", __FILE__, afs.fs_fsize);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: partition size = %lld\n", __FILE__, afs.fs_size*afs.fs_fsize);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: block per group %i\n", __FILE__, afs.fs_fpg);

            break;
        case 1:
            log_mesg(1, 0, 0, fs_opt.debug, "%s: magic = %x (UFS1)\n", __FILE__, afs.fs_magic);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: superblock location = %lld\nid = [ %x %x ]\n", __FILE__, afs.fs_sblockloc, afs.fs_id[0], afs.fs_id[1]);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: group (ncg) = %d\n", __FILE__, afs.fs_ncg);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: UFS size = %lld\n", __FILE__, afs.fs_size);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: Blocksize fs_fsize = %i\n", __FILE__, afs.fs_fsize);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: partition size = %lld\n", __FILE__, afs.fs_old_size*afs.fs_fsize);
            log_mesg(1, 0, 0, fs_opt.debug, "%s: block per group %i\n", __FILE__, afs.fs_fpg);

            break;
        default:
            log_mesg(1, 0, 0, fs_opt.debug, "%s: disk.d_ufs = %c", __FILE__, disk.d_ufs);
            break;
    }

    /// get ufs flags
    if (afs.fs_old_flags & FS_FLAGS_UPDATED)
        fsflags = afs.fs_flags;
    else
        fsflags = afs.fs_old_flags;

    /// check ufs status
    if(fs_opt.ignore_fschk){
	log_mesg(1, 0, 0, fs_opt.debug, "%s: %s: Ignore filesystem check\n", __FILE__, __FILE__);
    }else{
	if (afs.fs_clean == 1) {
	    log_mesg(1, 0, 0, fs_opt.debug, "%s: FS CLEAN (%i)\n", __FILE__, afs.fs_clean);
	} else {
	    if ((fsflags & (FS_UNCLEAN | FS_NEEDSFSCK)) == 0) {
		log_mesg(0, 1, 1, fs_opt.debug, "%s: UFS flag FS_UNCLEAN or FS_NEEDSFSCK\n\n", __FILE__);
	    }
	    log_mesg(1, 0, 0, fs_opt.debug, "%s: FS CLEAN not set!(%i)\n", __FILE__, afs.fs_clean);
	}
    }
}

/// close device
static void fs_close(){
    log_mesg(1, 0, 0, fs_opt.debug, "%s: close\n", __FILE__);
    ufs_disk_close(&disk);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: done\n\n", __FILE__);
}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    unsigned long long     total_block, block, bused = 0, bfree = 0;
    int                    done = 0, i = 0, start = 0, bit_size = 1;
    unsigned char* p;


    fs_open(device);

    /// init progress
    progress_bar   bprog;	/// progress_bar structure defined in progress.h
    progress_init(&bprog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    total_block = 0;
    /// read group
    while ((i = cgread(&disk)) != 0) {
        log_mesg(2, 0, 0, fs_opt.debug, "%s: \ncg = %d\n", __FILE__, disk.d_lcg);
        log_mesg(2, 0, 0, fs_opt.debug, "%s: blocks = %i\n", __FILE__, acg.cg_ndblk);
        p = cg_blksfree(&acg);

        for (block = 0; block < acg.cg_ndblk; block++){
            if (isset(p, block)) {
                pc_clear_bit(total_block, bitmap);
                bfree++;
                log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %lli\n", __FILE__, block);
            } else {
                pc_set_bit(total_block, bitmap);
                bused++;
                log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %lli\n", __FILE__, block);
            }
	    total_block++;
            update_pui(&bprog, total_block, total_block,done);
        }
        log_mesg(1, 0, 0, fs_opt.debug, "%s: read bitmap done\n", __FILE__);

    }

    fs_close();

    log_mesg(1, 0, 0, fs_opt.debug, "%s: total used = %lli, total free = %lli\n", __FILE__, bused, bfree);
    update_pui(&bprog, 1, 1, 1);

}

void initial_image_hdr(char* device, image_head* image_hdr)
{

    fs_open(device);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, ufs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = afs.fs_fsize;
    image_hdr->usedblocks  = get_used_block();
    switch (disk.d_ufs) {
        case 2:
	    image_hdr->totalblock = (unsigned long long)afs.fs_size;
            image_hdr->device_size = afs.fs_fsize*afs.fs_size;
            break;
        case 1:
	    image_hdr->totalblock = (unsigned long long)afs.fs_old_size;
            image_hdr->device_size = afs.fs_fsize*afs.fs_old_size;
            break;
        default:
            break;
    }

    fs_close();
}

