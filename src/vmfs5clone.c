/**
 * vmfsclone.c - part of Partclone project
 * part of source from vmfs-tools/fsck.c
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read vmfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <unistd.h>
#include <inttypes.h>
#include <vmfs/vmfs.h>
#include <pthread.h>


#include "partclone.h"
#include "reiser4clone.h"
#include "progress.h"
#include "fs_common.h"

vmfs_fs_t *fs;
vmfs_dir_t *root_dir;
unsigned long *blk_bitmap;
progress_bar   prog;        /// progress_bar structure defined in progress.h
unsigned long long checked;
void *thread_update_bitmap_pui(void *arg);
int bitmap_done = 0;
unsigned long long total_block = 0;

/* Forward declarations */
typedef struct vmfs_dir_map vmfs_dir_map_t;
typedef struct vmfs_blk_map vmfs_blk_map_t;

/* Directory mapping */
struct vmfs_dir_map {
    char *name;
    uint32_t blk_id;
    int is_dir;
    vmfs_blk_map_t *blk_map;
};

/* 
 * Block mapping, which allows to keep track of inodes given a block number.
 * Used for troubleshooting/debugging/future dump.
 */
#define VMFS_BLK_MAP_BUCKETS     512
#define VMFS_BLK_MAP_MAX_INODES  32

struct vmfs_blk_map {
    uint32_t blk_id;
    union {
	uint32_t inode_id[VMFS_BLK_MAP_MAX_INODES];
	vmfs_inode_t inode;
    };
    vmfs_dir_map_t *dir_map;
    u_int ref_count;
    u_int nlink;
    int status;
    vmfs_blk_map_t *prev,*next;
};

typedef struct vmfs_dump_info vmfs_dump_info_t;
struct vmfs_dump_info {
    vmfs_blk_map_t *blk_map[VMFS_BLK_MAP_BUCKETS];
    u_int blk_count[VMFS_BLK_TYPE_MAX];
    vmfs_dir_map_t *dir_map;
};

/* Allocate a directory map structure */
vmfs_dir_map_t *vmfs_dir_map_alloc(const char *name,uint32_t blk_id)
{
    vmfs_dir_map_t *map;

    if (!(map = calloc(1,sizeof(*map))))
	return NULL;

    if (!(map->name = strdup(name))) {
	free(map);
	return NULL;
    }

    map->blk_id = blk_id;

    return map;
}

/* Create the root directory mapping */
vmfs_dir_map_t *vmfs_dir_map_alloc_root(void)
{
    vmfs_dir_map_t *map;
    uint32_t root_blk_id;

    root_blk_id = VMFS_BLK_FD_BUILD(0,0,0);

    if (!(map = vmfs_dir_map_alloc("/",root_blk_id)))
	return NULL;

    return map;
}

/* Hash function for a block ID */
static inline u_int vmfs_block_map_hash(uint32_t blk_id)
{
    return(blk_id ^ (blk_id >> 5));
}

/* Find a block mapping */
vmfs_blk_map_t *vmfs_block_map_find(vmfs_blk_map_t **ht,uint32_t blk_id)
{   
    vmfs_blk_map_t *map;
    u_int bucket;

    bucket = vmfs_block_map_hash(blk_id) % VMFS_BLK_MAP_BUCKETS;

    for(map=ht[bucket];map;map=map->next)
	if (map->blk_id == blk_id)
	    return map;

    return NULL;
}

/* Get a block mapping */
vmfs_blk_map_t *vmfs_block_map_get(vmfs_blk_map_t **ht,uint32_t blk_id)
{
    vmfs_blk_map_t *map;
    u_int bucket;

    if ((map = vmfs_block_map_find(ht,blk_id)) != NULL)
	return map;

    if (!(map = calloc(1,sizeof(*map))))
	return NULL;

    bucket = vmfs_block_map_hash(blk_id) % VMFS_BLK_MAP_BUCKETS;

    map->blk_id = blk_id;

    map->prev = NULL;
    map->next = ht[bucket];
    ht[bucket] = map;

    return map;
}

