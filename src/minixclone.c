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

int dev;
int fs_version = 1;

char *EXECNAME = "partclone.minix";
extern fs_cmd_opt fs_opt;

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
    log_mesg(2, 0, 0, fs_opt.debug, "%s: open minix fs\n", __FILE__); 
    if (MINIX_BLOCK_SIZE != lseek(dev, MINIX_BLOCK_SIZE, SEEK_SET))
	printf("seek failed");

    super_block_buffer = calloc(1, MINIX_BLOCK_SIZE);
    if (!super_block_buffer)
	printf("unable to alloc buffer for superblock");

    if (MINIX_BLOCK_SIZE != read(dev, super_block_buffer, MINIX_BLOCK_SIZE))
	printf("unable to read super block");
}

static void fs_close(){
    close(dev);   
}

extern void initial_image_hdr(char* device, image_head* image_hdr){
    fs_open(device);
    if (MAGIC == MINIX_SUPER_MAGIC) {
	fs_version = 1;
    } else if (MAGIC == MINIX_SUPER_MAGIC2) {
	fs_version = 1;
    } else if (MAGIC == MINIX2_SUPER_MAGIC) {
	fs_version = 2;
    } else if (MAGIC == MINIX2_SUPER_MAGIC2) {
	fs_version = 2;
    } else
	printf("bad magic number in super-block");

    if (get_zone_size() != 0 || MINIX_BLOCK_SIZE != 1024)
	printf("Only 1k blocks/zones supported");
    if (get_nimaps() * MINIX_BLOCK_SIZE * 8 < get_ninodes() + 1)
	printf("bad s_imap_blocks field in super-block");
    if (get_nzmaps() * MINIX_BLOCK_SIZE * 8 < get_nzones() - get_first_zone() + 1)
	printf("bad s_zmap_blocks field in super-block");

    printf("get_first_zone %lu\n", get_first_zone());
    printf("get_nzones %lu\n", get_nzones());
    printf("zones map size %lu\n", get_nzmaps());
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, minix_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = MINIX_BLOCK_SIZE;
    image_hdr->totalblock  = get_nzones();
    image_hdr->usedblocks  = get_nzones();
    image_hdr->device_size = get_nzones()*MINIX_BLOCK_SIZE;
    fs_close();
}

extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui){
    unsigned long first_zone = get_first_zone();
    unsigned long zones = get_nzones();
    unsigned long imaps = get_nimaps();
    unsigned long zmaps = get_nzmaps();
    char * inode_map;
    char * zone_map;
    ssize_t rc;
    unsigned long test_block = 0, test_inode = 0, test_zone = 0;

    fs_open(device);
    inode_map = malloc(imaps * MINIX_BLOCK_SIZE);
    if (!inode_map)
	printf("Unable to allocate buffer for inode map");
    zone_map = malloc(zmaps * MINIX_BLOCK_SIZE);
    if (!inode_map)
	printf("Unable to allocate buffer for zone map");
    memset(inode_map,0,sizeof(inode_map));
    memset(zone_map,0,sizeof(zone_map));

    rc = read(dev, inode_map, imaps * MINIX_BLOCK_SIZE);
    if (rc < 0 || imaps * MINIX_BLOCK_SIZE != (size_t) rc)
	printf("Unable to read inode map");

    rc = read(dev, zone_map, zmaps * MINIX_BLOCK_SIZE);
    if (rc < 0 || zmaps * MINIX_BLOCK_SIZE != (size_t) rc)
	printf("Unable to read zone map");

    printf("%ld blocks\n", zones);
    printf("log2 block/zone: %lu\n", get_zone_size());
    printf("Zonesize=%d\n",MINIX_BLOCK_SIZE<<get_zone_size());
    printf("Maxsize=%ld\n", get_max_size());
    printf("Filesystem state=%d\n", Super.s_state);

    for (test_inode = 0; test_inode < imaps; test_inode++){
	if(isset(inode_map,test_inode)){
	    printf("test_inode %lu in use\n", test_inode);    
	}else{
	    printf("test_inode %lu not use\n", test_inode);    
	}
    }
/*
    for (test_block = 0; test_block <=get_first_zone(); test_block++){
	pc_set_bit(test_block, bitmap);
    }
*/

    for (test_block = 0; test_block < zones; test_block++){
	test_zone = test_block - get_first_zone()+1;
	if ((test_zone < 0) || (test_zone > zones+get_first_zone()))
	    test_zone = 0;
	if(isset(zone_map,test_zone)){
	    printf("test_block %lu in use\n", test_block);    
	    pc_set_bit(test_block, bitmap);
	}else{
	    printf("test_block %lu not use\n", test_block);    
	}
    }
    fs_close();
}


