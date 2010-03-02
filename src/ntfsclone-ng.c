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


/********************************************************
 * Routines for counting attributes free bits.
 *
 * Used to count and verify number of free clusters
 * as per ntfs volume $BitMap.
 *
 * Copied from ntfs-3g 2009.11.14 source code:
 *
 ********************************************************/

/* Below macros are 32-bit ready. */
#define BCX(x) ((x) - (((x) >> 1) & 0x77777777) - \
                      (((x) >> 2) & 0x33333333) - \
                      (((x) >> 3) & 0x11111111))

#define BITCOUNT(x) (((BCX(x) + (BCX(x) >> 4)) & 0x0F0F0F0F) % 255)

static u8 *ntfs_init_lut256(void)
{
        int i;
        u8 *lut;

        lut = ntfs_malloc(256);
        if (lut)
                for(i = 0; i < 256; i++)
                        *(lut + i) = 8 - BITCOUNT(i);
        return lut;
}

static s64 ntfs_attr_get_free_bits(ntfs_attr *na)
{
        u8 *buf, *lut;
        s64 br      = 0;
        s64 total   = 0;
        s64 nr_free = 0;

        lut = ntfs_init_lut256();
        if (!lut)
                return -1;

        buf = ntfs_malloc(65536);
        if (!buf)
                goto out;

        while (1) {
                u32 *p;
                br = ntfs_attr_pread(na, total, 65536, buf);
                if (br <= 0)
                        break;
                total += br;
                p = (u32 *)buf + br / 4 - 1;
                for (; (u8 *)p >= buf; p--) {
                        nr_free += lut[ *p        & 255] +
                                   lut[(*p >>  8) & 255] +
                                   lut[(*p >> 16) & 255] +
                                   lut[(*p >> 24)      ];
                }
                switch (br % 4) {
                        case 3:  nr_free += lut[*(buf + br - 3)];
                        case 2:  nr_free += lut[*(buf + br - 2)];
                        case 1:  nr_free += lut[*(buf + br - 1)];
                }
        }
        free(buf);
out:
        free(lut);
        if (!total || br < 0)
                return -1;
        return nr_free;
}

/* End of ntfs-3g routines code copy */
/*********************************************************/


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

        // Initialize free clusters metric
        ntfs->nr_free_clusters = ntfs_attr_get_free_bits(ntfs->lcnbmp_na);

        if ( ntfs->nr_free_clusters < 0 || ntfs->nr_free_clusters >= ntfs->nr_clusters) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Bad number of free (%lld) or total (%lld) clusters!\n", __FILE__,
                ntfs->nr_free_clusters, ntfs->nr_clusters); 
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
    int ret = ntfs_umount(ntfs, 0);

    if (ret != 0) {
        log_mesg(0, 0, 1, fs_opt.debug, "%s: NTFS unmount error %i!\n", __FILE__, errno);
    }
}

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui)
{
    unsigned char	*ntfs_bitmap;
    unsigned long long	current_block, used_block, free_block, pos;
    long long int	count;
    unsigned long	bitmap_size = (ntfs->nr_clusters + 7) / 8;
    int start = 0;
    int bit_size = 1;

    fs_open(device);

    if (bitmap_size > ntfs->lcnbmp_na->data_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: calculated bitmap size (%lu) > lcnbmp_na->data_size (%llu)\n", __FILE__, bitmap_size, ntfs->lcnbmp_na->data_size);
    }
 
    ntfs_bitmap = (unsigned char*)malloc(bitmap_size);

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
    if (count == -1){					    // On error and nothing has been read
	log_mesg(0, 1, 1, fs_opt.debug, "%s: read ntfs attr error: %s\n", __FILE__, strerror(errno));
    }
    if (count != bitmap_size){
	log_mesg(0, 1, 1, fs_opt.debug, "%s: the readed size of ntfs_attr not expected: %s\n", __FILE__, strerror(errno));
    }

    for (current_block = 0; current_block < ntfs->nr_clusters; current_block++)
    {
        char bit;
        bit = ntfs_bit_get(ntfs_bitmap, current_block);
        if (bit == -1){                                     // Return -1 on error
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: check bitmap error\n", __FILE__); 
	}else if(bit == 1){				    // The value of the bit (0 or 1)
            bitmap[current_block] = 1;
            used_block++;
        } else {
            bitmap[current_block] = 0;
            free_block++;
        }
        /// update progress
        update_pui(&prog, current_block, 0);

    }

    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Used Block\t: %llu\n", __FILE__, used_block);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Free Block\t: %llu\n", __FILE__, free_block);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Calculated Bitmap Size\t: %lu\n", __FILE__, bitmap_size);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Bitmap attribute data size\t: %llu\n", __FILE__, ntfs->lcnbmp_na->data_size);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Bitmap attribute initialized size\t: %llu\n", __FILE__, ntfs->lcnbmp_na->initialized_size);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: [bitmap] Bitmap attribute allocated size\t: %llu\n", __FILE__, ntfs->lcnbmp_na->allocated_size);

    free(ntfs_bitmap);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap alloc free\n", __FILE__);
    fs_close();
    log_mesg(3, 0, 0, fs_opt.debug, "%s: fs_close done\n", __FILE__);

    if (used_block != image_hdr.usedblocks) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: used blocks count mismatch: %llu in header, %llu from readbitmap\n", __FILE__,
                used_block, image_hdr.usedblocks);
    }

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
    image_hdr->usedblocks  = (unsigned long long)(ntfs->nr_clusters - ntfs->nr_free_clusters);
    image_hdr->device_size = (unsigned long long)ntfs_device_size_get(ntfs->dev, 1);
    fs_close();

    log_mesg(3, 0, 0, fs_opt.debug, "%s: hdr - usedblocks:\t: %llu\n", __FILE__, image_hdr->usedblocks);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: hdr - totalblocks:\t: %llu\n", __FILE__, image_hdr->totalblock);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: ntfs - nr_free:\t: %lld\n", __FILE__, ntfs->nr_free_clusters);
}

