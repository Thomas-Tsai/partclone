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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "ctree.h"
#include "disk-io.h"
#include "transaction.h"

/*
 * walks the btree of allocated inodes and find a hole.
 */
int btrfs_find_free_objectid(struct btrfs_trans_handle *trans,
			     struct btrfs_root *root,
			     u64 dirid, u64 *objectid)
{
	struct btrfs_path *path;
	struct btrfs_key key;
	int ret;
	int slot = 0;
	u64 last_ino = 0;
	int start_found;
	struct extent_buffer *l;
	struct btrfs_key search_key;
	u64 search_start = dirid;

	path = btrfs_alloc_path();
	BUG_ON(!path);
	search_start = root->last_inode_alloc;
	search_start = max((unsigned long long)search_start,
				BTRFS_FIRST_FREE_OBJECTID);
	search_key.objectid = search_start;
	search_key.offset = 0;

	btrfs_init_path(path);
	start_found = 0;
	ret = btrfs_search_slot(trans, root, &search_key, path, 0, 0);
	if (ret < 0)
		goto error;

	if (path->slots[0] > 0)
		path->slots[0]--;

	while (1) {
		l = path->nodes[0];
		slot = path->slots[0];
		if (slot >= btrfs_header_nritems(l)) {
			ret = btrfs_next_leaf(root, path);
			if (ret == 0)
				continue;
			if (ret < 0)
				goto error;
			if (!start_found) {
				*objectid = search_start;
				start_found = 1;
				goto found;
			}
			*objectid = last_ino > search_start ?
				last_ino : search_start;
			goto found;
		}
		btrfs_item_key_to_cpu(l, &key, slot);
		if (key.objectid >= search_start) {
			if (start_found) {
				if (last_ino < search_start)
					last_ino = search_start;
				if (key.objectid > last_ino) {
					*objectid = last_ino;
					goto found;
				}
			}
		}
		start_found = 1;
		last_ino = key.objectid + 1;
		path->slots[0]++;
	}
	// FIXME -ENOSPC
found:
	root->last_inode_alloc = *objectid;
	btrfs_free_path(path);
	BUG_ON(*objectid < search_start);
	return 0;
error:
	btrfs_free_path(path);
	return ret;
}