/* print block id pos */
void print_pos_by_id (const vmfs_fs_t *fs, uint32_t blk_id)
{
    unsigned long long pos = 0;
    uint32_t blk_type = VMFS_BLK_TYPE(blk_id);
    unsigned long long current = 0 ;

    switch(blk_type) {
	/* File Block */
	case VMFS_BLK_TYPE_FB:
	    pos = (unsigned long long)VMFS_BLK_FB_ITEM(blk_id) * vmfs_fs_get_blocksize(fs);
	    /* reference vmfs-tools/libvmfs/vmfs_volume.c 
	       pos += vol->vmfs_base + 0x1000000;
	     */
	    pos += 1048576 + 16777216;
	    break;

	    /* Sub-Block */
	case VMFS_BLK_TYPE_SB:
	    pos = vmfs_bitmap_get_item_pos(fs->sbc, VMFS_BLK_SB_ENTRY(blk_id), VMFS_BLK_SB_ITEM(blk_id));
	    break;

	    /* Pointer Block */
	case VMFS_BLK_TYPE_PB:
	    pos = vmfs_bitmap_get_item_pos(fs->pbc,VMFS_BLK_PB_ENTRY(blk_id), VMFS_BLK_PB_ITEM(blk_id));
	    break;

	    /* File Descriptor / Inode */
	case VMFS_BLK_TYPE_FD:
	    pos = vmfs_bitmap_get_item_pos(fs->fdc,VMFS_BLK_FD_ENTRY(blk_id), VMFS_BLK_FD_ITEM(blk_id));
	    break;

	default:
	    log_mesg(0, 0, 0, fs_opt.debug, "Unsupported block type 0x%2.2x\n", blk_type);
	    //fprintf(stderr,"Unsupported block type 0x%2.2x\n",blk_type);
    }
    if (checked < total_block)
	checked++;
    current = pos/vmfs_fs_get_blocksize(fs);
    if ( current > total_block )
	log_mesg(3, 0, 0, fs_opt.debug, "total_block Error Blockid = 0x%8.8x, Type = 0x%2.2x, Pos: %llu, bitmapid: %llu, c: %llu\n", blk_id, blk_type, pos, current, checked);
    log_mesg(3, 0, 0, fs_opt.debug, "Blockid = 0x%8.8x, Type = 0x%2.2x, Pos: %llu, bitmapid: %llu, c: %llu\n", blk_id, blk_type, pos, current, checked);
    pc_set_bit(current, blk_bitmap, total_block);
}


