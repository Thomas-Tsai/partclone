/**
 * minixclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read minix super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 **/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/param.h>
#include <stdint.h>

#include "partclone.h"
#include "progress.h"
#include "fs_common.h"
#include "minixclone.h"

#define MAX_MINIX_TOTAL_BLOCKS (1ULL << 32) // Max blocks (approx 4.3 billion) for Minix V3, to prevent DoS.
#define MIN_MINIX_BLOCK_SIZE 1024 // Smallest supported block size
#define MAX_MINIX_BLOCK_SIZE 4096 // Largest supported block size
#define MAX_MINIX_MAP_SIZE_BYTES (1ULL << 24) // Max 16MB for inode/zone map (generous upper bound)

char *super_block_buffer;
#define inode_in_use(x, map) (isset(map,(x)) != 0)
#define Super (*(struct minix_super_block *) super_block_buffer)
#define Super3 (*(struct minix3_super_block *) super_block_buffer)
#define MAGIC (Super.s_magic)
#define MAGIC3 (Super3.s_magic)

int dev;
int fs_version = 1;

static inline unsigned long get_max_size(void)
{
    switch (fs_version) {
	case 3:
	    return (unsigned long)Super3.s_max_size;
	default:
	    return (unsigned long)Super.s_max_size;
    }
}    

static inline unsigned long get_ninodes(void)
{
    switch (fs_version) {
	case 3:
	    return Super3.s_ninodes;
	default:
	    return (unsigned long)Super.s_ninodes;
    }
}

static inline unsigned long get_nzones(void)
{       
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_zones;
	case 2: 
	    return (unsigned long)Super.s_zones;
	default:
	    return (unsigned long)Super.s_nzones;
    }
}


static inline unsigned long get_nimaps(void)
{               
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_imap_blocks;
	default:
	    return (unsigned long)Super.s_imap_blocks;
    }
}               

static inline unsigned long get_nzmaps(void)
{               
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_zmap_blocks;
	default:
	    return (unsigned long)Super.s_zmap_blocks;
    }
}       


static inline unsigned long get_zone_size(void)
{               
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_log_zone_size;
	default:
	    return (unsigned long)Super.s_log_zone_size;
    }
}


static inline unsigned long get_block_size(void)
{
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_blocksize;
	default:
	    return MINIX_BLOCK_SIZE;
    }
}


static inline unsigned long get_first_zone(void)
{
    switch (fs_version) {
	case 3: 
	    return (unsigned long)Super3.s_firstdatazone;
	default:
	    return (unsigned long)Super.s_firstdatazone;
    }
}


static int fs_open(char *device) {
    dev = open(device,O_RDONLY);
    if (dev < 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Cannot open device %s: %s\n", __FILE__, device, strerror(errno));
        return -1;
    }
    log_mesg(0, 0, 0, fs_opt.debug, "%s: open minix fs\n", __FILE__); 
    if (MINIX_BLOCK_SIZE != lseek(dev, MINIX_BLOCK_SIZE, SEEK_SET)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: seek failed: %s\n", __FILE__, strerror(errno));
        close(dev);
        return -1;
    }

    super_block_buffer = calloc(1, MINIX_BLOCK_SIZE);
    if (!super_block_buffer) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: unable to alloc buffer for superblock: %s\n", __FILE__, strerror(errno));
        close(dev);
        return -1;
    }

    if (MINIX_BLOCK_SIZE != read(dev, super_block_buffer, MINIX_BLOCK_SIZE)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: unable to read super block: %s\n", __FILE__, strerror(errno));
        free(super_block_buffer);
        close(dev);
        return -1;
    }
    return 0; // Success
}
static void fs_close(){
    if (super_block_buffer) {
        free(super_block_buffer);
        super_block_buffer = NULL;
    }
    close(dev);
}

