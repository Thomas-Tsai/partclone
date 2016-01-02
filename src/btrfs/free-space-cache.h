/*
 * Copyright (C) 2009 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __BTRFS_FREE_SPACE_CACHE
#define __BTRFS_FREE_SPACE_CACHE

struct btrfs_free_space {
	struct rb_node offset_index;
	u64 offset;
	u64 bytes;
	unsigned long *bitmap;
	struct list_head list;
};

struct btrfs_free_space_ctl {
	struct rb_root free_space_offset;
	u64 free_space;
	int extents_thresh;
	int free_extents;
	int total_bitmaps;
	int unit;
	u64 start;
	void *private;
	u32 sectorsize;
};

int load_free_space_cache(struct btrfs_fs_info *fs_info,
			  struct btrfs_block_group_cache *block_group);

void __btrfs_remove_free_space_cache(struct btrfs_free_space_ctl *ctl);
void btrfs_remove_free_space_cache(struct btrfs_block_group_cache
				     *block_group);
void btrfs_dump_free_space(struct btrfs_block_group_cache *block_group,
			   u64 bytes);
struct btrfs_free_space *
btrfs_find_free_space(struct btrfs_free_space_ctl *ctl, u64 offset, u64 bytes);
int btrfs_init_free_space_ctl(struct btrfs_block_group_cache *block_group,
			      int sectorsize);
void unlink_free_space(struct btrfs_free_space_ctl *ctl,
		       struct btrfs_free_space *info);
#endif
