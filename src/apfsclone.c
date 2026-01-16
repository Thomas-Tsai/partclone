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

#define APFS_MAGIC_CONSTANT 0x4253584EULL // "BSXN"

#define MAX_APFS_TOTAL_BLOCKS (1ULL << 50) // Max blocks (approx 1 Exabyte @ 4KB/block)
#define MIN_APFS_BLOCK_SIZE 4096 // APFS block size is typically 4KB
#define MAX_APFS_BLOCK_SIZE 4096 // APFS block size is fixed at 4KB, or larger for specific cases
#define MAX_APFS_XP_DESC_BLOCKS (1ULL << 16) // Max 65536 blocks for descriptor area
#define MAX_APFS_SPACEMAN_SIZE (1ULL << 20) // Max 1MB for spaceman struct
#define MAX_APFS_CHUNK_INFO_COUNT (1ULL << 24) // Max 16 million chunk info blocks/arrays
#define MAX_APFS_CHUNK_PER_CIB (1ULL << 16) // Max 65536 chunks per chunk info block


static void *get_spaceman_buf(int fd, const struct nx_superblock_t *nxsb)
{
    uint32_t block_size = nxsb->nx_block_size;
    uint64_t base = nxsb->nx_xp_desc_base;
    uint32_t blocks = nxsb->nx_xp_desc_blocks;
    int found_nxsb = 0;
    checkpoint_map_phys_t *cpm = NULL;
    void *ret = NULL;
    obj_phys_t *obj = NULL;
    const uint32_t type_checkpoint_map =
	(OBJ_PHYSICAL | OBJECT_TYPE_CHECKPOINT_MAP);
    const uint32_t type_spaceman = (OBJ_EPHEMERAL | OBJECT_TYPE_SPACEMAN);
    uint32_t i;
    ssize_t read_size;

    if (block_size == 0 || block_size != MIN_APFS_BLOCK_SIZE) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Invalid APFS block size in get_spaceman_buf: %u.\n", __FILE__, block_size);
        return NULL;
    }

    if (base >> 63 || blocks >> 31) { // Original warning
        log_mesg(0, 1, 1, fs_opt.debug, "%s: B-tree checkpoint descriptor area not supported (base or blocks too large).\n", __FILE__);
        return NULL;
    }
    if (blocks == 0 || blocks > MAX_APFS_XP_DESC_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero xp_desc_blocks: %u (Max allowed: %llu).\n", __FILE__, blocks, MAX_APFS_XP_DESC_BLOCKS);
        return NULL;
    }

    obj = malloc(block_size);
    if (!obj) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for obj buffer: %s\n", __FILE__, strerror(errno));
        return NULL;
    }
    for (; blocks > 0; base++, blocks--) {
        if (base > ULLONG_MAX / block_size) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Potential overflow in pread offset calculation (base: %llu, block_size: %u).\n", __FILE__, base, block_size);
            free(obj);
            return NULL;
        }
        read_size = pread(fd, obj, block_size, base * block_size);
	if (read_size != block_size) {
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: pread error reading obj (%zd bytes, expected %u): %s\n", __FILE__, read_size, block_size, strerror(errno));
            free(obj);
            return NULL;
        }
	if (obj->o_type == nxsb->nx_o.o_type) {
	    /* Sanity check: *nxsb has a copy of latest nx_superblock
	     * if the filesystem is unmounted cleanly. */
	    if (obj->o_xid < nxsb->nx_o.o_xid)
		continue;
	    if (obj->o_xid > nxsb->nx_o.o_xid)
		log_mesg(0, 1, 1, fs_opt.debug, "%s: newer nx_superblock found in descriptor\n", __FILE__);
	    found_nxsb = 1;
	}
	if (obj->o_type == type_checkpoint_map) {
	    /* Find latest checkpoint_map_phys_t */
	    if (!cpm || cpm->cpm_o.o_xid < obj->o_xid) {
		if (!cpm) {
		    cpm = malloc(block_size);
                    if (!cpm) {
                        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for cpm buffer: %s\n", __FILE__, strerror(errno));
                        free(obj);
                        return NULL;
                    }
                }
		memcpy(cpm, obj, block_size);
	    }
	}
    }
    free(obj);
    obj = NULL;

    if (!found_nxsb) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: nx_superblock not found in descriptor.\n", __FILE__);
        free(cpm);
        return NULL;
    }
    if (!cpm) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: checkpoint_map_phys not found in descriptor.\n", __FILE__);
        return NULL;
    }
    if (!(cpm->cpm_flags & CHECKPOINT_MAP_LAST)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: multiple checkpoint_map_phys not supported.\n", __FILE__);
        free(cpm);
        return NULL;
    }
    if (cpm->cpm_count == 0 || cpm->cpm_count > block_size / sizeof(cpm->cpm_map[0])) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero cpm_count: %u.\n", __FILE__, cpm->cpm_count);
        free(cpm);
        return NULL;
    }

    for (i = 0; i < cpm->cpm_count; i++) {
	if (cpm->cpm_map[i].cpm_oid == nxsb->nx_spaceman_oid &&
	    cpm->cpm_map[i].cpm_type == type_spaceman) {
            if (cpm->cpm_map[i].cpm_size == 0 || cpm->cpm_map[i].cpm_size > MAX_APFS_SPACEMAN_SIZE) {
                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero spaceman size: %u (Max allowed: %llu).\n", __FILE__, cpm->cpm_map[i].cpm_size, MAX_APFS_SPACEMAN_SIZE);
                free(cpm);
                return NULL;
            }
	    ret = malloc(cpm->cpm_map[i].cpm_size);
	    if (!ret) {
		log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for spaceman_phys_t: %s\n", __FILE__, strerror(errno));
                free(cpm);
                return NULL;
            }
            if (cpm->cpm_map[i].cpm_paddr > ULLONG_MAX / block_size) {
                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Potential overflow in pread offset for spaceman (paddr: %llu, block_size: %u).\n", __FILE__, cpm->cpm_map[i].cpm_paddr, block_size);
                free(ret); free(cpm);
                return NULL;
            }
            read_size = pread(fd, ret, cpm->cpm_map[i].cpm_size, cpm->cpm_map[i].cpm_paddr * block_size);
	    if (read_size != cpm->cpm_map[i].cpm_size) {
		log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: pread error reading spaceman (%zd bytes, expected %u): %s\n", __FILE__, read_size, cpm->cpm_map[i].cpm_size, strerror(errno));
                free(ret); free(cpm);
                return NULL;
            }
	    break;
	}
    }
    free(cpm);
    if (!ret) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: spaceman not found.\n", __FILE__);
        return NULL;
    }
    return ret;
}
int APFSDEV;
struct nx_superblock_t nxsb;

