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
#include "progress.h"
#include "fs_common.h"

struct HFSPlusVolumeHeader sb;
static struct HFSVolumeHeader hsb;
int ret;

static int IsAllocationBlockUsed(UInt32 thisAllocationBlock, UInt8* allocationFileContents)
{
    UInt8 thisByte;

    thisByte = allocationFileContents[thisAllocationBlock / 8];
    //log_mesg(0, 0, 0, fs_opt.debug, "IsAB:%i\n", (thisByte & (1 << (7 - (thisAllocationBlock % 8)))));
    return (thisByte & (1 << (7 - (thisAllocationBlock % 8)))) != 0;
}

static void print_fork_data(HFSPlusForkData* fork){
    int i = 0;

    HFSPlusExtentDescriptor* exten;
    log_mesg(2, 0, 0, fs_opt.debug, "%s: logicalSize: %#lx\n", __FILE__, fork->logicalSize);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: clumpSize: %i\n", __FILE__, be32toh(fork->clumpSize));
    log_mesg(2, 0, 0, fs_opt.debug, "%s: totalBlocks: %i\n", __FILE__, be32toh(fork->totalBlocks));
    for (i = 0; i < 8; i++ ){
        exten = &fork->extents[i];
        log_mesg(2, 0, 0, fs_opt.debug, "%s: \texten %i startBlock: %i\n", __FILE__, i, be32toh(exten->startBlock));
        log_mesg(2, 0, 0, fs_opt.debug, "%s: \texten %i blockCount: %i\n", __FILE__, i, be32toh(fork->extents[i].blockCount));

    }
}

// Find the offset of the embedded HFS+ volume inside an HFS wrapper
static UInt64 hfs_embed_offset() {
    int wrapper_block_size;

    wrapper_block_size = be32toh(hsb.allocationBlockSize);
    return (UInt64)be16toh(hsb.firstAllocationBlock) * PART_SECTOR_SIZE +
        be16toh(hsb.embedExtent.startBlock) * wrapper_block_size;
}

// Try to use this device as an HFS+ volume embedded in an HFS wrapper
static void open_wrapped_volume(char *device, short *signature, char *buffer) {
    short wrapper_signature;
    int hfsp_block_size, wrapper_block_size;
    UInt64 embed_offset, hfsp_sb_offset;
    UInt64 embed_size, hfsp_size;

    memcpy(&hsb, &sb, sizeof(HFSVolumeHeader)); // HFS header is always smaller, so just copy it
    wrapper_signature = be16toh(hsb.embedSignature);
    if (wrapper_signature != HFSPlusSignature) // Make sure embedSignature is right, otherwise this isn't HFS+ at all
        log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus volume is really just HFS, can't clone that.\n", __FILE__);

    // Find the embedded volume, and copy its superblock into sb
    embed_offset = hfs_embed_offset();
    hfsp_sb_offset = embed_offset + HFSHeaderOffset;
    if (lseek(ret, hfsp_sb_offset, SEEK_SET) != hfsp_sb_offset)
        log_mesg(0, 1, 1, fs_opt.debug, "%s: failed seeking to embedded HFS Plus superblock at offset %lu on device %s.\n", __FILE__, hfsp_sb_offset, device);
    if (read(ret, buffer, sizeof(HFSPlusVolumeHeader)) != sizeof(HFSPlusVolumeHeader))
        log_mesg(0, 1, 1, fs_opt.debug, "%s: read embedded HFSPlusVolumeHeader fail.\n", __FILE__);
    memcpy(&sb, buffer, sizeof(HFSPlusVolumeHeader));
    *signature = be16toh(sb.signature);

    if (*signature == HFSPlusSignature) { // If this fails, fs_open will soon error because of the bad signature
        log_mesg(0, 0, 0, fs_opt.debug, "%s: HFS_Plus embedded volume found at offset %lu.\n", __FILE__, embed_offset);

        // Do some sanity checks
        hfsp_block_size = be32toh(sb.blockSize);
        wrapper_block_size = be32toh(hsb.allocationBlockSize);
        if (wrapper_block_size % hfsp_block_size != 0)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus wrapper block size %u is not a multiple of block size %u.\n", __FILE__, wrapper_block_size, hfsp_block_size);
        if (embed_offset % hfsp_block_size != 0)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus embedded volume offset %lu is not a multiple of block size %u.\n", __FILE__, embed_offset, hfsp_block_size);

        embed_size = (UInt64)wrapper_block_size * (UInt16)be16toh(hsb.embedExtent.blockCount);
        hfsp_size = (UInt64)hfsp_block_size * be32toh(sb.totalBlocks);
        if (embed_size != hfsp_size)
            log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus embedded volume size %lu doesn't match wrapper embed size %lu.\n", __FILE__, hfsp_size, embed_size);
    }

    // TODO: Actually support this!
    log_mesg(0, 1, 1, fs_opt.debug, "%s: Wrapped HFS+ not yet supported!\n", __FILE__);
}