static int count_used_block(unsigned long *used_block_out){ 
    unsigned long zones = get_nzones();
    unsigned long imaps = get_nimaps();
    unsigned long zmaps = get_nzmaps();
    char *inode_map = NULL;
    char *zone_map = NULL;
    ssize_t rc;
    unsigned long test_block = 0, test_zone = 0;
    unsigned long current_used_block_count = 0;
    unsigned long block_size = get_block_size();

    if (block_size == 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Invalid block size (0) before map allocation.\n", __FILE__);
        return -1;
    }
    if (zones == 0 || zones > MAX_MINIX_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero number of zones detected: %lu. Max allowed: %llu\n",
                 __FILE__, zones, MAX_MINIX_TOTAL_BLOCKS);
        return -1;
    }

    if (fs_version == 3){
        if (lseek(dev, block_size*2, SEEK_SET) != 8192) { // 8192 is fixed for V3 superblock
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: V3 seek failed: %s\n", __FILE__, strerror(errno));
            return -1;
        }
    }

    unsigned long inode_map_size = imaps * block_size;
    unsigned long zone_map_size = zmaps * block_size;

    if (imaps == 0 || imaps * block_size < imaps || inode_map_size > MAX_MINIX_MAP_SIZE_BYTES) { 
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero inode map size detected: %lu. Max allowed: %llu\n", __FILE__, inode_map_size, MAX_MINIX_MAP_SIZE_BYTES);
        return -1;
    }
    if (zmaps == 0 || zmaps * block_size < zmaps || zone_map_size > MAX_MINIX_MAP_SIZE_BYTES) { 
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero zone map size detected: %lu. Max allowed: %llu\n", __FILE__, zone_map_size, MAX_MINIX_MAP_SIZE_BYTES);
        return -1;
    }

    inode_map = (char *)calloc(1, inode_map_size);
    if (!inode_map) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to allocate buffer for inode map: %s\n", __FILE__, strerror(errno));
        return -1;
    }
    zone_map = (char *)calloc(1, zone_map_size);
    if (!zone_map) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to allocate buffer for zone map: %s\n", __FILE__, strerror(errno));
        free(inode_map);
        return -1;
    }

    rc = read(dev, inode_map, inode_map_size);
    if (rc < 0 || inode_map_size != (size_t) rc) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to read inode map (rc=%zd, expected=%lu): %s\n", __FILE__, rc, inode_map_size, strerror(errno));
        free(inode_map); free(zone_map);
        return -1;
    }

    rc = read(dev, zone_map, zone_map_size);
    if (rc < 0 || zone_map_size != (size_t) rc) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to read zone map (rc=%zd, expected=%lu): %s\n", __FILE__, rc, zone_map_size, strerror(errno));
        free(inode_map); free(zone_map);
        return -1;
    }

    for (test_block = 0; test_block < zones; test_block++){
	test_zone = test_block - get_first_zone()+1;
	if ((test_zone < 0) || (test_zone > zones+get_first_zone())) {
	    test_zone = 0;
        }
        if (test_zone / 8 >= zone_map_size) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: test_zone (%lu) out of bounds for zone map (size %lu).\n", __FILE__, test_zone, zone_map_size);
            free(inode_map); free(zone_map);
            return -1;
        }

	if(isset(zone_map,test_zone)){
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu in use\n", __FILE__, test_block);    
	    current_used_block_count++;
	}else{
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu not use\n", __FILE__, test_block);    
	}
    }
    free(zone_map);
    free(inode_map);
    *used_block_out = current_used_block_count;
    return 0; // Success
}


