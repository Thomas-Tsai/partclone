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

#include "partclone.h"
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

#include "progress.h"
#include "fs_common.h"

#define MAX_UFS_TOTAL_BLOCKS (1ULL << 40) // Max total blocks (approx 512TB @ 512B/block) to prevent DoS/memory exhaustion
#define MIN_UFS_BLOCK_SIZE 512 // Smallest supported UFS block size
#define MAX_UFS_BLOCK_SIZE 65536 // Largest supported UFS block size (64KB)
#define MAX_UFS_NCG (1ULL << 24) // Max 16 million cylinder groups for DoS prevention
#define MAX_UFS_CG_BLOCKS (1ULL << 20) // Max 1 million blocks per cylinder group for DoS prevention

#define afs     disk.d_fs
#define acg     disk.d_cg
struct uufsd disk;

/// get_used_block - get UFS used blocks
static int get_used_block(unsigned long long *bused_out, unsigned long long total_blocks)
{
    unsigned long long    block;
    unsigned long long    current_bfree = 0;
    int                   i = 0;
    unsigned char	  *p;

    if (afs.fs_ncg <= 0 || (unsigned long long)afs.fs_ncg > MAX_UFS_NCG) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero number of cylinder groups detected: %d. Max allowed: %llu\n", afs.fs_ncg, MAX_UFS_NCG);
        return -1;
    }

    disk.d_lcg = -1; // Force cgread to read the first cg (0)

    // Loop for all cylinder groups up to afs.fs_ncg
    for (int cg_idx = 0; cg_idx < afs.fs_ncg; cg_idx++) {
        i = cgread(&disk);
        if (i == 0) { // End of groups before expected
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Unexpected end of cylinder groups (only read %d out of %d).\n", cg_idx, afs.fs_ncg);
            return -1;
        }
        if (i == -1) { // Error reading cylinder group
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to read cylinder group %d: %s.\n", cg_idx, strerror(errno));
            return -1;
        }

        log_mesg(2, 0, 0, fs_opt.debug, "%s: \ncg = %d\n", __FILE__, disk.d_lcg);
        log_mesg(2, 0, 0, fs_opt.debug, "%s: data blocks = %i\n", __FILE__, acg.cg_ndblk);

        // Validate acg.cg_ndblk
        if (acg.cg_ndblk <= 0 || (unsigned long long)acg.cg_ndblk > MAX_UFS_CG_BLOCKS) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero blocks in cylinder group detected: %d. Max allowed: %llu\n", acg.cg_ndblk, MAX_UFS_CG_BLOCKS);
            return -1;
        }

        p = cg_blksfree(&acg);
        if (!p) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get block free map for cylinder group %d.\n", cg_idx);
            return -1;
        }

        for (block = 0; block < (unsigned long long)acg.cg_ndblk; block++){
            if (isset(p, block)) {
                current_bfree++;
            }
        }
    }
    log_mesg(1, 0, 0, fs_opt.debug, "%s: total blocks = %llu, total free = %lli, total used = %lli\n", __FILE__, total_blocks, current_bfree, total_blocks - current_bfree);
    *bused_out = total_blocks - current_bfree;
    return 0; // Success
}

