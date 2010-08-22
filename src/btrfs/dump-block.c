/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
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
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "kerncompat.h"
#include "radix-tree.h"
#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"
#include "version.h"

static void print_extent_leaf(struct btrfs_root *root, struct extent_buffer *l)
{
	int i;
	struct btrfs_item *item;
//	struct btrfs_extent_ref *ref;
	struct btrfs_key key;
	static u64 last = 0;
	static u64 last_len = 0;
	u32 nr = btrfs_header_nritems(l);
	u32 type;

	for (i = 0 ; i < nr ; i++) {
		item = btrfs_item_nr(l, i);
		btrfs_item_key_to_cpu(l, &key, i);
		type = btrfs_key_type(&key);
		switch (type) {
		case BTRFS_EXTENT_ITEM_KEY:
			last_len = key.offset;
			last = key.objectid;
			printf("from %llu, size %llu\n", last, last_len);
			break;
#if 0
		case BTRFS_EXTENT_REF_KEY:
			ref = btrfs_item_ptr(l, i, struct btrfs_extent_ref);
			printf("%llu %llu extent back ref root %llu gen %llu "
			       "owner %llu num_refs %lu\n",
			       (unsigned long long)last,
			       (unsigned long long)last_len,
			       (unsigned long long)btrfs_ref_root(l, ref),
			       (unsigned long long)btrfs_ref_generation(l, ref),
			       (unsigned long long)btrfs_ref_objectid(l, ref),
			       (unsigned long)btrfs_ref_num_refs(l, ref));
			break;
#endif
		};
		fflush(stdout);
	}
}

static void print_extents(struct btrfs_root *root, struct extent_buffer *eb)
{
	int i;
	u32 nr;
	u32 size;

	if (!eb)
		return;
	if (btrfs_is_leaf(eb)) {
		print_extent_leaf(root, eb);
		return;
	}
	size = btrfs_level_size(root, btrfs_header_level(eb) - 1);
	nr = btrfs_header_nritems(eb);
	for (i = 0; i < nr; i++) {
		struct extent_buffer *next = read_tree_block(root,
					     btrfs_node_blockptr(eb, i),
					     size,
					     btrfs_node_ptr_generation(eb, i));
		if (btrfs_is_leaf(next) &&
		    btrfs_header_level(eb) != 1)
			BUG();
		if (btrfs_header_level(next) !=
			btrfs_header_level(eb) - 1)
			BUG();
		print_extents(root, next);
		free_extent_buffer(next);
	}
}

int main(int argc, char **argv)
{
	struct btrfs_root *root;
	struct btrfs_path path;
	struct btrfs_key key;
	struct btrfs_root_item ri;
	struct extent_buffer *leaf;
	struct btrfs_disk_key disk_key;
	struct btrfs_key found_key;
	char uuidbuf[37];
	int ret;
	int slot;
	int extent_only = 0;
	struct btrfs_root *tree_root_scan;
	char *device;

	device = argv[1];
	radix_tree_init();

	root = open_ctree(device, 0, 0);
	if (!root) {
		fprintf(stderr, "unable to open %s\n", device);
		exit(1);
	}
	tree_root_scan = root->fs_info->tree_root;

	btrfs_init_path(&path);
again:
	key.offset = 0;
	key.objectid = 0;
	btrfs_set_key_type(&key, BTRFS_ROOT_ITEM_KEY);
	ret = btrfs_search_slot(NULL, tree_root_scan, &key, &path, 0, 0);
	BUG_ON(ret < 0);
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
					      tree_root_scan->leafsize, 0);
			if(extent_only)
			    print_extents(tree_root_scan, buf);

		}
		path.slots[0]++;
	}
	btrfs_release_path(root, &path);

	if (tree_root_scan == root->fs_info->tree_root &&
	    root->fs_info->log_root_tree) {
		tree_root_scan = root->fs_info->log_root_tree;
		goto again;
	}

	if (extent_only)
		return 0;

	printf("total bytes %llu\n",
	       (unsigned long long)btrfs_super_total_bytes(&root->fs_info->super_copy));
	printf("bytes used %llu\n",
	       (unsigned long long)btrfs_super_bytes_used(&root->fs_info->super_copy));
	uuidbuf[36] = '\0';
	uuid_unparse(root->fs_info->super_copy.fsid, uuidbuf);
	printf("uuid %s\n", uuidbuf);
	printf("%s\n", BTRFS_BUILD_VERSION);
	return 0;
}
