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

int mnt_x=0;
char *mnt_path = "/tmp/partclone_nilfs2_mnt/"; //fixme
struct nilfs_super_block *sbp;
struct nilfs *nilfs;

char *EXECNAME = "partclone.nilfs";

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

	if (nilfs_get_sustat(nilfs, &sustat) < 0)
		return 1;
	segnum = 0;
	rest = sustat.ss_nsegs;

	for ( ; rest > 0 && segnum < sustat.ss_nsegs; rest -= n) {
		count = min_t(__u64, rest, LSSU_NSEGS);
		nsi = nilfs_get_suinfo(nilfs, segnum, suinfos, count);
		if (nsi < 0)
			return 1;

		n = lssu_print_suinfo(nilfs, segnum, nsi, sustat.ss_prot_seq, bitmap);
		segnum += nsi;
	}

	return 0;
}

/// open device
static void fs_open(char* device)
{
    int open_flags;
    int mkstatus = 0;
    struct stat st;
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_mount\n", __FILE__); 

    if (stat(mnt_path, &st) != 0) {
	if (mkdir(mnt_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0 && errno != EEXIST) {
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: cannot create directory on %s\n", __FILE__, mnt_path);
	}
    } else if (!S_ISDIR(st.st_mode)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: error ENOTDIR cannot create directory on %s\n", __FILE__, strerror(mkstatus), mnt_path);
    }

    mnt_x = mount(device, mnt_path, "nilfs2", MS_MGC_VAL | MS_RDONLY | MS_NOSUID, "");
    open_flags = NILFS_OPEN_RDONLY | NILFS_OPEN_RAW;
    nilfs = nilfs_open(device, NULL, open_flags);
    if (nilfs == NULL) {
	umount(mnt_path);	
	log_mesg(0, 1, 1, fs_opt.debug, "%s: cannot open NILFS on %s: %m\n", __FILE__, device ? : "device");
    }
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_mount done\n", __FILE__);
}

/// close device
static void fs_close()
{
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_umount\n", __FILE__);
    nilfs_close(nilfs);
    umount(mnt_path);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs_umount done\n", __FILE__);
}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    int start = 0;
    int bit_size = 1;
    int status = 0;

    fs_open(device);
    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    blocks_per_segment = nilfs_get_blocks_per_segment(nilfs);
    total_block = image_hdr.totalblock;
    status = lssu_list_suinfo(nilfs, bitmap);

    if (status == 1 ){
	log_mesg(2, 0, 0, fs_opt.debug, "%s: nilfs list suinfo got fail\n", __FILE__);
    }

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    sbp = nilfs->n_sb;
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, nilfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = nilfs_get_block_size(nilfs);
    image_hdr->totalblock  = sbp->s_dev_size / image_hdr->block_size;
    image_hdr->usedblocks  = image_hdr->totalblock - sbp->s_free_blocks_count;
    image_hdr->device_size = sbp->s_dev_size;
    fs_close();
}

