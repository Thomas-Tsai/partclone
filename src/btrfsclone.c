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
#include <pthread.h>
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
int block_size = 0;
uint64_t dev_size = 0;
unsigned long long total_block = 0;

extern progress_bar prog;
static unsigned long long walk_items = 0;
static double est_total = 0;
static int bitmap_phase = 0;
static int bitmap_done = 0;

/* rough work estimate of a tree from its root node: nritems scaled by a
 * presumed fanout per level; only used for fuzzy progress weighting */
static double root_item_weight(struct extent_buffer *eb)
{
    double est = btrfs_header_nritems(eb);
    int level = btrfs_header_level(eb);

    while (level-- > 0)
	est *= 200;
    return est > 0 ? est : 1;
}

void *thread_update_bitmap_pui(void *arg);

/// number of tree blocks that could not be read while building the bitmap;
/// any non-zero value means the resulting image would silently miss data,
/// so read_bitmap() aborts at the end instead of producing a corrupt image.
static unsigned long unreadable_tree_blocks = 0;

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
    struct btrfs_key key;
    struct extent_buffer *leaf;
    struct btrfs_path xpath = { 0 };
    struct btrfs_root *csum_root;
    struct btrfs_root *extent_root;
    int bres;
    pthread_t prog_bitmap_thread;

    total_block = fs_info.totalblock;

    /// init progress: a background thread maps structural phases and the
    /// per-item counter of the single extent-tree walk onto a 0..10000
    /// (0.01%) scale
    progress_init(&prog, 0, 10000, 10000, BITMAP, 1);
    walk_items = 0;
    est_total = 0;
    bitmap_phase = 0;
    bitmap_done = 0;
    bres = pthread_create(&prog_bitmap_thread, NULL, thread_update_bitmap_pui, NULL);
    if (bres) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, thread create error\n", __func__, __LINE__);
    }

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
    /* The marks below are defense-in-depth: every allocated tree block is
     * already covered by the extent-tree walk that follows (each metadata
     * block owns an EXTENT_ITEM/METADATA_ITEM). The handful of explicit
     * root marks costs nothing and keeps the image correct even if the
     * walk has to bail out early. */
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&extent_root->root_item), &bsize, 0);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&csum_root->root_item), &bsize, 0);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->dev_root->root_item), &bsize, 0);
    /* chunk_root and tree_root addresses live in the superblock, not as
     * ROOT_ITEMs in the root tree, so their root_item.bytenr is 0. Read
     * them from the superblock directly. */
    check_extent_bitmap(bitmap, btrfs_super_chunk_root(info->super_copy), &bsize, 0);
    check_extent_bitmap(bitmap, btrfs_super_root(info->super_copy), &bsize, 0);
    check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->fs_root->root_item), &bsize, 0);

    /* Quota Tree: optional, may be NULL when quotas are disabled. */
    if (info->quota_root)
	check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->quota_root->root_item), &bsize, 0);

    /* UUID Tree: optional, may be NULL. Same rationale as quota_root. */
    if (info->uuid_root)
	check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->uuid_root->root_item), &bsize, 0);

    /* Raid Stripe Tree: feature-gated (BTRFS_FEATURE_INCOMPAT_RAID_STRIPE_TREE),
     * may be NULL when the feature is not enabled. */
    if (btrfs_fs_incompat(info, RAID_STRIPE_TREE) && info->stripe_root)
	check_extent_bitmap(bitmap, btrfs_root_bytenr(&info->stripe_root->root_item), &bsize, 0);

    if (btrfs_fs_compat_ro(info, BLOCK_GROUP_TREE)) {
        struct btrfs_root *bg_root = btrfs_block_group_root(info);
        if (bg_root)
            check_extent_bitmap(bitmap, btrfs_root_bytenr(&bg_root->root_item), &bsize, 0);
    }

    /* Log tree: if the filesystem was not unmounted cleanly there may be a
     * log tree root stored in the superblock; it is not reachable from the
     * root tree either, so mark it explicitly as well. */
    if (btrfs_super_log_root(info->super_copy))
	check_extent_bitmap(bitmap, btrfs_super_log_root(info->super_copy), &bsize, 0);

    /* The extent tree is the filesystem's single source of truth about
     * space allocation: every used data extent owns an EXTENT_ITEM and
     * every allocated metadata block owns an EXTENT_ITEM (non-skinny) or
     * a METADATA_ITEM (skinny). Walking it once replaces the old walk of
     * every subvolume and snapshot fs tree, which re-marked the same
     * shared extents once per snapshot AND re-marked the rest of every
     * single-profile chunk because btrfs_map_block() expands the request
     * length when the caller leaves the type-1 clamp unused.
     *
     * All marks here use the type-1 clamp: the extent length comes from
     * the item key itself, so marking a trailing chunk remainder that is
     * not in use must not happen. For EXTENT_ITEM_KEY the key offset is
     * the extent length in bytes; for METADATA_ITEM_KEY the key offset is
     * the tree level, so the length is always the nodesize instead. */
    bitmap_phase = 3;
    if (extent_root->node)
	est_total = root_item_weight(extent_root->node);
    if (est_total <= 0)
	est_total = 1;

    key.objectid = 0;
    key.type = BTRFS_EXTENT_ITEM_KEY;
    key.offset = 0;

    ret = btrfs_search_slot(NULL, extent_root, &key, &xpath, 0, 0);
    if (ret < 0) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Error searching extent tree %d; the bitmap would be incomplete, aborting instead of creating a corrupt image\n", __FILE__, ret);
    }
    if (ret > 0 && xpath.slots[0])
	xpath.slots[0]--;

    while (1) {
	if (xpath.slots[0] >= btrfs_header_nritems(xpath.nodes[0])) {
	    ret = btrfs_next_leaf(extent_root, &xpath);
	    if (ret < 0) {
		unreadable_tree_blocks++;
		log_mesg(0, 0, 1, fs_opt.debug, "%s: Error going to next extent tree leaf %d; the bitmap would be incomplete, aborting\n", __FILE__, ret);
		break;
	    }
	    if (ret > 0)
		break;
	}
	leaf = xpath.nodes[0];
	btrfs_item_key_to_cpu(leaf, &key, xpath.slots[0]);
	walk_items++;

	if (key.type == BTRFS_EXTENT_ITEM_KEY) {
	    u64 len = key.offset;

	    log_mesg(3, 0, 0, fs_opt.debug, "%s: EXTENT_ITEM %llu len %llu\n", __FILE__, key.objectid, key.offset);
	    check_extent_bitmap(bitmap, key.objectid, &len, 1);
	} else if (key.type == BTRFS_METADATA_ITEM_KEY) {
	    u64 len = bsize;

	    log_mesg(3, 0, 0, fs_opt.debug, "%s: METADATA_ITEM %llu level %llu\n", __FILE__, key.objectid, key.offset);
	    check_extent_bitmap(bitmap, key.objectid, &len, 1);
	}
	xpath.slots[0]++;
    }

    btrfs_release_path(&xpath);
    bitmap_done = 1;
    update_pui(&prog, 1, 1, 1);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: bitmap est items = %.0f, walked items = %llu\n", __FILE__, est_total, walk_items);
    if (unreadable_tree_blocks)
	log_mesg(0, 1, 1, fs_opt.debug, "%s: %lu tree block(s) could not be read; the bitmap would be incomplete, aborting instead of creating a corrupt image\n", __FILE__, unreadable_tree_blocks);
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

void *thread_update_bitmap_pui(void *arg){

    static const unsigned long long stage_floor[] = { 1 * 100, 5 * 100, 10 * 100, 15 * 100 };
    unsigned long long display;

    while (bitmap_done == 0) {
	if (bitmap_phase < 3) {
	    display = stage_floor[bitmap_phase];
	} else {
	    double frac = (double)walk_items / est_total;

	    if (frac > 1.0)
		frac = 1.0;
	    display = 15 * 100 + (unsigned long long)(frac * 85 * 100);
	}
	update_pui(&prog, display, display, 0);
	sleep(2);
    }
    pthread_exit("exit");
}