void read_super_blocks(char* device, file_system_info* fs_info) {
    if (fs_open(device) != 0) { // Check return value
        return; // Abort if fs_open failed
    }
    if (MAGIC == MINIX_SUPER_MAGIC) {
	fs_version = 1;
    } else if (MAGIC == MINIX_SUPER_MAGIC2) {
	fs_version = 1;
    } else if (MAGIC == MINIX2_SUPER_MAGIC) {
	fs_version = 2;
    } else if (MAGIC == MINIX2_SUPER_MAGIC2) {
	fs_version = 2;
    } else if (MAGIC3 == MINIX3_SUPER_MAGIC){
	fs_version = 3;
    } else
	log_mesg(0, 1, 1, fs_opt.debug, "%s: bad magic number in super-block", __FILE__);

    log_mesg(0, 0, 0, fs_opt.debug, "%s: get_first_zone %lu\n", __FILE__, get_first_zone());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: get_nzones %lu\n", __FILE__, get_nzones());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: zones map size %lu\n", __FILE__, get_nzmaps());
    unsigned long detected_used_blocks;
    strncpy(fs_info->fs, minix_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = get_block_size();
    fs_info->totalblock  = get_nzones();

    if (fs_info->block_size < MIN_MINIX_BLOCK_SIZE || fs_info->block_size > MAX_MINIX_BLOCK_SIZE || (fs_info->block_size & (fs_info->block_size - 1)) != 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Invalid block size detected in superblock: %lu.\n", fs_info->block_size);
        fs_close();
        return;
    }
    if (fs_info->totalblock == 0 || fs_info->totalblock > MAX_MINIX_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Maliciously large or zero total blocks detected in superblock: %lu. Max allowed: %llu\n", fs_info->totalblock, MAX_MINIX_TOTAL_BLOCKS);
        fs_close();
        return;
    }
    if (fs_info->block_size == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow or division by zero in device size calculation. totalblock: %lu, block_size: %lu\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->totalblock * fs_info->block_size;

    if (count_used_block(&detected_used_blocks) != 0) {
        fs_close();
        return;
    }
    fs_info->usedblocks  = detected_used_blocks;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_close();
}



void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui) {
    if (fs_open(device) != 0) {
        return;
    }

    unsigned long zones = get_nzones();
    unsigned long imaps = get_nimaps();
    unsigned long zmaps = get_nzmaps();
    char * inode_map = NULL;
    char * zone_map = NULL;
    ssize_t rc;
    unsigned long test_block = 0, test_zone = 0;

    unsigned long block_size = get_block_size();

    if (block_size == 0) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Invalid block size (0) before map allocation.\n", __FILE__);
        fs_close();
        return;
    }
    if (zones == 0 || zones > MAX_MINIX_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero number of zones detected: %lu. Max allowed: %llu\n",
                 __FILE__, zones, MAX_MINIX_TOTAL_BLOCKS);
        fs_close();
        return;
    }

    if (fs_version == 3){
        if (lseek(dev, block_size*2, SEEK_SET) != 8192) { // 8192 is fixed for V3 superblock
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: V3 seek failed: %s\n", __FILE__, strerror(errno));
            fs_close();
            return;
        }
    }

    unsigned long inode_map_size = imaps * block_size;
    unsigned long zone_map_size = zmaps * block_size;

    if (imaps == 0 || imaps * block_size < imaps || inode_map_size > MAX_MINIX_MAP_SIZE_BYTES) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero inode map size detected: %lu. Max allowed: %llu\n", __FILE__, inode_map_size, MAX_MINIX_MAP_SIZE_BYTES);
        fs_close();
        return;
    }
    if (zmaps == 0 || zmaps * block_size < zmaps || zone_map_size > MAX_MINIX_MAP_SIZE_BYTES) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero zone map size detected: %lu. Max allowed: %llu\n", __FILE__, zone_map_size, MAX_MINIX_MAP_SIZE_BYTES);
        fs_close();
        return;
    }

    inode_map = (char *)calloc(1, inode_map_size);
    if (!inode_map) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to allocate buffer for inode map: %s\n", __FILE__, strerror(errno));
        fs_close();
        return;
    }
    zone_map = (char *)calloc(1, zone_map_size);
    if (!zone_map) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to allocate buffer for zone map: %s\n", __FILE__, strerror(errno));
        free(inode_map);
        fs_close();
        return;
    }

    rc = read(dev, inode_map, inode_map_size);
    if (rc < 0 || inode_map_size != (size_t) rc) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to read inode map (rc=%zd, expected=%lu): %s\n", __FILE__, rc, inode_map_size, strerror(errno));
        free(inode_map); free(zone_map);
        fs_close();
        return;
    }

    rc = read(dev, zone_map, zone_map_size);
    if (rc < 0 || zone_map_size != (size_t) rc) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Unable to read zone map (rc=%zd, expected=%lu): %s\n", __FILE__, rc, zone_map_size, strerror(errno));
        free(inode_map); free(zone_map);
        fs_close();
        return;
    }

    log_mesg(0, 0, 0, fs_opt.debug, "%s: %ld blocks\n", __FILE__, zones);
    log_mesg(0, 0, 0, fs_opt.debug, "%s: log2 block/zone: %lu\n", __FILE__, get_zone_size());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: Zonesize=%d\n", __FILE__,block_size<<get_zone_size());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: Maxsize=%ld\n", __FILE__, get_max_size());


    for (test_block = 0; test_block < zones; test_block++){
	test_zone = test_block - get_first_zone()+1;
	if ((test_zone < 0) || (test_zone > zones+get_first_zone())) {
	    test_zone = 0;
        }
        if (test_zone / 8 >= zone_map_size) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: test_zone (%lu) out of bounds for zone map (size %lu).\n", __FILE__, test_zone, zone_map_size);
            free(inode_map); free(zone_map);
            fs_close();
            return;
        }

	if(isset(zone_map,test_zone)){
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu in use\n", __FILE__, test_block);    
	    pc_set_bit(test_block, bitmap, fs_info.totalblock);
	}else{
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu not use\n", __FILE__, test_block);    
	}
    }
    free(zone_map);
    free(inode_map);
    fs_close();
}


