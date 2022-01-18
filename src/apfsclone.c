/**
 * apfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read apfs super block and bitmap
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
#include "apfsclone.h"
#include "progress.h"
#include "fs_common.h"

int APFSDEV;
struct nx_superblock_t nxsb;

/// get_apfs_free_count
static uint64_t get_apfs_free_count(){

    int block_size = 4096;
    size_t size = 0;

    struct spaceman_phys_t spaceman;
    char *spaceman_buf;

    int sd = 0;
    int cnt = 0;
    int total_cnt = 0;
    uint64_t read_size = 0;
    uint64_t free_count = 0;
    uint64_t total_free_count = 0;

    spaceman_buf = (char *)malloc(block_size*nxsb.nx_xp_data_len);
    memset(spaceman_buf, 0, block_size*nxsb.nx_xp_data_len); // not sure what is real size
    read_size = pread(APFSDEV, spaceman_buf,  block_size*nxsb.nx_xp_data_len, nxsb.nx_xp_data_base*block_size);
    memcpy(&spaceman, spaceman_buf, sizeof(spaceman));
    log_mesg(2, 0, 0, fs_opt.debug, "%s: sm_block_size %x\n", __FILE__, spaceman.sm_block_size);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: blocks_per_chunk %x\n", __FILE__, spaceman.sm_blocks_per_chunk);
    log_mesg(2, 0 ,0, fs_opt.debug, "%s: sd count=%i\n", __FILE__, sd);

    for (sd = 0; sd < SD_COUNT; sd++){
    
        free_count = spaceman.sm_dev[sd].sm_free_count;

	log_mesg(1, 0, 0, fs_opt.debug, "%s: block_count %x in sd %x\n", __FILE__, spaceman.sm_dev[sd].sm_block_count, sd);
	log_mesg(1, 0, 0, fs_opt.debug, "%s: free_count  %x in sd %x\n", __FILE__, free_count, sd);

        total_free_count += free_count;
    }
 
    return total_free_count;
}

/// open device
static void fs_open(char* device){

    char *buf;
    size_t buffer_size = 4096;
    size_t read_size = 0;

    APFSDEV = open(device, O_RDONLY);
    if (APFSDEV == -1){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: Cannot open the partition (%s).\n", __FILE__, device);
    }
    buf = (char *)malloc(buffer_size);
    memset(buf, 0, buffer_size);
    read_size = read(APFSDEV, buf, buffer_size);
    if (read_size != buffer_size){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: read error\n", __FILE__);
    }
    memcpy(&nxsb, buf, sizeof(nxsb));
    // todo : add apfs check!

    log_mesg(1, 0, 0, fs_opt.debug, "%s:APFS MAGIC 0x%jX\n", __FILE__, nxsb.nx_magic);

    if(lseek(APFSDEV, 0, SEEK_SET) != 0) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: device %s seek fail\n", __FILE__, device);
    }

    free(buf);

}

/// close device
static void fs_close(){
    close(APFSDEV);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    int start = 0;
    int bit_size = 1;

    uint32_t block_size = 4096;
    size_t size = 0;

    struct spaceman_phys_t spaceman;
    char *spaceman_buf;
    uint64_t sm_offset = 0;
    
    char *chunk_info_block_buf;
    struct obj_phys_t obj_phys_t;
    struct chunk_info_block_t chunk_info_block;
    struct chunk_info_t chunk_info;
    uint64_t  read_size = 0;
    uint64_t  addr;
    uint64_t  addr_data;
    uint64_t cnt = 0;
    uint64_t cnt_count = 0;
    uint64_t chunk = 0;
    uint64_t blocks_per_chunk = 0;

    int sd = 0;
    int total_cnt = 0;
    int ci_bitmap = 0;
    uint64_t free_count = 0;
    uint64_t total_free_count = 0;

    uint64_t block = 0;
    uint64_t bitmap_block = 0;
    char *bitmap_entry_buf;

    fs_open(device);

    /// init progress
    progress_bar   prog;	/// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);


    for (block = 0; block < fs_info.totalblock; block++){
        pc_set_bit(block, bitmap, fs_info.totalblock);
    }

    spaceman_buf = (char *)malloc(block_size);
    memset(spaceman_buf, 0, block_size);
    read_size = pread(APFSDEV, spaceman_buf, block_size, nxsb.nx_xp_data_base*block_size);
    memcpy(&spaceman, spaceman_buf, sizeof(spaceman));

    block_size = nxsb.nx_block_size;
    blocks_per_chunk = spaceman.sm_blocks_per_chunk;

    chunk_info_block_buf = (char *)malloc(block_size);
    bitmap_entry_buf = (char *)malloc(block_size);

    for (sd = 0; sd < SD_COUNT; sd++){
    
        sm_offset = spaceman.sm_dev[sd].sm_addr_offset;

        if (spaceman.sm_dev[sd].sm_cab_count > 0){
            cnt_count = spaceman.sm_dev[sd].sm_cab_count;
        } else {
            cnt_count = spaceman.sm_dev[sd].sm_cib_count;
        }
        log_mesg(2, 0, 0, fs_opt.debug, "%s: sd %u, sm_offset %x, cnt_count %x\n", __FILE__, sd, sm_offset, cnt_count);

        if ((sm_offset != 0) && (cnt_count != 0)) {
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: check each chunk\n", __FILE__);

            for (cnt = 0; cnt < cnt_count; cnt++){
		log_mesg(2, 0, 0, fs_opt.debug, "%s: check chunk %u\n", __FILE__, cnt);

                addr = sm_offset + sizeof(addr)*cnt;

                memset(&addr_data, 0, sizeof(addr));
                memcpy(&addr_data, spaceman_buf+addr, sizeof(addr));
                log_mesg(2, 0, 0, fs_opt.debug, "%s: bitmap addr  %lx\n", __FILE__, addr_data);

                memset(chunk_info_block_buf, 0,  block_size);
                memset(&chunk_info_block, 0, sizeof(chunk_info_block));
                read_size = pread(APFSDEV, chunk_info_block_buf, block_size, addr_data*block_size);
                memcpy(&chunk_info_block, chunk_info_block_buf, sizeof(chunk_info_block));
                memcpy(&obj_phys_t, chunk_info_block_buf, sizeof(obj_phys_t));

                log_mesg(2, 0, 0, fs_opt.debug, "%s: chunk_info_count %x\n", __FILE__, chunk_info_block.cib_chunk_info_count);
                log_mesg(2, 0, 0, fs_opt.debug, "%s: block type %x\n", __FILE__, obj_phys_t.o_type);

                if(obj_phys_t.o_type == 0x40000007){
                
                    log_mesg(3, 0, 0, fs_opt.debug, "%s: get addr %x\n", __FILE__, addr_data);
                    for (chunk = 0; chunk < chunk_info_block.cib_chunk_info_count; chunk++) {
                        memset(&chunk_info, 0, sizeof(chunk_info));
                        memcpy(&chunk_info, chunk_info_block_buf+sizeof(chunk_info_block)+chunk*sizeof(chunk_info), sizeof(chunk_info));
                        log_mesg(2, 0, 0, fs_opt.debug, "%s: xid = %x, offset = %x, bitTot = %x, bit avl = %x, block = %x\n", __FILE__, chunk_info.ci_xid, chunk_info.ci_addr, chunk_info.ci_block_count, chunk_info.ci_free_count, chunk_info.ci_bitmap_addr );
    
                        
                        if ((chunk_info.ci_bitmap_addr == 0) && (chunk_info.ci_free_count == blocks_per_chunk))  { // 0x8000, all free
                            for (block = 0 ; block < chunk_info.ci_block_count; block++){
                                pc_clear_bit(block+chunk_info.ci_addr, bitmap, fs_info.totalblock);
				//log_mesg(3, 0, 0, fs_opt.debug, "%s: free block  %lx\n", __FILE__, block+chunk_info.ci_addr);
                            }
                        } else {
                        
                            memset(bitmap_entry_buf, 0, block_size);
                            read_size = pread(APFSDEV, bitmap_entry_buf, block_size, chunk_info.ci_bitmap_addr*block_size);
    
                            for (block = 0 ; block < chunk_info.ci_block_count; block++){
                                if (pc_test_bit(block, bitmap_entry_buf, chunk_info.ci_block_count) == 0){
                                    pc_clear_bit(block+chunk_info.ci_addr, bitmap, fs_info.totalblock);
				    //log_mesg(3, 0, 0, fs_opt.debug, "%s: free block  %lx\n", __FILE__, block+chunk_info.ci_addr);
                                }
                            }
                        }
    		    update_pui(&prog, block, block, 0);
                    } // end of bitmap
                } else { // end of type check
                    for (chunk = 0; chunk < spaceman.sm_blocks_per_chunk; chunk++) {
                        for (block = 0 ; block < spaceman.sm_chunks_per_cib; block++){
                            bitmap_block = block+spaceman.sm_blocks_per_chunk*spaceman.sm_chunks_per_cib*cnt;
                            if (bitmap_block <= fs_info.totalblock){
                                pc_clear_bit(bitmap_block, bitmap, fs_info.totalblock);
				//log_mesg(3, 0, 0, fs_opt.debug, "%s: free block  %lx\n", __FILE__, bitmap_block);
                            }
                        }
                    }
                } // end of type check
            } // end of cnt
        }
    }
    free(bitmap_entry_buf);
    free(chunk_info_block_buf);
    free(spaceman_buf);

    fs_close();
    ///// update progress
    update_pui(&prog, 1, 1, 1);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{
    unsigned long long free_blocks = 0;

    fs_open(device);
    free_blocks = get_apfs_free_count();

    strncpy(fs_info->fs, apfs_MAGIC, FS_MAGIC_SIZE);
    fs_info->block_size  = nxsb.nx_block_size;
    fs_info->totalblock  = nxsb.nx_block_count;
    fs_info->usedblocks  = fs_info->totalblock-free_blocks;
    fs_info->device_size = fs_info->totalblock*fs_info->block_size;
    fs_close();
}

