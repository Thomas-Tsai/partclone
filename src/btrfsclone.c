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

#define _XOPEN_SOURCE 500
#define _GNU_SOURCE
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

#include "btrfs/ctree.h"
#include "btrfs/volumes.h"
#include "btrfs/disk-io.h"
#include "btrfs/utils.h"
#include "btrfs/version.h"


#include "partclone.h"
#include "btrfsclone.h"
#include "progress.h"
#include "fs_common.h"

struct btrfs_fs_info *info;
struct btrfs_root *root;
struct btrfs_path path;
int block_size = 0;
uint64_t dev_size = 0;

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
    pos_block = pos/block_size;
    block_end = (pos+length)/block_size;
    if ((pos+length)%block_size > 0)
	block_end++;

    log_mesg(3, 0, 0, fs_opt.debug, "%s: block offset: %llu block count: %llu\n",__FILE__,  pos_block, block_end);

    for(block = pos_block; block < block_end; block++){
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block %i is used\n",__FILE__,  block);
	pc_set_bit(block, bitmap);
    }
}


int check_extent_bitmap(unsigned long* bitmap, u64 bytenr, u64 *num_bytes)
{
    struct btrfs_multi_bio *multi = NULL;
    int ret = 0;
    int mirror = 0;
    //struct btrfs_fs_info *info = root->fs_info;
    u64 maxlen = *num_bytes;


    if (*num_bytes % root->sectorsize)
	return -EINVAL;


    ret = btrfs_map_block(&info->mapping_tree, READ, bytenr, num_bytes,
	    &multi, mirror, NULL);
    if (ret) {
	log_mesg(1, 0, 0, fs_opt.debug, "%s: Couldn't map the block %llu\n", __FILE__, bytenr);
    }

    log_mesg(3, 0, 0, fs_opt.debug, "%s: read data from %llu and size %llu\n", __FILE__, multi->stripes[0].physical, *num_bytes);
    if (*num_bytes > maxlen)
	*num_bytes = maxlen;

    set_bitmap(bitmap, multi->stripes[0].physical, *num_bytes);
    return 0;
}

