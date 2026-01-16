/**
* nilfsclone.c - part of Partclone project
*
* Copyright (c) 2015~ Thomas Tsai <thomas at nchc org tw>
*
* read nilfs super block and bitmap
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/


#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

//#include "nilfs/nilfs.h"
//#include "nilfs/util.h"
#include <nilfs.h>
#define LSSU_NSEGS      512

#include "partclone.h"
#include "nilfsclone.h"
#include "progress.h"
#include "fs_common.h"

#define MAX_NILFS_TOTAL_BLOCKS (1ULL << 40) // Max total blocks (approx 1TB @ 1KB/block) to prevent DoS/memory exhaustion
#define MIN_NILFS_BLOCK_SIZE 1024 // Smallest supported block size (1KB)
#define MAX_NILFS_BLOCK_SIZE 65536 // Largest supported block size (64KB)
#define MAX_NILFS_NSEGS (1ULL << 30) // Max 1 billion segments to prevent DoS

int mnt_x=0;
char *mnt_path = "/tmp/partclone_nilfs2_mnt/"; //fixme
struct nilfs_super_block *sbp;
struct nilfs *nilfs;

extern fs_cmd_opt fs_opt;

static struct nilfs_suinfo suinfos[LSSU_NSEGS];
int blocks_per_segment;
unsigned long long total_block;

///set useb block
static void set_bitmap(unsigned long* bitmap, uint64_t segm, uint64_t count){
    uint64_t block;
    uint64_t pos_block;
    uint64_t block_end;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: segment: %llu count %llu\n", __FILE__,  segm, count);
    if (segm*blocks_per_segment + count > total_block) {
	log_mesg(1, 0, 0, fs_opt.debug, "%s: block(%llu) larger than total blocks(%llu), skip it.\n", __FILE__,  segm, total_block);
	return;
    }
    pos_block = segm*blocks_per_segment;
    block_end = (segm+1)*blocks_per_segment;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: block offset: %llu block count: %llu\n",__FILE__,  pos_block, block_end);

    for(block = pos_block; block < block_end; block++){
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block %i is used\n",__FILE__,  block);
	pc_set_bit(block, bitmap, total_block);
    }
}

static ssize_t lssu_print_suinfo(struct nilfs *nilfs, __u64 segnum,
				 ssize_t nsi, __u64 protseq,
				 unsigned long* bitmap)
{
    ssize_t i, n = 0;
    int all = 1;

    for (i = 0; i < nsi; i++, segnum++) {
	if (!all && nilfs_suinfo_clean(&suinfos[i]))
	    continue;

	log_mesg(3, 0, 0, fs_opt.debug, "%s: seg %llu, %c %c %c , %i\n",
			       __FILE__, (unsigned long long)segnum,
			       nilfs_suinfo_active(&suinfos[i]) ? 'a' : '-',
			       nilfs_suinfo_dirty(&suinfos[i]) ? 'd' : '-',
			       nilfs_suinfo_error(&suinfos[i]) ? 'e' : '-',
			       suinfos[i].sui_nblocks);
        if  (nilfs_suinfo_active(&suinfos[i]) || nilfs_suinfo_dirty(&suinfos[i])){
	    set_bitmap(bitmap, (unsigned long long)segnum,  suinfos[i].sui_nblocks);
        }
        n++;
    }
    return n;
}

static int lssu_list_suinfo(struct nilfs *nilfs, unsigned long* bitmap)
{
    struct nilfs_sustat sustat;
    __u64 segnum, rest, count;
    ssize_t nsi, n;

    if (nilfs_get_sustat(nilfs, &sustat) < 0) {
	log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get NILFS segment usage statistics.\n");
	return 1;
    }
    if (sustat.ss_nsegs == 0 || sustat.ss_nsegs > MAX_NILFS_NSEGS) {
	log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero number of NILFS segments detected: %llu. Max allowed: %llu\n",
	sustat.ss_nsegs, MAX_NILFS_NSEGS);
	return 1;
    }

    segnum = 0;
    rest = sustat.ss_nsegs;

    for ( ; rest > 0 && segnum < sustat.ss_nsegs; rest -= n) {
	count = min_t(__u64, rest, LSSU_NSEGS);
	nsi = nilfs_get_suinfo(nilfs, segnum, suinfos, count);
	if (nsi < 0) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Failed to get NILFS segment info for segment %llu.\n", segnum);
	    return 1;
        }
        if (nsi == 0 || nsi > LSSU_NSEGS || nsi > rest) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid number of suinfo entries read: %zd. Expected between 1 and %d, and <= remaining segments (%llu).\n", nsi, LSSU_NSEGS, rest);
            return 1;
        }

	n = lssu_print_suinfo(nilfs, segnum, nsi, sustat.ss_prot_seq, bitmap);
        if (n < 0) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: lssu_print_suinfo returned negative value for segment %llu.\n", segnum);
            return 1;
        }
        if (n > nsi) {
            log_mesg(0, 1, 1, fs_opt.debug, "ERROR: lssu_print_suinfo returned more entries than read for segment %llu.\n", segnum);
            return 1;
        }
	segnum += nsi; // Use nsi for advancing segnum, as it's the number read.
    }

    return 0;
}

/// open device
static int fs_open(char* device) // Change return type to int
{
    int open_flags;
    struct stat st;
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_mount\n", __FILE__);

    if (stat(mnt_path, &st) != 0) {
	if (mkdir(mnt_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: cannot create directory on %s: %s\n", __FILE__, mnt_path, strerror(errno));
            return -1;
	}
    } else if (!S_ISDIR(st.st_mode)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: %s is not a directory: %s\n", __FILE__, mnt_path, strerror(ENOTDIR));
        return -1;
    }

    mnt_x = mount(device, mnt_path, "nilfs2", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, "");
    if (mnt_x != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: cannot mount device %s on %s: %s\n", __FILE__, device, mnt_path, strerror(errno));
        return -1;
    }

    open_flags = NILFS_OPEN_RDONLY | NILFS_OPEN_RAW;
    nilfs = nilfs_open(device, NULL, open_flags);
    if (nilfs == NULL) {
	umount(mnt_path);
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: cannot open NILFS on %s: %s\n", __FILE__, device ? : "device", strerror(errno));
        return -1;
    }
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_mount done\n", __FILE__);
    return 0; // Success
}

/// close device
static void fs_close()
{
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_umount\n", __FILE__);
    if (nilfs) {
        nilfs_close(nilfs);
        nilfs = NULL;
    }
    if (mnt_x == 0) {
        umount(mnt_path);
        mnt_x = -1;
    }
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_umount done\n", __FILE__);
}

///  readbitmap - read bitmap
extern void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    int start = 0;
    int bit_size = 1;
    int status = 0;

    if (fs_open(device) != 0) {
        return; // Abort
    }

    /// init progress
    progress_bar   prog;    /// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    blocks_per_segment = nilfs_get_blocks_per_segment(nilfs);

    if (blocks_per_segment <= 0 || blocks_per_segment > fs_info.totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid blocks_per_segment detected: %d. Should be positive and <= total blocks (%llu)\n",
                 blocks_per_segment, fs_info.totalblock);
        fs_close();
        return;
    }

    total_block = fs_info.totalblock;
    status = lssu_list_suinfo(nilfs, bitmap);

    if (status != 0 ){
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: nilfs list suinfo failed with status %d\n", __FILE__, status);
        fs_close();
        return;
    }

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}
/// read super block and write to image head
extern void read_super_blocks(char* device, file_system_info* fs_info)
{
    if (fs_open(device) != 0) {
        return; // Abort
    }
    sbp = nilfs->n_sb;
    strncpy(fs_info->fs, nilfs_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = nilfs_get_block_size(nilfs);

    if (fs_info->block_size < MIN_NILFS_BLOCK_SIZE || fs_info->block_size > MAX_NILFS_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %u.\n", fs_info->block_size);
        fs_close();
        return;
    }

    if (sbp->s_dev_size == 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Device size (s_dev_size) is zero in superblock.\n");
        fs_close();
        return;
    }
    if (fs_info->block_size == 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Block size is zero, cannot calculate total blocks.\n");
        fs_close();
        return;
    }
    fs_info->totalblock  = sbp->s_dev_size / fs_info->block_size;

    if (fs_info->totalblock == 0 || fs_info->totalblock > MAX_NILFS_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks detected in superblock: %llu. Max allowed: %llu\n", fs_info->totalblock, MAX_NILFS_TOTAL_BLOCKS);
        fs_close();
        return;
    }

    if (fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow in device size calculation. totalblock: %llu, block_size: %u\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->totalblock * fs_info->block_size;

    fs_info->usedblocks  = fs_info->totalblock - sbp->s_free_blocks_count;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_close();
}
