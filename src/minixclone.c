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

#include "partclone.h"
#include "progress.h"
#include "fs_common.h"
#include "minixclone.h"

char *super_block_buffer;
#define inode_in_use(x, map) (isset(map,(x)) != 0)
#define Super (*(struct minix_super_block *) super_block_buffer)
#define Super3 (*(struct minix3_super_block *) super_block_buffer)
#define MAGIC (Super.s_magic)
#define MAGIC3 (Super3.s_magic)

int dev;
int fs_version = 1;

char *EXECNAME = "partclone.minix";

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


static void fs_open(char *device) {
    dev = open(device,O_RDONLY);
    log_mesg(0, 0, 0, fs_opt.debug, "%s: open minix fs\n", __FILE__); 
    if (MINIX_BLOCK_SIZE != lseek(dev, MINIX_BLOCK_SIZE, SEEK_SET))
	log_mesg(0, 1, 1, fs_opt.debug, "%s: seek failed\n", __FILE__);

    super_block_buffer = calloc(1, MINIX_BLOCK_SIZE);
    if (!super_block_buffer)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: unable to alloc buffer for superblock", __FILE__);

    if (MINIX_BLOCK_SIZE != read(dev, super_block_buffer, MINIX_BLOCK_SIZE))
	log_mesg(0, 1, 1, fs_opt.debug, "%s: unable to read super block", __FILE__);
}

static void fs_close(){
    close(dev);   
}

static unsigned long count_used_block(){
    unsigned long zones = get_nzones();
    unsigned long imaps = get_nimaps();
    unsigned long zmaps = get_nzmaps();
    char * inode_map;
    char * zone_map;
    ssize_t rc;
    unsigned long test_block = 0, test_zone = 0;
    unsigned long used_block = 0;
    unsigned long block_size = get_block_size();

    if (fs_version == 3){
	if (lseek(dev, block_size*2, SEEK_SET) != 8192)
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: seek failed", __FILE__);
    }

    inode_map = malloc(imaps * block_size);
    if (!inode_map)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to allocate buffer for inode map", __FILE__);
    zone_map = malloc(zmaps * block_size);
    if (!inode_map)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to allocate buffer for zone map", __FILE__);
    memset(inode_map,0,sizeof(inode_map));
    memset(zone_map,0,sizeof(zone_map));

    rc = read(dev, inode_map, imaps * block_size);
    if (rc < 0 || imaps * block_size != (size_t) rc)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to read inode map", __FILE__);

    rc = read(dev, zone_map, zmaps * block_size);
    if (rc < 0 || zmaps * block_size != (size_t) rc)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to read zone map", __FILE__);

    for (test_block = 0; test_block < zones; test_block++){
	test_zone = test_block - get_first_zone()+1;
	if ((test_zone < 0) || (test_zone > zones+get_first_zone()))
	    test_zone = 0;
	if(isset(zone_map,test_zone)){
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu in use\n", __FILE__, test_block);    
	    used_block++;
	}else{
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu not use\n", __FILE__, test_block);    
	}
    }
    return used_block;
}


void initial_image_hdr(char* device, image_head* image_hdr) {
    fs_open(device);
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
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, minix_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = get_block_size();
    image_hdr->totalblock  = get_nzones();
    image_hdr->usedblocks  = count_used_block();
    image_hdr->device_size = get_nzones()*get_block_size();
    fs_close();
}



void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui) {
    unsigned long zones = get_nzones();
    unsigned long imaps = get_nimaps();
    unsigned long zmaps = get_nzmaps();
    char * inode_map;
    char * zone_map;
    ssize_t rc;
    unsigned long test_block = 0, test_zone = 0;

    fs_open(device);

    unsigned long block_size = get_block_size();

    if (fs_version == 3){
	if (lseek(dev, block_size*2, SEEK_SET) != 8192)
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: seek failed", __FILE__);
    }

    inode_map = malloc(imaps * block_size);
    if (!inode_map)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to allocate buffer for inode map", __FILE__);
    zone_map = malloc(zmaps * block_size);
    if (!inode_map)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to allocate buffer for zone map", __FILE__);
    memset(inode_map,0,sizeof(inode_map));
    memset(zone_map,0,sizeof(zone_map));
    pc_init_bitmap(bitmap, 0x00, image_hdr.totalblock);

    rc = read(dev, inode_map, imaps * block_size);
    if (rc < 0 || imaps * block_size != (size_t) rc)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to read inode map", __FILE__);

    rc = read(dev, zone_map, zmaps * block_size);
    if (rc < 0 || zmaps * block_size != (size_t) rc)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to read zone map", __FILE__);

    log_mesg(0, 0, 0, fs_opt.debug, "%s: %ld blocks\n", __FILE__, zones);
    log_mesg(0, 0, 0, fs_opt.debug, "%s: log2 block/zone: %lu\n", __FILE__, get_zone_size());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: Zonesize=%d\n", __FILE__,block_size<<get_zone_size());
    log_mesg(0, 0, 0, fs_opt.debug, "%s: Maxsize=%ld\n", __FILE__, get_max_size());


    for (test_block = 0; test_block < zones; test_block++){
	test_zone = test_block - get_first_zone()+1;
	if ((test_zone < 0) || (test_zone > zones+get_first_zone()))
	    test_zone = 0;
	if(isset(zone_map,test_zone)){
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu in use\n", __FILE__, test_block);    
	    pc_set_bit(test_block, bitmap);
	}else{
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: test_block %lu not use\n", __FILE__, test_block);    
	}
    }
    fs_close();
}


