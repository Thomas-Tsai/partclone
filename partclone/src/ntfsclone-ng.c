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
#include "reiserfsclone.h"

char *EXECNAME	= "partclone.ntfs";
int debug	= 2;
ntfs_volume *ntfs;

/// open device
static void fs_open(char* device){
   
    int	    err;
    int	    device_size, volume_size;

    ntfs = ntfs_mount(device, NTFS_MNT_RDONLY);
    if (!ntfs) {
	err = errno;
	log_mesg(0, 1, 1, debug, "ntfs mount error %i\n", err);
    } else {
	if (NVolWasDirty(ntfs)) {
	    log_mesg(0, 1, 1, debug, "NTFS Volume '%s' is scheduled for a check or it was shutdown\nuncleanly. Please boot Windows or use the --force option to progress.\n", device);
	} else {
	    log_mesg(3, 0, 0, debug, "NTFS Volume '%s' is clean\n", device);
	}

	if (NTFS_MAX_CLUSTER_SIZE < ntfs->cluster_size) {
	    log_mesg(0, 1, 1, debug, "NTFS Cluster size %u is too large!\n", (unsigned int)ntfs->cluster_size);
	} else {
	    log_mesg(3, 0, 0, debug, "NTFS Cluster size is right!\n");
	}

	log_mesg(3, 0, 0, debug, "NTFS volume version: %d.%d\n", ntfs->major_ver, ntfs->minor_ver);

	if (!ntfs_version_is_supported(ntfs)) {
	    log_mesg(3, 0, 0, debug, "libntfs open/mount %s as ntfs successfull\n", device);
	} else {
	    log_mesg(0, 1, 1, debug, "Unknown NTFS version\n");
	}
	
	device_size = ntfs_device_size_get(ntfs->dev, 1);
	volume_size = ntfs->nr_clusters * ntfs->cluster_size;

	log_mesg(3, 0, 0, debug, "Cluster size\t: %u\n", (unsigned int)ntfs->cluster_size);
	log_mesg(3, 0, 0, debug, "Volume size\t: %u * %u + 512 = %lli + 512 = %lli\n", 
		(unsigned int)ntfs->cluster_size,
		(unsigned int)ntfs->nr_clusters,
		volume_size,
		(volume_size+512)
		);
	log_mesg(3, 0, 0, debug, "Device size\t: %u\n", device_size);

	if (device_size < volume_size) {
	    log_mesg(0, 1, 1, debug, "Current NTFS volume size is bigger than the device size (%lld)!\nCorrupt partition table or incorrect device partitioning?\n", device_size);
	}

    }


}

/// close device
static void fs_close(){
    ntfs_umount(ntfs, 0);
}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap)
{
    unsigned char	ntfs_bitmap[NTFS_BUF_SIZE];
    unsigned long long	current_block, used_block, free_block, count, pos;
    unsigned long	bitmap_size = (ntfs->nr_clusters + 7) / 8;
    int			i;

    fs_open(device);

    if (bitmap == NULL) {
	log_mesg(0, 1, 1, debug, "bitmap alloc error\n");
    }

    pos = 0;
    used_block = 0;
    free_block = 0;
    while (1) {
	count = ntfs_attr_pread(ntfs->lcnbmp_na, pos, NTFS_BUF_SIZE, ntfs_bitmap);
	if (count == -1)
	    log_mesg(0, 1, 1, debug, "Couldn't get $Bitmap $DATA");
	if (count == 0) {
	    if(bitmap_size > pos)
		log_mesg(0, 1, 1, debug, "Bitmap size is smaller than expected (%lld != %lld)\n", bitmap_size, pos);
	    break;
	}
	for (i = 0; i < count; i++, pos++) {
	    if (bitmap_size <= pos)
		break;
	    for (current_block = pos * 8; current_block < (pos + 1) * 8; current_block++){
		if (ntfs_bit_get(ntfs_bitmap, current_block)){
		    bitmap[current_block] = 1;
		    used_block++;
		} else {
		    bitmap[current_block] = 0;
		    free_block++;
		}
	    }
	}
    }

    log_mesg(3, 0, 0, debug, "\nUsed Block\t: %lld\n", used_block);
    log_mesg(3, 0, 0, debug, "Free Block\t: %lld\n", free_block);

    fs_close();

}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, ntfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = ntfs->cluster_size;
    image_hdr->totalblock  = ntfs->nr_clusters;
    image_hdr->usedblocks  = (ntfs->nr_clusters - ntfs->nr_free_clusters - 1);
    image_hdr->device_size = ntfs_device_size_get(ntfs->dev, 1);
    fs_close();
}

