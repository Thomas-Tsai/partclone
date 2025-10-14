/**
 * btrfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read btrfs super block and extent
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <getopt.h>
#include <uuid/uuid.h>

#include "btrfs/kernel-shared/ctree.h"
#include "btrfs/kernel-shared/volumes.h"
#include "btrfs/kernel-shared/disk-io.h"
#include "btrfs/common/extent-cache.h"
#include "kernel-shared/tree-checker.h"
#include "btrfs/common/utils.h"
#include "btrfs/libbtrfs/version.h"


#include "partclone.h"
#include "btrfsclone.h"
#include "progress.h"
#include "fs_common.h"

struct btrfs_fs_info *info;
struct btrfs_root *root;
struct btrfs_path path = { 0 };
int block_size = 0;
uint64_t dev_size = 0;
unsigned long long total_block = 0;

///set useb block
static void set_bitmap(unsigned long* bitmap, uint64_t pos, uint64_t length){
    uint64_t block;
    uint64_t pos_block;
    uint64_t block_end;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: offset: %llu size: %llu block_size: %i\n", __FILE__,  pos, length, block_size);
    if (pos > dev_size) {
	log_mesg(1, 0, 0, fs_opt.debug, "%s: offset(%llu) larger than device size(%llu), skip it.\n", __FILE__,  pos, dev_size);
	return;
    } 
    if ((pos+length) > dev_size){
        length = dev_size-pos;
    }
    pos_block = pos/block_size;
    block_end = (pos+length)/block_size;
    if ((pos+length)%block_size > 0)
	block_end++;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: block offset: %llu block count: %llu\n",__FILE__,  pos_block, block_end);

    for(block = pos_block; block < block_end; block++){
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block %i is used\n",__FILE__,  block);
	pc_set_bit(block, bitmap, total_block);
    }
}


static int get_num_copies(struct btrfs_fs_info *fs_info, u64 logical, u64 len)
{
	struct btrfs_mapping_tree *map_tree = &fs_info->mapping_tree;
	struct cache_extent *ce;
	struct map_lookup *map;
	int ret;

	ce = search_cache_extent(&map_tree->cache_tree, logical);
	if (!ce) {
		log_mesg(1, 0, 0, fs_opt.debug, "No mapping for %llu-%llu\n",
			(unsigned long long)logical,
			(unsigned long long)logical+len);
		return 1;
	}
	if (ce->start > logical || ce->start + ce->size < logical) {
		log_mesg(1, 0, 0, fs_opt.debug, "Invalid mapping for %llu-%llu, got \"%llu-%llu\"\n",
			(unsigned long long)logical,
			(unsigned long long)logical+len,
			(unsigned long long)ce->start,
			(unsigned long long)ce->start + ce->size);
		return 1;
	}
	map = container_of(ce, struct map_lookup, ce);

	if (map->type & (BTRFS_BLOCK_GROUP_DUP | BTRFS_BLOCK_GROUP_RAID1_MASK))
		ret = map->num_stripes;
	else if (map->type & BTRFS_BLOCK_GROUP_RAID10)
		ret = map->sub_stripes;
	else if (map->type & BTRFS_BLOCK_GROUP_RAID5)
		ret = 2;
	else if (map->type & BTRFS_BLOCK_GROUP_RAID6)
		ret = 3;
	else
		ret = 1;
	return ret;
}

int check_extent_bitmap(unsigned long* bitmap, u64 bytenr, u64 *num_bytes, int type)
{
    struct btrfs_multi_bio *multi = NULL;
    int ret = 0;
    int mirror;
    int num_copies;
    u64 maxlen;


    log_mesg(3, 0, 0, fs_opt.debug, "%s: check_extent_bitmap bytenr %llu and size %llu\n", __FILE__, bytenr, *num_bytes);
    if (*num_bytes % root->fs_info->sectorsize)
	return -EINVAL;

    if (type == 1){
	maxlen = *num_bytes;
    }

    num_copies = get_num_copies(info, bytenr, *num_bytes);

    for (mirror = 1; mirror <= num_copies; mirror++) {
        u64 length = *num_bytes;
        ret = btrfs_map_block(info, READ, bytenr, &length,
                &multi, mirror, NULL);
        if (ret) {
            log_mesg(1, 0, 0, fs_opt.debug, "%s: Couldn't map the block %llu mirror %d\n", __FILE__, bytenr, mirror);
            continue;
        }

        log_mesg(3, 0, 0, fs_opt.debug, "%s: read data from %llu and size %llu\n", __FILE__, multi->stripes[0].physical, length);

        if (type == 1){
            length = maxlen;
        }

        set_bitmap(bitmap, multi->stripes[0].physical, length);
        free(multi);
        multi = NULL;
    }
    return 0;
}

static void dump_file_extent_item(unsigned long* bitmap, struct extent_buffer *eb,
				   int slot,
				   struct btrfs_file_extent_item *fi)
{
	int extent_type = btrfs_file_extent_type(eb, fi);

	if (extent_type == BTRFS_FILE_EXTENT_INLINE) {
	    return;
	}

	if (extent_type == BTRFS_FILE_EXTENT_PREALLOC) {
		log_mesg(3, 0, 0, fs_opt.debug, "%s: DUMP: prealloc data disk byte %llu nr %llu\n", __FILE__, 
		  (unsigned long long)btrfs_file_extent_disk_bytenr(eb, fi),
		  (unsigned long long)btrfs_file_extent_disk_num_bytes(eb, fi));
		set_bitmap(bitmap, (unsigned long long)btrfs_file_extent_disk_bytenr(eb, fi), 
		                   (unsigned long long)btrfs_file_extent_disk_num_bytes(eb, fi) );
		return;
	}
	log_mesg(3, 0, 0, fs_opt.debug, "DUMP: extent data disk byte %llu nr %llu\n",
		(unsigned long long)btrfs_file_extent_disk_bytenr(eb, fi),
		(unsigned long long)btrfs_file_extent_disk_num_bytes(eb, fi));
		set_bitmap(bitmap, (unsigned long long)btrfs_file_extent_disk_bytenr(eb, fi), 
		                   (unsigned long long)btrfs_file_extent_disk_num_bytes(eb, fi) );
}

int csum_bitmap(unsigned long* bitmap, struct btrfs_root *root){
    struct extent_buffer *leaf;
    struct btrfs_key key;
    u64 offset = 0, num_bytes = 0;
    u16 csum_size = btrfs_super_csum_size(root->fs_info->super_copy);
    int ret = 0;
    u64 data_len;
    unsigned long leaf_offset;

    log_mesg(2, 0, 0, fs_opt.debug, "%s: csum_bitmap\n", __FILE__);
    //root = root->fs_info->_csum_root;
    root = btrfs_csum_root(root->fs_info, 0);

    key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
    key.type = BTRFS_EXTENT_CSUM_KEY;
    key.offset = 0;

    ret = btrfs_search_slot(NULL, root, &key, &path, 0, 0);
    if (ret < 0) {
	log_mesg(0, 0, 1, fs_opt.debug, "%s: Error searching csum tree %d\n", __FILE__, ret);
	btrfs_free_path(&path);
	return ret;
    }

    if (ret > 0 && path.slots[0])
	path.slots[0]--;
    ret = 0;

    while (1) {
	if (path.slots[0] >= btrfs_header_nritems(path.nodes[0])) {
	    ret = btrfs_next_leaf(root, &path);
	    if (ret < 0) {
		log_mesg(0, 0, 1, fs_opt.debug, "%s: Error going to next leaf %d\n", __FILE__, ret);
		break;
	    }
	    if (ret)
		break;
	}
	leaf = path.nodes[0];

	btrfs_item_key_to_cpu(leaf, &key, path.slots[0]);
	if (key.type != BTRFS_EXTENT_CSUM_KEY) {
	    path.slots[0]++;
	    continue;
	}

	data_len = (btrfs_item_size(leaf, path.slots[0]) /
		csum_size) * root->fs_info->sectorsize;
	leaf_offset = btrfs_item_ptr_offset(leaf, path.slots[0]);
	log_mesg(2, 0, 0, fs_opt.debug, "%s: leaf_offset %lu\n", __FILE__, leaf_offset);
	log_mesg(2, 0, 0, fs_opt.debug, "%s: key.offset %lu\n", __FILE__, key.offset);
	ret = check_extent_bitmap(bitmap, key.offset, &data_len, 0);
	if (ret)
	    break;
	if (!num_bytes) {
	    offset = key.offset;
	} else if (key.offset != offset + num_bytes) {

	    offset = key.offset;
	    num_bytes = 0;
	}
	num_bytes += data_len;
	path.slots[0]++;
    }

    btrfs_release_path(&path);

    return ret;

}


void dump_start_leaf(unsigned long* bitmap, struct btrfs_root *root, struct extent_buffer *eb, int follow){

    u64 bytenr;
    u64 size;
    u64 objectid;
    u64 offset;
    u32 type;
    int i;
    struct btrfs_disk_key disk_key;
    struct btrfs_file_extent_item *fi;
    int root_eb_level = btrfs_header_level(eb);

    if (!eb)
	return;
    u32 nr = btrfs_header_nritems(eb);
    if (btrfs_is_leaf(eb)) {
	size = (u64)root->fs_info->nodesize;
	log_mesg(3, 0, 0, fs_opt.debug, "%s: DUMP: leaf %llu\n", __FILE__, (unsigned long long)btrfs_header_bytenr(eb));
	bytenr = (unsigned long long)btrfs_header_bytenr(eb);
	check_extent_bitmap(bitmap, bytenr, &size, 0);
	for (i = 0 ; i < nr ; i++) {
	    btrfs_item_key(eb, &disk_key, i);
	    type = btrfs_disk_key_type(&disk_key);
	    if (type == BTRFS_EXTENT_DATA_KEY){
		fi = btrfs_item_ptr(eb, i,
			struct btrfs_file_extent_item);
		dump_file_extent_item(bitmap, eb, i, fi);
	    }
	    if (type == BTRFS_EXTENT_ITEM_KEY){
		objectid = btrfs_disk_key_objectid(&disk_key);
		offset = btrfs_disk_key_offset(&disk_key);
		log_mesg(3, 0, 0, fs_opt.debug, "%s: type == BTRFS_EXTENT_ITEM_KEY %llu %llu\n", __FILE__, objectid, offset);
		check_extent_bitmap(bitmap, objectid, &offset, 1);
	    }

	}
	return;
    }

/*
    for (i = 0; i < nr; i++) {
	u64 blocknr = btrfs_node_blockptr(eb, i);
	btrfs_node_key(eb, &disk_key, i);
	btrfs_disk_key_to_cpu(&key, &disk_key);
	//printf("\t");
	//btrfs_print_key(&disk_key);
	//printf(" block %llu (%llu) gen %llu\n",
	//(unsigned long long)blocknr,
	//(unsigned long long)blocknr / size,
	//(unsigned long long)btrfs_node_ptr_generation(eb, i));
	//fflush(stdout);
    }
*/


    if (!follow)
	return;
    log_mesg(3, 0, 0, fs_opt.debug, "%s: follow %i\n", __FILE__, follow);
    for (i = 0; i < nr; i++) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: follow %i\n", __FILE__, follow);
	bytenr = (unsigned long long)btrfs_header_bytenr(eb);
	check_extent_bitmap(bitmap, bytenr, &size, 0);
        struct btrfs_tree_parent_check check = {
            .owner_root = btrfs_header_owner(eb),
            .transid = btrfs_node_ptr_generation(eb, i),
            .level = btrfs_header_level(eb),
        };
	struct extent_buffer *next = read_tree_block(root->fs_info,
		btrfs_node_blockptr(eb, i),  &check);
	bytenr = (unsigned long long)btrfs_header_bytenr(next);
	check_extent_bitmap(bitmap, bytenr, &size, 0);
	if (!extent_buffer_uptodate(next)) {
	    log_mesg(0, 0, 1, fs_opt.debug, "%s: failed to read %llu in tree %llu\n", __FILE__,
		    (unsigned long long)btrfs_node_blockptr(eb, i),
		    (unsigned long long)btrfs_header_owner(eb));
	    continue;
	}
	if (btrfs_is_leaf(next) && btrfs_header_level(eb) != 1)
	    log_mesg(0, 0, 1, fs_opt.debug, "%s(%i): BUG\n", __FILE__, __LINE__);
	if (btrfs_header_level(next) !=	btrfs_header_level(eb) - 1)
	    log_mesg(0, 0, 1, fs_opt.debug, "%s(%i): BUG\n", __FILE__, __LINE__);

	dump_start_leaf(bitmap, root, next, 1);
	free_extent_buffer(next);
    }


}