/// get_apfs_free_count
static uint64_t get_apfs_free_count(){

    struct spaceman_phys_t spaceman;
    void *spaceman_buf;
    uint64_t free_count = 0;
    uint64_t total_free_count = 0;

    spaceman_buf = get_spaceman_buf(APFSDEV, &nxsb);
    if (!spaceman_buf) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Failed to get spaceman buffer.\n", __FILE__);
        return 0;
    }
    memcpy(&spaceman, spaceman_buf, sizeof(spaceman));

    if (spaceman.sm_block_size == 0 || spaceman.sm_block_size != nxsb.nx_block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Spaceman block size %u inconsistent with superblock %u.\n", __FILE__, spaceman.sm_block_size, nxsb.nx_block_size);
        free(spaceman_buf);
        return 0;
    }

    log_mesg(2, 0, 0, fs_opt.debug, "%s: sm_block_size %x\n", __FILE__, spaceman.sm_block_size);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: blocks_per_chunk %x\n", __FILE__, spaceman.sm_blocks_per_chunk);
    log_mesg(2, 0 ,0, fs_opt.debug, "%s: sd count=%i\n", __FILE__, SD_COUNT); // SD_COUNT is a define

    // Validate SD_COUNT if it comes from untrusted source or is used as loop limit without bounds check
    // Assuming SD_COUNT is a safe constant or derived from struct definition.
    if (SD_COUNT == 0 || SD_COUNT > 1024) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero SD_COUNT detected: %u.\n", __FILE__, SD_COUNT);
        free(spaceman_buf);
        return 0;
    }

    for (int sd = 0; sd < SD_COUNT; sd++){
        if (spaceman.sm_dev[sd].sm_block_count > MAX_APFS_TOTAL_BLOCKS) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large sm_dev[%d].sm_block_count: %llu.\n", __FILE__, sd, spaceman.sm_dev[sd].sm_block_count);
            free(spaceman_buf);
            return 0;
        }
        if (spaceman.sm_dev[sd].sm_free_count > spaceman.sm_dev[sd].sm_block_count) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: sm_dev[%d].sm_free_count %llu exceeds sm_block_count %llu.\n", __FILE__, sd, spaceman.sm_dev[sd].sm_free_count, spaceman.sm_dev[sd].sm_block_count);
            free(spaceman_buf);
            return 0;
        }

        free_count = spaceman.sm_dev[sd].sm_free_count;

	log_mesg(1, 0, 0, fs_opt.debug, "%s: block_count %llX in sd %i\n", __FILE__, spaceman.sm_dev[sd].sm_block_count, sd);
	log_mesg(1, 0, 0, fs_opt.debug, "%s: free_count  %llX in sd %i\n", __FILE__, free_count, sd);

        total_free_count += free_count;
        if (total_free_count < free_count) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Overflow in total_free_count calculation.\n", __FILE__);
            free(spaceman_buf);
            return 0;
        }
    }
    free(spaceman_buf);
    return total_free_count;
}

