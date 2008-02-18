/**
 fatclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read hfsplus super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "partclone.h"
#include "fatclone.h"

struct FatBootSector fat_sb;
struct FatFsInfo fatfs_info;
int ret;
int FS;
char *EXECNAME = "clone.fat";

/// open device
static void fs_open(char* device)
{
    int r = 0;
    char *buffer;

    log_mesg(2, 0, 0, 2, "open device\n");
    ret = open(device, O_RDONLY);
    
    buffer = (char*)malloc(sizeof(FatBootSector));
    r = read (ret, buffer, sizeof(FatBootSector));
    memcpy(&fat_sb, buffer, sizeof(FatBootSector));
    free(buffer);

    buffer = (char*)malloc(sizeof(FatFsInfo));
    r = read(ret, &fatfs_info, sizeof(FatFsInfo));
    memcpy(&fatfs_info, buffer, sizeof(FatFsInfo));
    free(buffer);
    log_mesg(2, 0, 0, 2, "open device down\n");

}

/// close device
static void fs_close()
{
    close(ret);
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    char sig;
    unsigned long long total_sector = 0;
    unsigned long long bused = 0;
    unsigned long long root_sec = 0;
    unsigned long long data_sec = 0;
    unsigned long long sec_per_fat = 0;
    unsigned long long cluster_count = 0;
    unsigned long long free_blocks = 0;

    log_mesg(2, 0, 0, 2, "initial_image start\n");
    fs_open(device);

    if (fat_sb.u.fat16.ext_signature == 0x29){
	FS = FAT_16;
    } else {
	FS = FAT_32;
    }

    if (fat_sb.sectors != 0)
	total_sector = (unsigned long long)fat_sb.sectors;
    else
	total_sector = (unsigned long long)fat_sb.sector_count;
   
    if(fat_sb.fat_length != 0)
	sec_per_fat = fat_sb.fat_length;
    else
	sec_per_fat = fat_sb.u.fat32.fat_length;

    root_sec = ((fat_sb.dir_entries * 32) + fat_sb.sector_size - 1) / fat_sb.sector_size;
    data_sec = total_sector - ( fat_sb.reserved + (fat_sb.fats * sec_per_fat) + root_sec);
    cluster_count = data_sec / fat_sb.cluster_size;
    if (fatfs_info.magic == 0x41615252) 
	free_blocks = fatfs_info.free_count;

    bused = get_used_block();

    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, fat_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = (int)fat_sb.sector_size;
    image_hdr->totalblock  = (unsigned long long)total_sector;
    image_hdr->device_size = (unsigned long long)(total_sector * image_hdr->block_size);
    image_hdr->usedblocks  = (unsigned long long)bused;
    log_mesg(2, 0, 0, 2, "Block Size:%i\n", image_hdr->block_size);
    log_mesg(2, 0, 0, 2, "Total Blocks:%i\n", image_hdr->totalblock);
    log_mesg(2, 0, 0, 2, "Used Blocks:%i\n", image_hdr->usedblocks);
    log_mesg(2, 0, 0, 2, "Device Size:%i\n", image_hdr->device_size);

    fs_close();
    log_mesg(2, 0, 0, 2, "initial_image down\n");
}

/// readbitmap - cread and heck bitmap, reference dumpe2fs
extern void readbitmap(char* device, image_head image_hdr, char* bitmap)
{
    int i = 0, j = 0;
    int rd = 0;
    unsigned long long block = 0, bfree = 0, bused = 0, DamagedClusters = 0;
    unsigned long long root_sec = 0, data_sec = 0, cluster_count = 0, sec_per_fat = 0;
    unsigned long long total_sector = 0;
    int FatReservedBytes = 0;
    uint16_t Fat16_Entry = 0;
    uint32_t Fat32_Entry = 0;

    fs_open(device);

    if (fat_sb.sectors != 0)
	total_sector = (unsigned long long)fat_sb.sectors;
    else
	total_sector = (unsigned long long)fat_sb.sector_count;

    if(fat_sb.fat_length != 0)
	sec_per_fat = fat_sb.fat_length;
    else
	sec_per_fat = fat_sb.u.fat32.fat_length;

    root_sec = ((fat_sb.dir_entries * 32) + fat_sb.sector_size - 1) / fat_sb.sector_size;
    data_sec = total_sector - ( fat_sb.reserved + (fat_sb.fats * sec_per_fat) + root_sec);
    cluster_count = data_sec / fat_sb.cluster_size;
    
    /// init bitmap
    for (i = 0 ; i < total_sector ; i++)
	bitmap[i] = 1;

    /// A) the reserved sectors are used
    for (i=0; i < fat_sb.reserved; i++,block++)
        bitmap[block] = 1;

    /// B) the FAT tables are on used sectors
    for (j=0; j < fat_sb.fats; j++)
        for (i=0; i < sec_per_fat ; i++,block++)
            bitmap[block] = 1;

    /// C) The rootdirectory is on used sectors
    if (root_sec > 0) /// no rootdir sectors on FAT32
        for (i=0; i < root_sec; i++,block++)
            bitmap[block] = 1;

    /// D) The clusters
    FatReservedBytes = fat_sb.sector_size * fat_sb.reserved;

    /// skip reserved data and two first FAT entries
    if (FS == FAT_16)
        lseek(ret, FatReservedBytes+(2*2), SEEK_SET);
    else if (FS == FAT_32)
        lseek(ret, FatReservedBytes+(2*4), SEEK_SET);
    else
        log_mesg(2, 0, 0, 2, "ERR_WRONG_FS\n");

    for (i=0; i < cluster_count; i++){
        log_mesg(2, 0, 0, 2, "Cluster %i, \t\t", i);
        /// If FAT16
        if(FS == FAT_16){
            rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
            if (rd == -1)
		log_mesg(2, 0, 0, 2, "read Fat16_Entry error\n");
            if (Fat16_Entry  == 0xFFF7) { /// bad FAT16 cluster
                DamagedClusters++;
                log_mesg(2, 0, 0, 2, "bad sec %i\n", block);
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else if (Fat16_Entry == 0x0000){ /// free
                bfree++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else {
                bused++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 1;
            }
            log_mesg(2, 0, 0, 2, "status: %x, block: %i, bitmap: %x\n", Fat16_Entry, block, bitmap[block]);

        } else if (FS == FAT_32){ /// FAT32
            rd = read(ret, &Fat32_Entry, sizeof(Fat32_Entry));
            if (rd == -1)
		log_mesg(2, 0, 0, 2, "read Fat32_Entry error\n");
            if (Fat32_Entry  == 0x0FFFFFF7) { /// bad FAT32 cluster
                DamagedClusters++;
                log_mesg(2, 0, 0, 2, "bad sec %i\n", block);
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else if (Fat32_Entry == 0x0000){ /// free
                bfree++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else {
                bused++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 1;
            }
            log_mesg(2, 0, 0, 2, "status: %x, block: %i, bitmap: %x\n", Fat32_Entry, block, bitmap[block]);
        } else 
            log_mesg(2, 0, 0, 2, "error fs\n");
    }

    while(block < total_sector){
        bitmap[block] = 1;
        block++;
    }

    log_mesg(2, 0, 0, 2, "Free Blocks %i\n", bfree);
    log_mesg(2, 0, 0, 2, "Used Blocks(not include reversed) %i\n", bused);
    log_mesg(2, 0, 0, 2, "Bad Blocks %i\n", DamagedClusters);

    log_mesg(2, 0, 0, 2, "done\n");
    fs_close();
}

/// get_used_block - get FAT used blocks
extern unsigned long long get_used_block()
{
    int i = 0, j = 0;
    int rd = 0;
    unsigned long long bitmap_size = 0;
    unsigned long long block = 0, bfree = 0, bused = 0, DamagedClusters = 0;
    unsigned long long root_sec = 0, data_sec = 0, cluster_count = 0, sec_per_fat = 0;
    unsigned long long total_sector = 0;
    unsigned long long real_back_block= 0;
    int FatReservedBytes = 0;
    uint16_t Fat16_Entry = 0;
    uint32_t Fat32_Entry = 0;
    char *bitmap;

    log_mesg(2, 0, 0, 2, "get_used_block start\n");

    if (fat_sb.sectors != 0)
	total_sector = (unsigned long long)fat_sb.sectors;
    else
	total_sector = (unsigned long long)fat_sb.sector_count;

    if(fat_sb.fat_length != 0)
	sec_per_fat = fat_sb.fat_length;
    else
	sec_per_fat = fat_sb.u.fat32.fat_length;

    root_sec = ((fat_sb.dir_entries * 32) + fat_sb.sector_size - 1) / fat_sb.sector_size;
    data_sec = total_sector - ( fat_sb.reserved + (fat_sb.fats * sec_per_fat) + root_sec);
    cluster_count = data_sec / fat_sb.cluster_size;
    
    bitmap_size = total_sector;
    bitmap = (char *)malloc(bitmap_size);
    if (bitmap == NULL)
	log_mesg(2, 0, 0, 2, "bitmapalloc error\n");
    memset(bitmap, 0, bitmap_size);

    /// A) the reserved sectors are used
    for (i=0; i < fat_sb.reserved; i++,block++)
        bitmap[block] = 1;

    /// B) the FAT tables are on used sectors
    for (j=0; j < fat_sb.fats; j++)
        for (i=0; i < sec_per_fat ; i++,block++)
            bitmap[block] = 1;

    /// C) The rootdirectory is on used sectors
    if (root_sec > 0) /// no rootdir sectors on FAT32
        for (i=0; i < root_sec; i++,block++)
            bitmap[block] = 1;

    /// D) The clusters
    FatReservedBytes = fat_sb.sector_size * fat_sb.reserved;

    /// skip reserved data and two first FAT entries
    if (FS == FAT_16)
        lseek(ret, FatReservedBytes+(2*2), SEEK_SET);
    else if (FS == FAT_32)
        lseek(ret, FatReservedBytes+(2*4), SEEK_SET);
    else
        log_mesg(2, 0, 0, 2, "ERR_WRONG_FS\n");

    for (i=0; i < cluster_count; i++){
        /// If FAT16
        if(FS == FAT_16){
            rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
            if (rd == -1)
		log_mesg(2, 0, 0, 2, "read Fat16_Entry error\n");
            if (Fat16_Entry  == 0xFFF7) { /// bad FAT16 cluster
                DamagedClusters++;
                log_mesg(2, 0, 0, 2, "bad sec %i\n", block);
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else if (Fat16_Entry == 0x0000){ /// free
                bfree++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else {
                bused++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 1;
            }

        } else if (FS == FAT_32){ /// FAT32
            rd = read(ret, &Fat32_Entry, sizeof(Fat32_Entry));
            if (rd == -1)
		log_mesg(2, 0, 0, 2, "read Fat32_Entry error\n");
            if (Fat32_Entry  == 0x0FFFFFF7) { /// bad FAT32 cluster
                DamagedClusters++;
                log_mesg(2, 0, 0, 2, "bad sec %i\n", block);
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else if (Fat32_Entry == 0x0000){ /// free
                bfree++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 0;
            } else {
                bused++;
                for (j=0; j < fat_sb.cluster_size; j++,block++)
                    bitmap[block] = 1;
            }
        } else 
            log_mesg(2, 0, 0, 2, "error fs\n");
    }

    while(block < total_sector){
        bitmap[block] = 1;
        block++;
    }


    for (block = 0; block < total_sector; block++)
    {
	if (bitmap[block] == 1) {
	    real_back_block++;
	}
    }
    free(bitmap);
    log_mesg(2, 0, 0, 2, "get_used_block down\n");

    return real_back_block;
}