/// open device
static void fs_open(char* device){

    struct cache_tree root_cache;
    //struct btrfs_fs_info *info;
    u64 bytenr = 0;

    log_mesg(0, 0, 0, fs_opt.debug, "\n%s: btrfs library version = %s\n", __FILE__, BTRFS_BUILD_VERSION);

    cache_tree_init(&root_cache);
    struct open_ctree_args oca = { 0 };

    oca.filename = device;
    oca.sb_bytenr = bytenr;
    oca.root_tree_bytenr = 0;
    oca.chunk_tree_bytenr = 0;
    oca.flags = OPEN_CTREE_PARTIAL;
    info = open_ctree_fs_info(&oca);

    if (!info) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Couldn't open file system\n", __FILE__);
    }
    root = info->fs_root;

    if (!extent_buffer_uptodate(info->tree_root->node) ||
	    !extent_buffer_uptodate(info->dev_root->node) ||
	    !extent_buffer_uptodate(info->chunk_root->node)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Critical roots corrupted, unable to fsck the FS\n", __FILE__);
    }


    if(!root){
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Unable to open %s\n", __FILE__, device);	
    }

}

/// close device
static void fs_close(){
    close_ctree(root);
}

void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui)
{

    int ret;
    struct btrfs_root *tree_root_scan;
    struct btrfs_key key;
    struct btrfs_disk_key disk_key;
    struct btrfs_key found_key;
    struct extent_buffer *leaf;
    struct btrfs_root_item ri;
    struct btrfs_root *csum_root;
    struct btrfs_root *extent_root;
    int slot;

    total_block = fs_info.totalblock;

    fs_open(device);
    dev_size = fs_info.device_size;
    block_size  = btrfs_super_nodesize(info->super_copy);
    u64 bsize = (u64)block_size;
    u64 sb_mirror_offset = 0;
    int sb_mirror = 0;

    set_bitmap(bitmap, 0, BTRFS_SUPER_INFO_OFFSET); // some data like mbr maybe in
    set_bitmap(bitmap, BTRFS_SUPER_INFO_OFFSET, block_size);
    for (sb_mirror = 0; sb_mirror <= BTRFS_SUPER_MIRROR_MAX; sb_mirror++){
        sb_mirror_offset = btrfs_sb_offset(sb_mirror);
        log_mesg(1, 0, 0, fs_opt.debug, "%s: sb mirror %i: %X\n", __FILE__, sb_mirror, sb_mirror_offset);
        set_bitmap(bitmap, sb_mirror_offset, block_size);
    } 

    csum_root = btrfs_csum_root(info, 0);
    extent_root = btrfs_extent_root(info, 0);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&extent_root->root_item), &bsize, 0);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&csum_root->root_item), &bsize, 0);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->quota_root->root_item), &block_size);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->dev_root->root_item), &bsize, 0);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->tree_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->chunk_root->root_item), &bsize);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->fs_root->root_item), &bsize, 0);

    //log_mesg(3, 0, 0, fs_opt.debug, "%s: super tree done.\n", __FILE__);

    if (info->tree_root->node) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: root tree:\n", __FILE__);
	dump_start_leaf(bitmap, info->tree_root, info->tree_root->node, 1);
    }
    if (info->chunk_root->node) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: chunk tree:\n", __FILE__);
	dump_start_leaf(bitmap, info->chunk_root, info->chunk_root->node, 1);
    }
    tree_root_scan = info->tree_root;

    if (!extent_buffer_uptodate(tree_root_scan->node))
	goto no_node;

    key.offset = 0;
    key.objectid = 0;
    key.type = BTRFS_ROOT_ITEM_KEY;
    ret = btrfs_search_slot(NULL, tree_root_scan, &key, &path, 0, 0);
    while(1) {
	leaf = path.nodes[0];
	slot = path.slots[0];
	if (slot >= btrfs_header_nritems(leaf)) {
	    ret = btrfs_next_leaf(tree_root_scan, &path);
	    if (ret != 0)
		break;
	    leaf = path.nodes[0];
	    slot = path.slots[0];
	}
	btrfs_item_key(leaf, &disk_key, path.slots[0]);
	btrfs_disk_key_to_cpu(&found_key, &disk_key);
	if (found_key.type == BTRFS_ROOT_ITEM_KEY) {
	    unsigned long offset;
	    struct extent_buffer *buf;
            struct btrfs_tree_parent_check check = { 0 };

	    offset = btrfs_item_ptr_offset(leaf, slot);
	    read_extent_buffer(leaf, &ri, offset, sizeof(ri));
	    buf = read_tree_block(tree_root_scan->fs_info, btrfs_root_bytenr(&ri), &check);
	    if (!extent_buffer_uptodate(buf))
		goto next;
	    dump_start_leaf(bitmap, tree_root_scan, buf, 1);
	    free_extent_buffer(buf);
	}
next:
	path.slots[0]++;
    }