/// open device
static void fs_open(char* device){

    char *buffer;
    short HFS_Version;
    short HFS_Signature;
    int HFS_Clean = 0;

    ret = open(device, O_RDONLY);
    if(lseek(ret, HFSHeaderOffset, SEEK_SET) != HFSHeaderOffset)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: device %s seek fail\n", __FILE__, device);
	
    buffer = (char*)malloc(sizeof(HFSPlusVolumeHeader));
    if (read (ret, buffer, sizeof(HFSPlusVolumeHeader)) != sizeof(HFSPlusVolumeHeader))
	log_mesg(0, 1, 1, fs_opt.debug, "%s: read HFSPlusVolumeHeader fail\n", __FILE__);
    memcpy(&sb, buffer, sizeof(HFSPlusVolumeHeader));

    HFS_Signature = be16toh(sb.signature);
    
    if(HFS_Signature == HFSSignature) {
        open_wrapped_volume(device, &HFS_Signature, buffer);
    }

    if(HFS_Signature != HFSPlusSignature && HFS_Signature != HFSXSignature){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus incorrect signature '%c%c'\n", __FILE__,
            (char)(HFS_Signature >> 8), (char)HFS_Signature);
    }

    HFS_Version = (short)be16toh(sb.version);
    HFS_Clean = (be32toh(sb.attributes)>>8) & 1;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: Signature=%c%c\n", __FILE__,
        (char)(HFS_Signature >> 8), (char)HFS_Signature);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: Version=%i\n", __FILE__, HFS_Version);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: Attr-Unmounted=%i(1 is clean, 0 is dirty)\n", __FILE__, HFS_Clean);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: Attr-Inconsistent=%i\n", __FILE__, (be32toh(sb.attributes)>>11) & 1);

    if(fs_opt.ignore_fschk){
        log_mesg(1, 0, 0, fs_opt.debug, "%s: Ignore filesystem check\n", __FILE__);
    } else {
        if (HFS_Clean)
            log_mesg(3, 0, 0, fs_opt.debug, "%s: HFS_Plus '%s' is clean\n", __FILE__, device);
        else 
            log_mesg(0, 1, 1, fs_opt.debug, "%s: HFS_Plus Volume '%s' is scheduled for a check or it was shutdown\nuncleanly. Please fix it by fsck.\n", __FILE__, device);
    }

    free(buffer);

}

