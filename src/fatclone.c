/**
  fatclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read FAT12/16/32 super block and bitmap
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
#include "progress.h"
#include "fs_common.h"

struct FatBootSector fat_sb;
struct FatFsInfo fatfs_info;
int ret;
int FS;
char *fat_type = "FATXX";
char *EXECNAME = "partclone.fat";
extern fs_cmd_opt fs_opt;

/// get fet type
static void get_fat_type(){

    /// fix, 1. make sure fat_sb; 2. the method shoud be check again
    if (fat_sb.u.fat16.ext_signature == 0x29){
        if (fat_sb.u.fat16.fat_name[4] == '6'){
            FS = FAT_16;
            fat_type = "FAT16";
            log_mesg(2, 0, 0, fs_opt.debug, "%s: FAT Type : FAT 16\n", __FILE__);
        } else if (fat_sb.u.fat16.fat_name[4] == '2'){
            FS = FAT_12;
            fat_type = "FAT12";
            log_mesg(2, 0, 0, fs_opt.debug, "%s: FAT Type : FAT 12\n", __FILE__);
        } else {
            log_mesg(2, 1, 1, fs_opt.debug, "%s: FAT Type : unknow\n" __FILE__);
        }
    } else if (fat_sb.u.fat32.fat_name[4] == '2'){
        FS = FAT_32;
        fat_type = "FAT32";
        log_mesg(2, 0, 0, fs_opt.debug, "%s: FAT Type : FAT 32\n", __FILE__);
    } else {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Unknown fat type!!\n", __FILE__);
    }
    log_mesg(2, 0, 0, fs_opt.debug, "%s: FS = %i\n", __FILE__, FS);

}

/// return total sectors
unsigned long long get_total_sector()
{
    unsigned long long total_sector = 0;

    /// get fat sectors
    if (fat_sb.sectors != 0)
        total_sector = (unsigned long long)fat_sb.sectors;
    else
        total_sector = (unsigned long long)fat_sb.sector_count;

    return total_sector;

}

///return sec_per_fat
unsigned long long get_sec_per_fat()
{
    unsigned long long sec_per_fat = 0;
    /// get fat length
    if(fat_sb.fat_length != 0)
        sec_per_fat = fat_sb.fat_length;
    else
        sec_per_fat = fat_sb.u.fat32.fat_length;
    return sec_per_fat;

}

///return root sec
unsigned long long get_root_sec()
{
    unsigned long long root_sec = 0;
    root_sec = ((fat_sb.dir_entries * 32) + fat_sb.sector_size - 1) / fat_sb.sector_size;
    return root_sec;
}

/// return cluster count
unsigned long long get_cluster_count()
{
    unsigned long long data_sec = 0;
    unsigned long long cluster_count = 0;
    unsigned long long total_sector = get_total_sector();
    unsigned long long root_sec = get_root_sec();
    unsigned long long sec_per_fat = get_sec_per_fat();

    data_sec = total_sector - ( fat_sb.reserved + (fat_sb.fats * sec_per_fat) + root_sec);
    cluster_count = data_sec / fat_sb.cluster_size;
    return cluster_count;
}

/// check fat status
//return - 0 Filesystem is in valid state.
//return - 1 Filesystem isn't in valid state.
//return - 2 other error.
extern int check_fat_status(){
    int rd = 0;
    uint16_t Fat16_Entry;
    uint32_t Fat32_Entry;
    int fs_error = 2;
    int fs_good = 0;
    int fs_bad = 1;


    /// fix. 1.check ret; 

    if (FS == FAT_16){
        /// FAT[0] contains BPB_Media code
        rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
        log_mesg(2, 0, 0, fs_opt.debug, "%s: Media %x\n", __FILE__, Fat16_Entry);
        if (rd == -1)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat16_Entry error\n", __FILE__);
        /// FAT[1] is set for FAT16/FAT32 for dirty/error volume flag
        rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
        if (rd == -1)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat16_Entry error\n", __FILE__);
        if (Fat16_Entry & 0x8000)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: Volume clean!\n", __FILE__);
        else
            return fs_bad;

        if (Fat16_Entry & 0x4000)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: I/O correct!\n", __FILE__);
        else 
            return fs_error;

    } else if (FS == FAT_32) {
        /// FAT[0] contains BPB_Media
        rd = read(ret, &Fat32_Entry, sizeof(Fat32_Entry));
        if (rd == -1)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat32_Entry error\n", __FILE__);
        /// FAT[1] is set for FAT16/FAT32 for dirty volume flag
        rd = read(ret, &Fat32_Entry, sizeof(Fat32_Entry));
        if (rd == -1)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat32_Entry error\n", __FILE__);
        if (Fat32_Entry & 0x08000000)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: Volume clean!\n", __FILE__);
        else
            return fs_bad;

        if (Fat32_Entry & 0x04000000)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: I/O correct!\n", __FILE__);
        else
            return fs_error;
    } else if (FS == FAT_12){
        /// FAT[0] contains BPB_Media code
        rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
        log_mesg(2, 0, 0, fs_opt.debug, "%s: Media %x\n", __FILE__, Fat16_Entry);
        if (rd == -1)
            log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat12_Entry error\n", __FILE__);
        rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
    } else
        log_mesg(2, 0, 0, fs_opt.debug, "%s: ERR_WRONG_FS\n", __FILE__);
    return fs_good;

}

/// mark reserved sectors as used
static unsigned long long mark_reserved_sectors(unsigned long* fat_bitmap, unsigned long long block)
{
    unsigned long long i = 0;
    unsigned long long j = 0;
    unsigned long long sec_per_fat = 0;
    unsigned long long root_sec = 0;
    sec_per_fat = get_sec_per_fat();
    root_sec = get_root_sec();

    /// A) the reserved sectors are used
    for (i=0; i < fat_sb.reserved; i++,block++)
        pc_set_bit(block, fat_bitmap);

    /// B) the FAT tables are on used sectors
    for (j=0; j < fat_sb.fats; j++)
        for (i=0; i < sec_per_fat ; i++,block++)
            pc_set_bit(block, fat_bitmap);

    /// C) The rootdirectory is on used sectors
    if (root_sec > 0) /// no rootdir sectors on FAT32
        for (i=0; i < root_sec; i++,block++)
            pc_set_bit(block, fat_bitmap);
    return block;
}

/// open device
static void fs_open(char* device)
{
    int r = 0;
    char *buffer;

    log_mesg(2, 0, 0, fs_opt.debug, "%s: open device\n", __FILE__);
    ret = open(device, O_RDONLY);

    buffer = (char*)malloc(sizeof(FatBootSector));
    if(buffer == NULL){
        log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
    }
    r = read (ret, buffer, sizeof(FatBootSector));
    memcpy(&fat_sb, buffer, sizeof(FatBootSector));
    free(buffer);

    buffer = (char*)malloc(sizeof(FatFsInfo));
    if(buffer == NULL){
        log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
    }
    r = read(ret, &fatfs_info, sizeof(FatFsInfo));
    memcpy(&fatfs_info, buffer, sizeof(FatFsInfo));
    free(buffer);

    log_mesg(2, 0, 0, fs_opt.debug, "%s: open device down\n", __FILE__);

}

/// close device
static void fs_close()
{
    close(ret);
}

/// check per FAT32 entry
unsigned long long check_fat32_entry(unsigned long* fat_bitmap, unsigned long long block, unsigned long long* bfree, unsigned long long* bused, unsigned long long* DamagedClusters)
{
    uint32_t Fat32_Entry = 0;
    int rd = 0;
    unsigned long long i = 0, j = 0;

    rd = read(ret, &Fat32_Entry, sizeof(Fat32_Entry));
    if (rd == -1)
        log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat32_Entry error\n", __FILE__);
    if (Fat32_Entry  == 0x0FFFFFF7) { /// bad FAT32 cluster
        DamagedClusters++;
        log_mesg(2, 0, 0, fs_opt.debug, "%s: bad sec %llu\n", __FILE__, block);
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else if (Fat32_Entry == 0x0000){ /// free
        bfree++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else {
        bused++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_set_bit(block, fat_bitmap);
    }

    return block;
}

/// check per FAT16 entry
unsigned long long check_fat16_entry(unsigned long* fat_bitmap, unsigned long long block, unsigned long long* bfree, unsigned long long* bused, unsigned long long* DamagedClusters)
{
    uint16_t Fat16_Entry = 0;
    int rd = 0;
    unsigned long long i = 0, j = 0;
    rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
    if (rd == -1)
        log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat16_Entry error\n", __FILE__);
    if (Fat16_Entry  == 0xFFF7) { /// bad FAT16 cluster
        DamagedClusters++;
        log_mesg(2, 0, 0, fs_opt.debug, "%s: bad sec %llu\n", __FILE__, block);
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else if (Fat16_Entry == 0x0000){ /// free
        bfree++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else {
        bused++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_set_bit(block, fat_bitmap);
    }
    return block;
}

/// check per FAT12 entry
unsigned long long check_fat12_entry(unsigned long* fat_bitmap, unsigned long long block, unsigned long long* bfree, unsigned long long* bused, unsigned long long* DamagedClusters)
{
    uint16_t Fat16_Entry = 0;
    uint16_t Fat12_Entry = 0;
    int rd = 0;
    unsigned long long i = 0, j = 0;
    rd = read(ret, &Fat16_Entry, sizeof(Fat16_Entry));
    if (rd == -1)
        log_mesg(2, 0, 0, fs_opt.debug, "%s: read Fat12_Entry error\n", __FILE__);
    Fat12_Entry = Fat16_Entry>>4;
    if (Fat12_Entry  == 0xFFF7) { /// bad FAT12 cluster
        DamagedClusters++;
        log_mesg(2, 0, 0, fs_opt.debug, "%s: bad sec %llu\n", __FILE__, block);
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else if (Fat12_Entry == 0x0000){ /// free
        bfree++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_clear_bit(block, fat_bitmap);
    } else {
        bused++;
        for (j=0; j < fat_sb.cluster_size; j++,block++)
            pc_set_bit(block, fat_bitmap);
    }
    return block;
}

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    char sig;
    unsigned long long total_sector = 0;
    unsigned long long bused = 0;
    unsigned long long data_sec = 0;
    unsigned long long sec_per_fat = 0;
    unsigned long long cluster_count = 0;
    unsigned long long free_blocks = 0;

    log_mesg(2, 0, 0, fs_opt.debug, "%s: initial_image start\n", __FILE__);
    fs_open(device);

    get_fat_type();

    total_sector = get_total_sector();

    bused = get_used_block();//so I need calculate by myself.

    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, fat_type, FS_MAGIC_SIZE);
    image_hdr->block_size  = (int)fat_sb.sector_size;
    image_hdr->totalblock  = (unsigned long long)total_sector;
    image_hdr->device_size = (unsigned long long)(total_sector * image_hdr->block_size);
    image_hdr->usedblocks  = (unsigned long long)bused;
    log_mesg(2, 0, 0, fs_opt.debug, "%s: Block Size:%i\n", __FILE__, image_hdr->block_size);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: Total Blocks:%llu\n", __FILE__, image_hdr->totalblock);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: Used Blocks:%llu\n", __FILE__, image_hdr->usedblocks);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: Device Size:%llu\n", __FILE__, image_hdr->device_size);

    fs_close();
    log_mesg(2, 0, 0, fs_opt.debug, "%s: initial_image down\n", __FILE__);
}

/// readbitmap - read and check bitmap
extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{
    unsigned long long i = 0, j = 0;
    int rd = 0;
    int fat_stat = 0;
    unsigned long long block = 0, bfree = 0, bused = 0, DamagedClusters = 0;
    unsigned long long cluster_count = 0;
    unsigned long long total_sector = 0;
    unsigned long long FatReservedBytes = 0;
    uint16_t Fat16_Entry = 0;
    uint32_t Fat32_Entry = 0;
    extern cmd_opt opt;
    int start = 0;
    int bit_size = 1;

    fs_open(device);

    total_sector = get_total_sector();
    cluster_count = get_cluster_count();

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, cluster_count, image_hdr.totalblock, BITMAP, bit_size);

    /// init bitmap
    memset(bitmap, 0xFF, sizeof(unsigned long)*LONGS(total_sector));

    /// A) B) C)
    block = mark_reserved_sectors(bitmap, block);

    /// D) The clusters
    FatReservedBytes = fat_sb.sector_size * fat_sb.reserved;

    /// The first cluster will be seek
    lseek(ret, FatReservedBytes, SEEK_SET);

    /// The second used to check FAT status
    fat_stat = check_fat_status();
    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    }else{
        if (fat_stat == 1)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);
        else if (fat_stat == 2)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: I/O error! %X\n", __FILE__);
    }

    for (i=0; i < cluster_count; i++){
        /// If FAT16
        if(FS == FAT_16){
            block = check_fat16_entry(bitmap, block, &bfree, &bused, &DamagedClusters);
        } else if (FS == FAT_32){ /// FAT32
            block = check_fat32_entry(bitmap, block, &bfree, &bused, &DamagedClusters);
        } else if (FS == FAT_12){ /// FAT12
            block = check_fat12_entry(bitmap, block, &bfree, &bused, &DamagedClusters);
        } else 
            log_mesg(2, 0, 0, fs_opt.debug, "%s: error fs\n", __FILE__);
        /// update progress
        update_pui(&prog, i, i, 0);//keep update
    }

    log_mesg(2, 0, 0, fs_opt.debug, "%s: done\n", __FILE__);
    fs_close();

    /// update progress
    update_pui(&prog, 1, 1, 1);//finish
}

/// get_used_block - get FAT used blocks
static unsigned long long get_used_block()
{
    unsigned long long i = 0;
    int rd = 0;
    int fat_stat = 0;
    unsigned long long block = 0, bfree = 0, bused = 0, DamagedClusters = 0;
    unsigned long long cluster_count = 0, total_sector = 0;
    unsigned long long real_back_block= 0;
    int FatReservedBytes = 0;
    unsigned long *fat_bitmap;

    log_mesg(2, 0, 0, fs_opt.debug, "%s: get_used_block start\n", __FILE__);

    total_sector = get_total_sector();
    cluster_count = get_cluster_count();

    fat_bitmap = (unsigned long *)calloc(sizeof(unsigned long), LONGS(total_sector));
    if (fat_bitmap == NULL)
        log_mesg(2, 1, 1, fs_opt.debug, "%s: bitmapalloc error\n", __FILE__);
    memset(fat_bitmap, 0xFF, sizeof(unsigned long)*LONGS(total_sector));

    /// A) B) C)
    block = mark_reserved_sectors(fat_bitmap, block);

    /// D) The clusters
    FatReservedBytes = fat_sb.sector_size * fat_sb.reserved;

    /// The first fat will be seek
    lseek(ret, FatReservedBytes, SEEK_SET);

    /// The second fat is used to check FAT status
    fat_stat = check_fat_status();
    if (fat_stat == 1)
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Filesystem isn't in valid state. May be it is not cleanly unmounted.\n\n", __FILE__);
    else if (fat_stat == 2)
        log_mesg(0, 1, 1, fs_opt.debug, "%s: I/O error! %X\n", __FILE__);

    for (i=0; i < cluster_count; i++){
        /// If FAT16
        if(FS == FAT_16){
            block = check_fat16_entry(fat_bitmap, block, &bfree, &bused, &DamagedClusters);
        } else if (FS == FAT_32){ /// FAT32
            block = check_fat32_entry(fat_bitmap, block, &bfree, &bused, &DamagedClusters);
        } else if (FS == FAT_12){ /// FAT12
            block = check_fat12_entry(fat_bitmap, block, &bfree, &bused, &DamagedClusters);
        } else 
            log_mesg(2, 0, 0, fs_opt.debug, "%s: error fs\n", __FILE__);
    }

    while(block < total_sector){
        pc_set_bit(block, fat_bitmap);
        block++;
    }


    for (block = 0; block < total_sector; block++)
    {
        if (pc_test_bit(block, fat_bitmap)) {
            real_back_block++;
        }
    }
    free(fat_bitmap);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: get_used_block down\n", __FILE__);

    return real_back_block;
}