/* Store block mapping of an inode */
static void  vmfs_dump_store_block(const vmfs_inode_t *inode,
	uint32_t pb_blk,
	uint32_t blk_id,
	void *opt_arg)
{
    vmfs_blk_map_t **ht = opt_arg;
    vmfs_blk_map_t *map;

    if (!(map = vmfs_block_map_get(ht,blk_id)))
	return;

    if (map->ref_count < VMFS_BLK_MAP_MAX_INODES)
	map->inode_id[map->ref_count] = inode->id;

    map->ref_count++;
    map->status = vmfs_block_get_status(inode->fs,blk_id);
    if (map->status <= 0){
	log_mesg(0, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x is used but not allocated.\n", __FILE__, blk_id);
    } else
	print_pos_by_id(inode->fs, blk_id);
}

/* Store inode info */
static int vmfs_dump_store_inode(const vmfs_fs_t *fs,vmfs_blk_map_t **ht,
	const vmfs_inode_t *inode)
{
    vmfs_blk_map_t *map;

    if (!(map = vmfs_block_map_get(ht,inode->id)))
	return(-1);

    memcpy(&map->inode,inode,sizeof(*inode));
    map->status = vmfs_block_get_status(fs,inode->id);
    if (map->status <= 0){
	log_mesg(0, 0, 0, fs_opt.debug, "%s: Block 0x%8.8x is used but not allocated.\n", __FILE__, inode->id);
    } else
	print_pos_by_id(fs, inode->id);

    return 0;
}


/* dump bitmap fb */
void dump_bitmaps_fb (vmfs_bitmap_t *b,uint32_t addr, void *opt)
{  
    vmfs_fs_t *fs = opt;
    uint32_t blk_id;

    blk_id = VMFS_BLK_FB_BUILD(addr, 0); 
    print_pos_by_id(fs, blk_id);
}

/* dump bitmap sb */
void dump_bitmaps_sb (vmfs_bitmap_t *b,uint32_t addr, void *opt)
{  
    vmfs_fs_t *fs = opt;
    uint32_t entry,item;
    uint32_t blk_id;

    entry = addr / b->bmh.items_per_bitmap_entry;
    item  = addr % b->bmh.items_per_bitmap_entry;

    blk_id = VMFS_BLK_SB_BUILD(entry, item, 0);
    print_pos_by_id(fs, blk_id);
}

/* dump bitmap pb */
void dump_bitmaps_pb (vmfs_bitmap_t *b,uint32_t addr, void *opt)
{  
    vmfs_fs_t *fs = opt;
    uint32_t entry,item;
    uint32_t blk_id;

    entry = addr / b->bmh.items_per_bitmap_entry;
    item  = addr % b->bmh.items_per_bitmap_entry;

    blk_id = VMFS_BLK_PB_BUILD(entry, item, 0);
    print_pos_by_id(fs, blk_id);
}


/* Initialize dump structures */
static void vmfs_dump_init(vmfs_dump_info_t *fi)
{
    memset(fi,0,sizeof(*fi));
    fi->dir_map = vmfs_dir_map_alloc_root();
}


/// open device
static void fs_open(char* device){
#ifndef VMFS5_ZLA_BASE
    vmfs_lvm_t *lvm;
#endif
    vmfs_flags_t flags;
    char *mdev[] = {device, NULL};

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: device %s\n", __FILE__, device);

#ifdef VMFS5_ZLA_BASE
    if (!(fs=vmfs_fs_open(mdev, flags))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open volume.\n", __FILE__);
    }
#else
    if (!(lvm = vmfs_lvm_create(flags))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to create LVM structure\n", __FILE__);
    }

    if (vmfs_lvm_add_extent(lvm, vmfs_vol_open(device, flags)) == -1) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open device/file \"%s\".\n", __FILE__, device);
    }

    if (!(fs = vmfs_fs_create(lvm))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open filesystem\n", __FILE__);
    }

    if (vmfs_fs_open(fs) == -1) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open volume.\n", __FILE__);
    }
#endif	

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0,0)))) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open root directory\n", __FILE__);
    }

}