no_node:
    //csum_bitmap(bitmap, root);
    btrfs_release_path(&path);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{    
    fs_open(device);

    strncpy(fs_info->fs, btrfs_MAGIC, FS_MAGIC_SIZE);

    fs_info->block_size  = btrfs_super_nodesize(root->fs_info->super_copy);
    fs_info->usedblocks  = btrfs_super_bytes_used(root->fs_info->super_copy) / fs_info->block_size;
    fs_info->superBlockUsedBlocks = fs_info->usedblocks;
    fs_info->device_size = btrfs_super_total_bytes(root->fs_info->super_copy);
    fs_info->totalblock  = fs_info->device_size / fs_info->block_size;
    log_mesg(0, 0, 0, fs_opt.debug, "block_size = %i\n", fs_info->block_size);
    log_mesg(0, 0, 0, fs_opt.debug, "usedblock = %lli\n", fs_info->usedblocks);
    log_mesg(0, 0, 0, fs_opt.debug, "superBlockUsedBlocks = %lli\n", fs_info->superBlockUsedBlocks);
    log_mesg(0, 0, 0, fs_opt.debug, "device_size = %llu\n", fs_info->device_size);
    log_mesg(0, 0, 0, fs_opt.debug, "totalblock = %lli\n", fs_info->totalblock);

    fs_close();
    log_mesg(0, 0, 0, fs_opt.debug, "%s: fs_close\n", __FILE__);
}

