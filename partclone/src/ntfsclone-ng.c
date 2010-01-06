/**
 * reiserfsclone.c - part of Partclone project
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
#include <errno.h>

#define NTFS_DO_NOT_CHECK_ENDIANS
#define NTFS_MAX_CLUSTER_SIZE   65536

#include <ntfs/device.h>
#include <ntfs/volume.h>
#include <ntfs/bitmap.h>

#include "partclone.h"
#include "ntfsclone-ng.h"
#include "progress.h"
#include "fs_common.h"

/// define mount flag
#ifdef NTFS_MNT_RDONLY
#define LIBNTFS_VER_10 1
#else
#define NTFS_MNT_RDONLY 1
#define LIBNTFS_VER_9 1
#endif

char *EXECNAME	= "partclone.ntfs";
extern fs_cmd_opt fs_opt;
ntfs_volume *ntfs;

/// open device
static void fs_open(char* device){

    int	    err;
    unsigned long long device_size, volume_size;

    ntfs = ntfs_mount(device, NTFS_MNT_RDONLY);
    if (!ntfs) {
        err = errno;
        log_mesg(0, 1, 1, fs_opt.debug, "%s: NOT NTFS partition, ntfs mount error %i\n", __FILE__, err);
    } else {
        
        if(fs_opt.ignore_fschk){
            log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
        }else{
            if (ntfs->flags & VOLUME_IS_DIRTY) {
                log_mesg(0, 1, 1, fs_opt.debug, "%s: NTFS Volume '%s' is scheduled for a check or it was shutdown\nuncleanly. Please boot Windows or fix it by fsck.\n", __FILE__, device);
            } else {
                log_mesg(3, 0, 0, fs_opt.debug, "%s: NTFS Volume '%s' is clean\n", __FILE__, device);
            }
        }

        if (NTFS_MAX_CLUSTER_SIZE < ntfs->cluster_size) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: NTFS Cluster size %u is too large!\n", __FILE__, (unsigned int)ntfs->cluster_size);
        } else {
            log_mesg(3, 0, 0, fs_opt.debug, "%s: NTFS Cluster size is right!\n", __FILE__);
        }

        log_mesg(3, 0, 0, fs_opt.debug, "%s: NTFS volume version: %d.%d\n", __FILE__, ntfs->major_ver, ntfs->minor_ver);

        if (!ntfs_version_is_supported(ntfs)) {
            log_mesg(3, 0, 0, fs_opt.debug, "%s: libntfs open/mount %s as ntfs successfull\n", __FILE__, device);
        } else {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Unknown NTFS version\n", __FILE__);
        }

        device_size = ntfs_device_size_get(ntfs->dev, 1);
        volume_size = ntfs->nr_clusters * ntfs->cluster_size;

        log_mesg(3, 0, 0, fs_opt.debug, "%s: Cluster size\t: %u\n", __FILE__, (unsigned int)ntfs->cluster_size);
        log_mesg(3, 0, 0, fs_opt.debug, "%s: Volume size\t: %u * %u + 512 = %lli + 512 = %lli\n", __FILE__, 
                (unsigned int)ntfs->cluster_size,
                (unsigned int)ntfs->nr_clusters,
                volume_size,
                (volume_size+512)
                );
        log_mesg(3, 0, 0, fs_opt.debug, "%s: Device size\t: %u\n", __FILE__, device_size);

        if (device_size < volume_size) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Current NTFS volume size is bigger than the device size (%lld)!\nCorrupt partition table or incorrect device partitioning?\n", __FILE__, device_size);
        }

    }


}

/// close device
static void fs_close(){
    ntfs_umount(ntfs, 0);
}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui)
{
    unsigned char	*ntfs_bitmap;
    unsigned long long	current_block, used_block, free_block, count, pos;
    unsigned long	bitmap_size = (ntfs->nr_clusters + 7) / 8;
    int			i;
    int start = 0;
    int bit_size = 1;

    fs_open(device);
    ntfs_bitmap = (char*)malloc(bitmap_size);

    if ((bitmap == NULL) || (ntfs_bitmap == NULL)) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap alloc error\n", __FILE__);
    }

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, bit_size);

    pos = 0;
    used_block = 0;
    free_block = 0;
    count = ntfs_attr_pread(ntfs->lcnbmp_na, pos, bitmap_size, ntfs_bitmap);
    for (current_block = 0; current_block < ntfs->nr_clusters; current_block++)
    {
        char bit;
        bit = ntfs_bit_get(ntfs_bitmap, current_block);
        if (bit){
            bitmap[current_block] = 1;
            used_block++;
        } else {
            bitmap[current_block] = 0;
            free_block++;
        }
        /// update progress
        update_pui(&prog, current_block, 0);

    }
    log_mesg(3, 0, 0, fs_opt.debug, "%s: Used Block\t: %lld\n", __FILE__, used_block);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: Free Block\t: %lld\n", __FILE__, free_block);
    free(ntfs_bitmap);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap alloc free\n", __FILE__);
    fs_close();
    log_mesg(3, 0, 0, fs_opt.debug, "%s: fs_close done\n", __FILE__);

    /// update progress
    update_pui(&prog, 1, 1);
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, ntfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = (int)ntfs->cluster_size;
    image_hdr->totalblock  = (unsigned long long)ntfs->nr_clusters;
    image_hdr->usedblocks  = (unsigned long long)(ntfs->nr_clusters - ntfs->nr_free_clusters - 1);
    image_hdr->device_size = (unsigned long long)ntfs_device_size_get(ntfs->dev, 1);
    fs_close();
}