/// open device
static int fs_open(char* device){ // Changed return type to int

    int fsflags = 0;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: UFS partition Open\n", __FILE__);
    if (ufs_disk_fillout(&disk, device) == -1){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: UFS open fail for %s: %s\n", __FILE__, device, strerror(errno));
        return -1;
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
    return 0; // Success
}
/// close device
static void fs_close(){
    log_mesg(1, 0, 0, fs_opt.debug, "%s: close\n", __FILE__);
    ufs_disk_close(&disk);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: done\n\n", __FILE__);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    unsigned long long     block_in_cg;
    unsigned long long     bused = 0, bfree = 0;
    int                    i = 0, start = 0, bit_size = 1;
    unsigned char* p;


    if (fs_open(device) != 0) {
        return; // Abort
    }

    // Initialize bitmap to all USED (0xFF). We will only CLEAR bits for free data blocks.
    pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);

    /// init progress
    progress_bar   bprog;	/// progress_bar structure defined in progress.h
    progress_init(&bprog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    if (afs.fs_ncg <= 0 || (unsigned long long)afs.fs_ncg > MAX_UFS_NCG) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero number of cylinder groups detected: %d. Max allowed: %llu\n", afs.fs_ncg, MAX_UFS_NCG);
        fs_close();
        return;
    }

    // Ensure we start from the first cylinder group
    disk.d_lcg = -1; // Force cgread to read the first cg (0)

    // Loop for all cylinder groups up to afs.fs_ncg
    for (int cg_idx = 0; cg_idx < afs.fs_ncg; cg_idx++) {
        i = cgread(&disk);
        if (i == 0) { // End of groups before expected
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Unexpected end of cylinder groups (only read %d out of %d).\n", cg_idx, afs.fs_ncg);
            fs_close();
            return;
        }
        if (i == -1) { // Error reading cylinder group
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to read cylinder group %d: %s.\n", cg_idx, strerror(errno));
            fs_close();
            return;
        }

        log_mesg(2, 0, 0, fs_opt.debug, "%s: \ncg = %d\n", __FILE__, disk.d_lcg);
        log_mesg(2, 0, 0, fs_opt.debug, "%s: data blocks = %i\n", __FILE__, acg.cg_ndblk);

        // Validate acg.cg_ndblk
        if (acg.cg_ndblk <= 0 || (unsigned long long)acg.cg_ndblk > MAX_UFS_CG_BLOCKS) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero blocks in cylinder group detected: %d. Max allowed: %llu\n", acg.cg_ndblk, MAX_UFS_CG_BLOCKS);
            fs_close();
            return;
        }

        p = cg_blksfree(&acg);
        if (!p) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get block free map for cylinder group %d.\n", cg_idx);
            fs_close();
            return;
        }

        uint64_t cg_base = cgstart(&afs, cg_idx);
        for (block_in_cg = 0; block_in_cg < (unsigned long long)afs.fs_fpg; block_in_cg++){
            uint64_t abs_frag = cg_base + block_in_cg;

            // Ensure abs_frag does not exceed fs_info.totalblock
            if (abs_frag >= fs_info.totalblock) {
                break;
            }

            if (isset(p, block_in_cg)) {
                pc_clear_bit(abs_frag, bitmap, fs_info.totalblock);
                bfree++;
                log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is free %lli (abs %lli)\n", __FILE__, block_in_cg, abs_frag);
            } else {
                pc_set_bit(abs_frag, bitmap, fs_info.totalblock);
                bused++;
                log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap is used %lli (abs %lli)\n", __FILE__, block_in_cg, abs_frag);
            }
            update_pui(&bprog, abs_frag, abs_frag, 0);
        }
        log_mesg(3, 0, 0, fs_opt.debug, "%s: read bitmap done for cg %d\n", __FILE__, cg_idx);
    } // End of for loop for cylinder groups

    fs_close();

    log_mesg(1, 0, 0, fs_opt.debug, "%s: total reported free = %lli\n", __FILE__, bfree);
    update_pui(&bprog, 1, 1, 1);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{

    if (fs_open(device) != 0) {
        return; // Abort
    }
    strncpy(fs_info->fs, ufs_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size = afs.fs_fsize;

    if (fs_info->block_size < MIN_UFS_BLOCK_SIZE || fs_info->block_size > MAX_UFS_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %u.\n", fs_info->block_size);
        fs_close();
        return;
    }

    switch (disk.d_ufs) {
        case 2:
            // afs.fs_size is int64_t
            if (afs.fs_size <= 0 || (unsigned long long)afs.fs_size > MAX_UFS_TOTAL_BLOCKS) {
                log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks (UFS2) detected: %lld. Max allowed: %llu\n", afs.fs_size, MAX_UFS_TOTAL_BLOCKS);
                fs_close();
                return;
            }
            fs_info->totalblock  = afs.fs_size;
            break;
        case 1:
            // afs.fs_old_size is int32_t
            if (afs.fs_old_size <= 0 || (unsigned long long)afs.fs_old_size > MAX_UFS_TOTAL_BLOCKS) {
                log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks (UFS1) detected: %d. Max allowed: %llu\n", afs.fs_old_size, MAX_UFS_TOTAL_BLOCKS);
                fs_close();
                return;
            }
            fs_info->totalblock  = afs.fs_old_size;
            break;
        default:
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Unknown UFS version [%d].\n", disk.d_ufs);
            fs_close();
            return;
    }

    unsigned long long detected_used_blocks;
    if (get_used_block(&detected_used_blocks, fs_info->totalblock) != 0) {
        fs_close();
        return;
    }
    fs_info->usedblocks = detected_used_blocks;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;

    if (fs_info->block_size == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow or division by zero in device size calculation. totalblock: %llu, block_size: %u\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->totalblock * fs_info->block_size;

    fs_close();
}