/// close device
static void fs_close(){
    vmfs_dir_close(root_dir);
    vmfs_fs_close(fs);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{
    unsigned long long used_block = 0, free_block = 0, err_block = 0;
    int start = 0;
    int bit_size = 1;

    vmfs_dump_info_t dump_info;
    vmfs_inode_t inode;
    vmfs_bitmap_header_t *fdc_bmp;
    vmfs_bitmap_t *fbb_bmp;
    vmfs_bitmap_t *sbc_bmp;
    vmfs_bitmap_t *pbc_bmp;
    uint32_t entry,item;
    uint64_t vmfs_fsinfo_base = VMFS_FSINFO_BASE;
    uint64_t vmfs_hb_base = VMFS_HB_BASE;
    uint64_t vmfs_volinfo_base = VMFS_VOLINFO_BASE;
    int i;
    int bres;
    pthread_t prog_bitmap_thread;

    fs_open(device);
    vmfs_dump_init(&dump_info);
    blk_bitmap = bitmap;

    /// init progress
    progress_init(&prog, start, fs_info.usedblocks, fs_info.usedblocks, BITMAP, bit_size, 0);
    pc_init_bitmap(bitmap, 0x00, fs_info.totalblock);
    checked = 0;
    /**
     * thread to print progress
     */
    bres = pthread_create(&prog_bitmap_thread, NULL, thread_update_bitmap_pui, NULL);
    if(bres){
	    log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, thread create error\n", __func__, __LINE__);
    }

    /// add base

    pc_set_bit(vmfs_hb_base/vmfs_fs_get_blocksize(fs), bitmap, fs_info.totalblock);
    pc_set_bit(vmfs_fsinfo_base/vmfs_fs_get_blocksize(fs), bitmap, fs_info.totalblock);
    pc_set_bit(vmfs_volinfo_base/vmfs_fs_get_blocksize(fs), bitmap, fs_info.totalblock);

    fdc_bmp = &fs->fdc->bmh;
    log_mesg(3, 0, 0, fs_opt.debug, "Scanning %u FDC entries...\n",fdc_bmp->total_items);

    for(i=0;i<fdc_bmp->total_items;i++) {
        entry = i / fdc_bmp->items_per_bitmap_entry;
        item  = i % fdc_bmp->items_per_bitmap_entry;

        /* Skip undefined/deleted inodes */
        if ((vmfs_inode_get(fs,VMFS_BLK_FD_BUILD(entry,item,0),&inode) == -1) ||
        	!inode.nlink)
            continue;

        inode.fs = fs;
        vmfs_dump_store_inode(fs,dump_info.blk_map,&inode);
        vmfs_inode_foreach_block(&inode,vmfs_dump_store_block,dump_info.blk_map);
    }


    fbb_bmp = fs->fbb;
    sbc_bmp = fs->sbc;
    pbc_bmp = fs->pbc;
    log_mesg(3, 0, 0, fs_opt.debug, "Scanning SBC\n");
    vmfs_bitmap_foreach(sbc_bmp,dump_bitmaps_sb,fs);
    log_mesg(3, 0, 0, fs_opt.debug, "Scanning PBC\n");
    vmfs_bitmap_foreach(pbc_bmp,dump_bitmaps_pb,fs);
    log_mesg(3, 0, 0, fs_opt.debug, "Scanning FBB\n");
    vmfs_bitmap_foreach(fbb_bmp,dump_bitmaps_fb,fs);

    fs_close();
    bitmap_done = 1;
    update_pui(&prog, 1, 1, 1);

    log_mesg(3, 0, 0, fs_opt.debug, "checked block %llu\n", checked);
    log_mesg(0, 0, 0, fs_opt.debug, "%s: Used:%llu, Free:%llu, Status err:%llu\n", __FILE__, used_block, free_block, err_block);

}

void read_super_blocks(char* device, file_system_info* fs_info)
{

    uint32_t alloc,total;
    uint32_t fdc_allocated, fbb_allocated, sbc_allocated, pbc_allocated;

    fs_open(device);
    strncpy(fs_info->fs, vmfs_MAGIC, FS_MAGIC_SIZE);
    total = fs->fbb->bmh.total_items;
    fdc_allocated = vmfs_bitmap_allocated_items(fs->fdc);
    fbb_allocated = vmfs_bitmap_allocated_items(fs->fbb);
    sbc_allocated = vmfs_bitmap_allocated_items(fs->sbc);
    pbc_allocated = vmfs_bitmap_allocated_items(fs->pbc);
    alloc = fdc_allocated + fbb_allocated + sbc_allocated + pbc_allocated;
    log_mesg(3, 0, 0, fs_opt.debug, "allocated fdc %"PRIu32"\n", fdc_allocated);
    log_mesg(3, 0, 0, fs_opt.debug, "allocated fbb %"PRIu32"\n", fbb_allocated);
    log_mesg(3, 0, 0, fs_opt.debug, "allocated sbc %"PRIu32"\n", sbc_allocated);
    log_mesg(3, 0, 0, fs_opt.debug, "allocated pbc %"PRIu32"\n", pbc_allocated);

    fs_info->block_size  = vmfs_fs_get_blocksize(fs);
    fs_info->totalblock  = total;
    fs_info->usedblocks  = alloc;
    fs_info->device_size = vmfs_fs_get_blocksize(fs) * total;
    total_block = total+100;
    log_mesg(3, 0, 0, fs_opt.debug, "block_size %u\n", fs_info->block_size);
    log_mesg(3, 0, 0, fs_opt.debug, "totalblock %llu\n", fs_info->totalblock);
    log_mesg(3, 0, 0, fs_opt.debug, "device_size %llu\n", fs_info->device_size);
    log_mesg(3, 0, 0, fs_opt.debug, "usedblocks %llu\n", fs_info->usedblocks);

    fs_close();
}

void *thread_update_bitmap_pui(void *arg){

    while (bitmap_done == 0) {
	update_pui(&prog, checked, checked, 0);
	sleep(4);
    }
    pthread_exit("exit");
}