static void fs_close(){

    close(ret);

}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui) {

    int IsUsed = 0;
    UInt8 *extent_bitmap;
    UInt32 block_size = 0;
    UInt32 bused = 0, bfree = 0, mused = 0;
    UInt32 block = 0, extent_block = 0, tb = 0;
    int allocation_exten = 0;
    UInt64 allocation_start_block;
    UInt32 allocation_block_size;
    int start = 0;
    int bit_size = 1;


    fs_open(device);
    tb = be32toh(sb.totalBlocks);
    block_size = be32toh(sb.blockSize);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    pc_init_bitmap(bitmap, 0xFF, tb);

    for (allocation_exten = 0; allocation_exten <= 7; allocation_exten++){
        allocation_start_block = block_size*be32toh(sb.allocationFile.extents[allocation_exten].startBlock);

        allocation_block_size = block_size*be32toh(sb.allocationFile.extents[allocation_exten].blockCount);
        log_mesg(2, 0, 0, 2, "%s: tb = %lu\n", __FILE__, tb);
        log_mesg(2, 0, 0, 2, "%s: extent_block = %lu\n", __FILE__, extent_block);
        log_mesg(2, 0, 0, 2, "%s: allocation_exten = %i\n", __FILE__, allocation_exten);
        log_mesg(2, 0, 0, 2, "%s: allocation_start_block = %lu\n", __FILE__, allocation_start_block);
        log_mesg(2, 0, 0, 2, "%s: allocation_block_size = %lu\n", __FILE__, allocation_block_size);

        if((allocation_start_block == 0) && (allocation_block_size == 0)){
            continue;
        }

        if(lseek(ret, allocation_start_block, SEEK_SET) != allocation_start_block)
	     log_mesg(0, 1, 1, fs_opt.debug, "%s: start_block %i seek fail\n", __FILE__, allocation_start_block);
        extent_bitmap = (UInt8*)malloc(allocation_block_size);
        if(read(ret, extent_bitmap, allocation_block_size) != allocation_block_size)
	    log_mesg(0, 0, 1, fs_opt.debug, "%s: read hfsp bitmap fail\n", __FILE__);
        for(extent_block = 0 ; (extent_block < allocation_block_size*8) && (block< tb); extent_block++){
            IsUsed = IsAllocationBlockUsed(extent_block, extent_bitmap);
            if (IsUsed){
                bused++;
                pc_set_bit(block, bitmap, fs_info.totalblock);
                log_mesg(3, 0, 0, fs_opt.debug, "%s: used block= %i\n", __FILE__, block);
            } else {
                bfree++;
                pc_clear_bit(block, bitmap, fs_info.totalblock);
                log_mesg(3, 0, 0, fs_opt.debug, "%s: free block= %i\n", __FILE__, block);
            }
            block++;
            /// update progress
            update_pui(&prog, block, block, 0);

        }
        log_mesg(2, 0, 0, 2, "%s: next exten\n", __FILE__);
        log_mesg(2, 0, 0, 2, "%s: extent_bitmap:%i\n", __FILE__, extent_bitmap);
        free(extent_bitmap);
        log_mesg(2, 0, 0, 2, "%s: bfree:%i\n", __FILE__, bfree);
        log_mesg(2, 0, 0, 2, "%s: bused:%i\n", __FILE__, bused);
    }
    mused = (be32toh(sb.totalBlocks) - be32toh(sb.freeBlocks));
    if(bused != mused)
        log_mesg(0, 1, 1, fs_opt.debug, "%s: bitmap count error, used:%lu, mbitmap:%lu\n", __FILE__, bused, mused);

    fs_close();
    /// update progress
    update_pui(&prog, 1, 1, 1);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{

    fs_open(device);
    strncpy(fs_info->fs, hfsplus_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = be32toh(sb.blockSize);
    fs_info->totalblock  = be32toh(sb.totalBlocks);
    fs_info->usedblocks  = be32toh(sb.totalBlocks) - be32toh(sb.freeBlocks);
    fs_info->device_size = fs_info->block_size * fs_info->totalblock;
    log_mesg(2, 0, 0, 2, "%s: blockSize:%i\n", __FILE__, be32toh(sb.blockSize));
    log_mesg(2, 0, 0, 2, "%s: totalBlocks:%i\n", __FILE__, be32toh(sb.totalBlocks));
    log_mesg(2, 0, 0, 2, "%s: freeBlocks:%i\n", __FILE__, be32toh(sb.freeBlocks));
    print_fork_data(&sb.allocationFile);
    print_fork_data(&sb.extentsFile);
    print_fork_data(&sb.catalogFile);
    print_fork_data(&sb.attributesFile);
    print_fork_data(&sb.startupFile);
    fs_close();
}