/// open device
static int fs_open(char* device){

    char *buf = NULL;
    size_t buffer_size = 4096;
    ssize_t read_size = 0;

    APFSDEV = open(device, O_RDONLY);
    if (APFSDEV == -1){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Cannot open the partition (%s): %s\n", __FILE__, device, strerror(errno));
        return -1;
    }
    buf = (char *)malloc(buffer_size);
    if (!buf) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for superblock buffer: %s\n", __FILE__, strerror(errno));
        close(APFSDEV);
        return -1;
    }
    memset(buf, 0, buffer_size);
    read_size = read(APFSDEV, buf, buffer_size);
    if (read_size != buffer_size){
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: read error for superblock (read %zd bytes, expected %zu): %s\n", __FILE__, read_size, buffer_size, strerror(errno));
        free(buf);
        close(APFSDEV);
        return -1;
    }
    memcpy(&nxsb, buf, sizeof(nxsb));

    // Validate APFS superblock (nxsb)
    if (nxsb.nx_magic != APFS_MAGIC_CONSTANT) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: APFS MAGIC 0x%llX mismatch (expected 0x%llX).\n", __FILE__, nxsb.nx_magic, APFS_MAGIC_CONSTANT);
        free(buf);
        close(APFSDEV);
        return -1;
    }
    if (nxsb.nx_block_size == 0 || nxsb.nx_block_size != MIN_APFS_BLOCK_SIZE) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Invalid or unsupported APFS block size: %u (expected %u).\n", __FILE__, nxsb.nx_block_size, MIN_APFS_BLOCK_SIZE);
        free(buf);
        close(APFSDEV);
        return -1;
    }
    if (nxsb.nx_block_count == 0 || nxsb.nx_block_count > MAX_APFS_TOTAL_BLOCKS) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero APFS block count: %llu (Max allowed: %llu).\n", __FILE__, nxsb.nx_block_count, MAX_APFS_TOTAL_BLOCKS);
        free(buf);
        close(APFSDEV);
        return -1;
    }


    log_mesg(1, 0, 0, fs_opt.debug, "%s: APFS MAGIC 0x%llX\n", __FILE__, nxsb.nx_magic); // Use %llX for uint64_t

    if(lseek(APFSDEV, 0, SEEK_SET) != 0) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: device %s seek fail: %s\n", __FILE__, device, strerror(errno));
        free(buf);
        close(APFSDEV);
        return -1;
    }

    free(buf);
    return 0; // Success
}
/// close device
static void fs_close(){
    close(APFSDEV);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    int start = 0;
    int bit_size = 1;

    uint32_t block_size;

    struct spaceman_phys_t spaceman;
    void *spaceman_buf;
    uint64_t sm_offset = 0;

    char *chunk_info_block_buf = NULL;
    struct obj_phys_t obj_phys_t;
    struct chunk_info_block_t chunk_info_block;
    struct chunk_info_t chunk_info;
    ssize_t  read_size = 0;
    uint64_t  addr;
    uint64_t  addr_data;
    uint64_t cnt = 0;
    uint64_t cnt_count = 0;
    uint64_t chunk = 0;
    uint64_t blocks_per_chunk = 0;

    int sd = 0;

    uint64_t block = 0;
    uint64_t bitmap_block = 0;
    char *bitmap_entry_buf = NULL;

    if (fs_open(device) != 0) {
        return; // Abort
    }

    pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);

    /// init progress
    progress_bar   prog;    /// progress_bar structure defined in progress.h
    progress_init(&prog, start, fs_info.totalblock, fs_info.totalblock, BITMAP, bit_size);

    spaceman_buf = get_spaceman_buf(APFSDEV, &nxsb);
    if (!spaceman_buf) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Failed to get spaceman buffer.\n", __FILE__);
        fs_close();
        return;
    }
    memcpy(&spaceman, spaceman_buf, sizeof(spaceman));

    block_size = nxsb.nx_block_size;
    blocks_per_chunk = spaceman.sm_blocks_per_chunk;
    if (blocks_per_chunk == 0 || blocks_per_chunk > fs_info.totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero blocks_per_chunk: %llu.\n", __FILE__, blocks_per_chunk);
        fs_close();
        return;
    }


    chunk_info_block_buf = (char *)malloc(block_size);
    if (!chunk_info_block_buf) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for chunk_info_block_buf: %s\n", __FILE__, strerror(errno));
        fs_close();
        return;
    }
    bitmap_entry_buf = (char *)malloc(block_size);
    if (!bitmap_entry_buf) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: malloc error for bitmap_entry_buf: %s\n", __FILE__, strerror(errno));
        free(chunk_info_block_buf);
        fs_close();
        return;
    }

    if (SD_COUNT == 0 || SD_COUNT > 1024) {
        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero SD_COUNT detected: %u.\n", __FILE__, SD_COUNT);
        free(chunk_info_block_buf); free(bitmap_entry_buf);
        fs_close();
        return;
    }

    for (sd = 0; sd < SD_COUNT; sd++){
        sm_offset = spaceman.sm_dev[sd].sm_addr_offset;
        uint32_t current_sd_cab_count = spaceman.sm_dev[sd].sm_cab_count;
        uint32_t current_sd_cib_count = spaceman.sm_dev[sd].sm_cib_count;

        if (current_sd_cab_count > 0){
            cnt_count = current_sd_cab_count;
        } else {
            cnt_count = current_sd_cib_count;
        }
        if (cnt_count > MAX_APFS_CHUNK_INFO_COUNT) {
            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero cnt_count for sd %d: %llu (Max allowed: %llu).\n", __FILE__, sd, cnt_count, MAX_APFS_CHUNK_INFO_COUNT);
            // This might be non-fatal if other sd's are fine, but for now, abort
            free(chunk_info_block_buf); free(bitmap_entry_buf);
            fs_close();
            return;
        }

        log_mesg(2, 0, 0, fs_opt.debug, "%s: sd %u, sm_offset %llx, cnt_count %llu\n", __FILE__, sd, sm_offset, cnt_count);

        if ((sm_offset != 0) && (cnt_count != 0)) { // Only proceed if there's actual data to process
	    log_mesg(2, 0, 0, fs_opt.debug, "%s: check each chunk\n", __FILE__);

            for (cnt = 0; cnt < cnt_count; cnt++){
		log_mesg(2, 0, 0, fs_opt.debug, "%s: check chunk %llu\n", __FILE__, cnt);

                addr = sm_offset + (uint64_t)sizeof(addr)*cnt; 

                memcpy(&addr_data, (char *)spaceman_buf+addr, sizeof(addr));

                log_mesg(2, 0, 0, fs_opt.debug, "%s: bitmap addr  %llx\n", __FILE__, addr_data); // use %llx for uint64_t

                // Validate addr_data before pread for chunk_info_block
                if (addr_data == 0 || addr_data > ULLONG_MAX / block_size) { // Check for zero and overflow
                    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Malicious physical address for chunk info block: %llu.\n", __FILE__, addr_data);
                    free(chunk_info_block_buf); free(bitmap_entry_buf);
                    fs_close();
                    return;
                }

                memset(chunk_info_block_buf, 0,  block_size);
                memset(&chunk_info_block, 0, sizeof(chunk_info_block));
                read_size = pread(APFSDEV, chunk_info_block_buf, block_size, addr_data*block_size);
                if (read_size != block_size) {
                    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: pread error reading chunk info block from %llu (%zd bytes, expected %u): %s\n", __FILE__, addr_data*block_size, read_size, block_size, strerror(errno));
                    free(chunk_info_block_buf); free(bitmap_entry_buf);
                    fs_close();
                    return;
                }
                memcpy(&chunk_info_block, chunk_info_block_buf, sizeof(chunk_info_block));
                memcpy(&obj_phys_t, chunk_info_block_buf, sizeof(obj_phys_t));

                log_mesg(2, 0, 0, fs_opt.debug, "%s: chunk_info_count %x\n", __FILE__, chunk_info_block.cib_chunk_info_count);
                log_mesg(2, 0, 0, fs_opt.debug, "%s: block type %x\n", __FILE__, obj_phys_t.o_type);

                if (chunk_info_block.cib_chunk_info_count == 0 || chunk_info_block.cib_chunk_info_count > MAX_APFS_CHUNK_PER_CIB) {
                    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero cib_chunk_info_count: %u (Max allowed: %llu).\n", __FILE__, chunk_info_block.cib_chunk_info_count, MAX_APFS_CHUNK_PER_CIB);
                    free(chunk_info_block_buf); free(bitmap_entry_buf);
                    fs_close();
                    return;
                }

                if(obj_phys_t.o_type == 0x40000007ULL){ // Use ULL for constants, check against actual APFS object type defines

                    log_mesg(3, 0, 0, fs_opt.debug, "%s: get addr %llx\n", __FILE__, addr_data); // use %llx for uint64_t
                    for (chunk = 0; chunk < chunk_info_block.cib_chunk_info_count; chunk++) {
                        if (sizeof(chunk_info_block) + chunk*(uint64_t)sizeof(chunk_info) > block_size - sizeof(chunk_info)) {
                            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Overflow in chunk_info memcpy offset or OOB access for chunk %llu.\n", __FILE__, chunk);
                            free(chunk_info_block_buf); free(bitmap_entry_buf);
                            fs_close();
                            return;
                        }
                        memset(&chunk_info, 0, sizeof(chunk_info));
                        memcpy(&chunk_info, chunk_info_block_buf+(uint64_t)sizeof(chunk_info_block)+chunk*(uint64_t)sizeof(chunk_info), sizeof(chunk_info));
                        log_mesg(2, 0, 0, fs_opt.debug, "%s: xid = %x, offset = %llx, bitTot = %llx, bit avl = %llx, block = %llx\n", __FILE__, chunk_info.ci_xid, chunk_info.ci_addr, chunk_info.ci_block_count, chunk_info.ci_free_count, chunk_info.ci_bitmap_addr ); // use %llx for uint64_t


                                                if ((chunk_info.ci_bitmap_addr == 0) && (chunk_info.ci_free_count == blocks_per_chunk))  { // 0x8000, all free
                                                    if (chunk_info.ci_block_count == 0 || (unsigned long long)chunk_info.ci_block_count > fs_info.totalblock) {
                                                        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero ci_block_count: %llu.\n", __FILE__, chunk_info.ci_block_count);
                                                        free(chunk_info_block_buf); free(bitmap_entry_buf);
                                                        fs_close();
                                                        return;
                                                    }
                                                    for (block = 0 ; block < chunk_info.ci_block_count; block++){
                                                        if (chunk_info.ci_addr > ULLONG_MAX - block) {
                                                            log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Overflow in block+ci_addr calculation.\n", __FILE__);
                                                            free(chunk_info_block_buf); free(bitmap_entry_buf);
                                                            fs_close();
                                                            return;
                                                        }
                                                        pc_clear_bit(block+chunk_info.ci_addr, bitmap, fs_info.totalblock);
                                                    }
                                                } else {
                                                    if (chunk_info.ci_bitmap_addr != 0 && (chunk_info.ci_bitmap_addr > ULLONG_MAX / block_size)) {
                                                        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Malicious physical address for bitmap entry: %llu.\n", __FILE__, chunk_info.ci_bitmap_addr);
                                                        free(chunk_info_block_buf); free(bitmap_entry_buf);
                                                        fs_close();
                                                        return;
                                                    }                            if (chunk_info.ci_block_count == 0 || (unsigned long long)chunk_info.ci_block_count > fs_info.totalblock) {
                                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero ci_block_count (for pc_test_bit loop): %llu.\n", __FILE__, chunk_info.ci_block_count);
                                free(chunk_info_block_buf); free(bitmap_entry_buf);
                                fs_close();
                                return;
                            }


                            memset(bitmap_entry_buf, 0, block_size);
                            read_size = pread(APFSDEV, bitmap_entry_buf, block_size, chunk_info.ci_bitmap_addr*block_size);
                            if (read_size != block_size) {
                                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: pread error reading bitmap entry from %llu (%zd bytes, expected %u): %s\n", __FILE__, chunk_info.ci_bitmap_addr*block_size, read_size, block_size, strerror(errno));
                                free(chunk_info_block_buf); free(bitmap_entry_buf);
                                fs_close();
                                return;
                            }

                            for (block = 0 ; block < chunk_info.ci_block_count; block++){
                                if (block / 8 >= block_size) {
                                    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Block index %llu out of bounds for bitmap_entry_buf (size %u).\n", __FILE__, block, block_size);
                                    free(chunk_info_block_buf); free(bitmap_entry_buf);
                                    fs_close();
                                    return;
                                }

                                if (chunk_info.ci_addr > ULLONG_MAX - block) {
                                    log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Overflow in block+ci_addr calculation.\n", __FILE__);
                                    free(chunk_info_block_buf); free(bitmap_entry_buf);
                                    fs_close();
                                    return;
                                }

                                if (pc_test_bit(block, (void *)bitmap_entry_buf, chunk_info.ci_block_count) == 0){
                                    pc_clear_bit(block+chunk_info.ci_addr, bitmap, fs_info.totalblock);
                                }
                            }
                        }
    		    update_pui(&prog, chunk, chunk_info_block.cib_chunk_info_count, 0);
                    } // end of bitmap
                } else { // end of type check (other object type)
                    if (spaceman.sm_blocks_per_chunk == 0 || spaceman.sm_blocks_per_chunk > MAX_APFS_TOTAL_BLOCKS) {
                        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero spaceman.sm_blocks_per_chunk: %llu.\n", __FILE__, spaceman.sm_blocks_per_chunk);
                        free(chunk_info_block_buf); free(bitmap_entry_buf);
                        fs_close();
                        return;
                    }
                    if (spaceman.sm_chunks_per_cib == 0 || spaceman.sm_chunks_per_cib > MAX_APFS_CHUNK_PER_CIB) {
                        log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Maliciously large or zero spaceman.sm_chunks_per_cib: %llu.\n", __FILE__, spaceman.sm_chunks_per_cib);
                        free(chunk_info_block_buf); free(bitmap_entry_buf);
                        fs_close();
                        return;
                    }

                    for (chunk = 0; chunk < spaceman.sm_blocks_per_chunk; chunk++) {
                        for (block = 0 ; block < spaceman.sm_chunks_per_cib; block++){
                            if (spaceman.sm_blocks_per_chunk > ULLONG_MAX / spaceman.sm_chunks_per_cib ||
                                spaceman.sm_blocks_per_chunk * spaceman.sm_chunks_per_cib > ULLONG_MAX / cnt ||
                                block > ULLONG_MAX - (spaceman.sm_blocks_per_chunk * spaceman.sm_chunks_per_cib * cnt) ) {
                                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: Overflow in bitmap_block calculation.\n", __FILE__);
                                free(chunk_info_block_buf); free(bitmap_entry_buf);
                                fs_close();
                                return;
                            }
                            bitmap_block = block + spaceman.sm_blocks_per_chunk*spaceman.sm_chunks_per_cib*cnt;
                            if (bitmap_block > fs_info.totalblock){
                                log_mesg(0, 1, 1, fs_opt.debug, "%s: ERROR: bitmap_block %llu exceeds fs_info.totalblock %llu.\n", __FILE__, bitmap_block, fs_info.totalblock);
                                free(chunk_info_block_buf); free(bitmap_entry_buf);
                                fs_close();
                                return;
                            }
                            pc_clear_bit(bitmap_block, bitmap, fs_info.totalblock);
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

    if (fs_open(device) != 0) {
        return; // Abort
    }
    // get_apfs_free_count already has error handling. If it returns 0 on error, it's fine. 
    free_blocks = get_apfs_free_count(); 

    strncpy(fs_info->fs, apfs_MAGIC, FS_MAGIC_SIZE);

    fs_info->block_size  = nxsb.nx_block_size;
    fs_info->totalblock  = nxsb.nx_block_count;

    if (free_blocks > fs_info->totalblock) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Free blocks (%llu) greater than total blocks (%llu).\n", free_blocks, fs_info->totalblock);
        fs_close();
        return;
    }

    fs_info->usedblocks  = fs_info->totalblock-free_blocks;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;

    if (fs_info->block_size == 0 || fs_info->totalblock > ULLONG_MAX / fs_info->block_size) {
        log_mesg(0, 1, 1, fs_opt.debug, "ERROR: Potential overflow or division by zero in device size calculation. totalblock: %llu, block_size: %u\n",
                 fs_info->totalblock, fs_info->block_size);
        fs_close();
        return;
    }
    fs_info->device_size = fs_info->totalblock*fs_info->block_size;
    fs_close();
}