/**
 hfsplusclone.c - part of Partclone project
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
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/types.h>

#include "partclone.h"
#include "hfsplusclone.h"

struct HFSPlusVolumeHeader sb;
int ret;
char *EXECNAME = "clone.hfsplus";

static short reverseShort(short s){
    unsigned char c1, c2;

    c1 = s & 255;
    c2 = (s >> 8) & 255;

    return (c1 << 8)+c2;
}

static int reverseInt(int i){
    unsigned char c1, c2, c3, c4;
    c1 = i & 255;
    c2 = (i >> 8) & 255;
    c3 = (i >> 16)& 255;
    c4 = (i >> 24)& 255;

    return ((int)c1<<24)+((int)c2<<16)+((int)c3<<8)+c4;
}

static int IsAllocationBlockUsed(UInt32 thisAllocationBlock, UInt8* allocationFileContents)
{
        UInt8 thisByte;
	int debug = 1;

        thisByte = allocationFileContents[thisAllocationBlock / 8];
	//log_mesg(0, 0, 0, debug, "IsAB:%i\n", (thisByte & (1 << (7 - (thisAllocationBlock % 8)))));
        return (thisByte & (1 << (7 - (thisAllocationBlock % 8)))) != 0;
}

static void print_fork_data(HFSPlusForkData* fork){
    int i = 0;
    int debug = 2;

    HFSPlusExtentDescriptor* exten;
    log_mesg(2, 0, 0, debug, "logicalSize: %#lx\n", fork->logicalSize);
    log_mesg(2, 0, 0, debug, "clumpSize: %i\n", reverseInt(fork->clumpSize));
    log_mesg(2, 0, 0, debug, "totalBlocks: %i\n", reverseInt(fork->totalBlocks));
    for (i = 0; i < 8; i++ ){
        exten = &fork->extents[i];
        log_mesg(2, 0, 0, debug, "\texten %i startBlock: %i\n", i, reverseInt(exten->startBlock));
        log_mesg(2, 0, 0, debug, "\texten %i blockCount: %i\n", i, reverseInt(fork->extents[i].blockCount));

    }
}

/// open device
static void fs_open(char* device){

    int s, r;
    char *buffer;

    ret = open(device, O_RDONLY);
    s = lseek(ret, 1024, SEEK_SET);
    buffer = (char*)malloc(sizeof(HFSPlusVolumeHeader));
    r = read (ret, buffer, sizeof(HFSPlusVolumeHeader));
    memcpy(&sb, buffer, sizeof(HFSPlusVolumeHeader));
    free(buffer);

}

static void fs_close(){

    close(ret);

}

extern void readbitmap(char* device, image_head image_hdr, char* bitmap){

    int r, IsUsed = 0, s, i;
    UInt8 *buffer2;
    long int tb = 0, rb = 0, bused = 0, bfree = 0;
    UInt32 b;
    int debug = 2;

    fs_open(device);
    tb = reverseInt((int)sb.totalBlocks);
    rb = (tb/8)+1;
    //rb = 8192;

    for (i = 0; i < tb; i++)
	bitmap[i] = 1;

    s = lseek(ret, 2560, SEEK_CUR);
    buffer2 = (UInt8*)malloc(rb);
    r = read (ret, buffer2, rb);
    for(b = 0 ; b < tb; b++){
	int check_block = b;
        IsUsed = IsAllocationBlockUsed(check_block, buffer2);
    if (IsUsed){
            bused++;
            bitmap[b] = 1;
	    log_mesg(3, 0, 0, debug, "used b = %i\n", b);
        }else{
            bfree++;
            bitmap[b] = 0;
	    log_mesg(3, 0, 0, debug, "free b = %i\n", b);
        }
    }
    
    log_mesg(2, 0, 0, 1, "rb:%i\n", rb);
    log_mesg(2, 0, 0, 1, "bfree:%i\n", bfree);
    log_mesg(2, 0, 0, 1, "bused:%i\n", bused);
    if(bfree != reverseInt((int)sb.freeBlocks))
        log_mesg(0, 1, 1, debug, "bitmap free count err, free:%i\n", bfree);


    free(buffer2);
    fs_close();
}

extern void initial_image_hdr(char* device, image_head* image_hdr)
{

    fs_open(device);
    memcpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    memcpy(image_hdr->fs, hfsplus_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = reverseInt(sb.blockSize);
    image_hdr->device_size = reverseInt(sb.totalBlocks)*reverseInt(sb.blockSize);
    image_hdr->totalblock  = reverseInt(sb.totalBlocks);
    image_hdr->usedblocks  = reverseInt(sb.totalBlocks) - reverseInt(sb.freeBlocks);
    log_mesg(2, 0, 0, 2, "blockSize:%i\n", reverseInt(sb.blockSize));
    log_mesg(2, 0, 0, 2, "totalBlocks:%i\n", reverseInt(sb.totalBlocks));
    log_mesg(2, 0, 0, 2, "freeBlocks:%i\n", reverseInt(sb.freeBlocks));
    print_fork_data(&sb.allocationFile);
    print_fork_data(&sb.extentsFile);
    print_fork_data(&sb.catalogFile);
    print_fork_data(&sb.attributesFile);
    print_fork_data(&sb.startupFile);
    fs_close();
}