static void dump_file_extent_item(unsigned long* bitmap, struct extent_buffer *eb,
				   struct btrfs_item *item,
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
    root = root->fs_info->csum_root;

    key.objectid = BTRFS_EXTENT_CSUM_OBJECTID;
    key.type = BTRFS_EXTENT_CSUM_KEY;
    key.offset = 0;

    btrfs_init_path(&path);

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

	data_len = (btrfs_item_size_nr(leaf, path.slots[0]) /
		csum_size) * root->sectorsize;
	leaf_offset = btrfs_item_ptr_offset(leaf, path.slots[0]);
	log_mesg(2, 0, 0, fs_opt.debug, "%s: leaf_offset %lu\n", __FILE__, leaf_offset);
	ret = check_extent_bitmap(bitmap, key.offset, &data_len);
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


void dump_start_leaf(unsigned long* bitmap, struct btrfs_root *root, struct extent_buffer *eb){

    u64 bytenr;
    u64 size;
    u64 objectid;
    u32 type;
    int i;
    struct btrfs_item *item;
    struct btrfs_disk_key disk_key;
    struct btrfs_file_extent_item *fi;
    u32 leaf_size;

    if (!eb)
	return;
    if (btrfs_is_leaf(eb)) {
	size = btrfs_level_size(root, btrfs_root_level(&root->root_item));
	log_mesg(3, 0, 0, fs_opt.debug, "%s: DUMP: leaf %llu\n", __FILE__, (unsigned long long)btrfs_header_bytenr(eb));
	bytenr = (unsigned long long)btrfs_header_bytenr(eb);
	check_extent_bitmap(bitmap, bytenr, &size);
	u32 nr = btrfs_header_nritems(eb);
	for (i = 0 ; i < nr ; i++) {
	    item = btrfs_item_nr(i);
	    btrfs_item_key(eb, &disk_key, i);
	    type = btrfs_disk_key_type(&disk_key);
	    if (type == BTRFS_EXTENT_DATA_KEY){
		fi = btrfs_item_ptr(eb, i,
			struct btrfs_file_extent_item);
		dump_file_extent_item(bitmap, eb, item, i, fi);
	    }
	    if (type == BTRFS_EXTENT_ITEM_KEY){
		objectid = btrfs_disk_key_objectid(&disk_key);
		check_extent_bitmap(bitmap, objectid, &size);
	    }

	}
	return;
    }

    u32 nr = btrfs_header_nritems(eb);
    leaf_size = btrfs_level_size(root, btrfs_header_level(eb) - 1);
    for (i = 0; i < nr; i++) {
	struct extent_buffer *next = read_tree_block(root,
		btrfs_node_blockptr(eb, i),
		leaf_size,
		btrfs_node_ptr_generation(eb, i));
	if (!next) {
	    log_mesg(0, 0, 1, fs_opt.debug, "%s: failed to read %llu in tree %llu\n", __FILE__,
		    (unsigned long long)btrfs_node_blockptr(eb, i),
		    (unsigned long long)btrfs_header_owner(eb));
	    continue;
	}
	if (btrfs_is_leaf(next) && btrfs_header_level(eb) != 1)
	    log_mesg(0, 0, 1, fs_opt.debug, "%s(%i): BUG\n", __FILE__, __LINE__);
	if (btrfs_header_level(next) !=	btrfs_header_level(eb) - 1)
	    log_mesg(0, 0, 1, fs_opt.debug, "%s(%i): BUG\n", __FILE__, __LINE__);

	dump_start_leaf(bitmap, root, next);
	free_extent_buffer(next);
    }


}



/// open device
static void fs_open(char* device){

    struct cache_tree root_cache;
    //struct btrfs_fs_info *info;
    u64 bytenr = 0;
    enum btrfs_open_ctree_flags ctree_flags = OPEN_CTREE_PARTIAL;


    log_mesg(0, 0, 0, fs_opt.debug, "\n%s: btrfs library version = %s\n", __FILE__, BTRFS_BUILD_VERSION);

    radix_tree_init();
    cache_tree_init(&root_cache);
    info = open_ctree_fs_info(device, bytenr, 0, ctree_flags);

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
    int slot;

    fs_open(device);
    dev_size = fs_info.device_size;
    block_size  = btrfs_super_nodesize(info->super_copy);

    set_bitmap(bitmap, BTRFS_SUPER_INFO_OFFSET, block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->extent_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->csum_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->quota_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->dev_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->tree_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->chunk_root->root_item), &block_size);
    //check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->fs_root->root_item), &block_size);

    //log_mesg(3, 0, 0, fs_opt.debug, "%s: super tree done.\n", __FILE__);

    if (info->tree_root->node) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: root tree:\n", __FILE__);
	dump_start_leaf(bitmap, info->tree_root, info->tree_root->node);
    }
    if (info->chunk_root->node) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: chunk tree:\n", __FILE__);
	dump_start_leaf(bitmap, info->chunk_root, info->chunk_root->node);
    }
    tree_root_scan = info->tree_root;

    btrfs_init_path(&path);
    if (!extent_buffer_uptodate(tree_root_scan->node))
	goto no_node;

    key.offset = 0;
    key.objectid = 0;
    btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);
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
	if (btrfs_key_type(&found_key) == BTRFS_ROOT_ITEM_KEY) {
	    unsigned long offset;
	    struct extent_buffer *buf;

	    offset = btrfs_item_ptr_offset(leaf, slot);
	    read_extent_buffer(leaf, &ri, offset, sizeof(ri));
	    buf = read_tree_block(tree_root_scan,
		    btrfs_root_bytenr(&ri),
		    btrfs_level_size(tree_root_scan,
			btrfs_root_level(&ri)),
		    0);
	    if (!extent_buffer_uptodate(buf))
		goto next;
	    dump_start_leaf(bitmap, tree_root_scan, buf);
	    free_extent_buffer(buf);
	}
next:
	path.slots[0]++;
    }
no_node:
    csum_bitmap(bitmap, root);
    btrfs_release_path(&path);
}

void read_super_blocks(char* device, file_system_info* fs_info)
{
    fs_open(device);

    strncpy(fs_info->fs, btrfs_MAGIC, FS_MAGIC_SIZE);

    fs_info->block_size  = btrfs_super_nodesize(root->fs_info->super_copy);
    fs_info->usedblocks  = btrfs_super_bytes_used(root->fs_info->super_copy) / fs_info->block_size;
    fs_info->device_size = btrfs_super_total_bytes(root->fs_info->super_copy);
    fs_info->totalblock  = fs_info->device_size / fs_info->block_size;
    log_mesg(0, 0, 0, fs_opt.debug, "block_size = %i\n", fs_info->block_size);
    log_mesg(0, 0, 0, fs_opt.debug, "usedblock = %lli\n", fs_info->usedblocks);
    log_mesg(0, 0, 0, fs_opt.debug, "device_size = %llu\n", fs_info->device_size);
    log_mesg(0, 0, 0, fs_opt.debug, "totalblock = %lli\n", fs_info->totalblock);


    fs_close();
    log_mesg(0, 0, 0, fs_opt.debug, "%s: fs_close\n", __FILE__);
}

