/*
 * Copyright (C) 2012 STRATO.  All rights reserved.
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

#ifndef _BTRFS_QGROUP_H
#define _BTRFS_QGROUP_H

#include "ioctl.h"
#include "kerncompat.h"

struct btrfs_qgroup;

typedef int (*btrfs_qgroup_filter_func)(struct btrfs_qgroup *, u64);
typedef int (*btrfs_qgroup_comp_func)(struct btrfs_qgroup *,
				      struct btrfs_qgroup *, int);


struct btrfs_qgroup_filter {
	btrfs_qgroup_filter_func filter_func;
	u64 data;
};

struct btrfs_qgroup_comparer {
	btrfs_qgroup_comp_func comp_func;
	int is_descending;
};

struct btrfs_qgroup_filter_set {
	int total;
	int nfilters;
	struct btrfs_qgroup_filter filters[0];
};

struct btrfs_qgroup_comparer_set {
	int total;
	int ncomps;
	struct btrfs_qgroup_comparer comps[0];
};

enum btrfs_qgroup_column_enum {
	BTRFS_QGROUP_QGROUPID,
	BTRFS_QGROUP_RFER,
	BTRFS_QGROUP_EXCL,
	BTRFS_QGROUP_MAX_RFER,
	BTRFS_QGROUP_MAX_EXCL,
	BTRFS_QGROUP_PARENT,
	BTRFS_QGROUP_CHILD,
	BTRFS_QGROUP_ALL,
};

enum btrfs_qgroup_comp_enum {
	BTRFS_QGROUP_COMP_QGROUPID,
	BTRFS_QGROUP_COMP_RFER,
	BTRFS_QGROUP_COMP_EXCL,
	BTRFS_QGROUP_COMP_MAX_RFER,
	BTRFS_QGROUP_COMP_MAX_EXCL,
	BTRFS_QGROUP_COMP_MAX
};

enum btrfs_qgroup_filter_enum {
	BTRFS_QGROUP_FILTER_PARENT,
	BTRFS_QGROUP_FILTER_ALL_PARENT,
	BTRFS_QGROUP_FILTER_MAX,
};

int btrfs_qgroup_parse_sort_string(char *opt_arg,
				struct btrfs_qgroup_comparer_set **comps);
u64 btrfs_get_path_rootid(int fd);
int btrfs_show_qgroups(int fd, struct btrfs_qgroup_filter_set *,
		       struct btrfs_qgroup_comparer_set *);
void btrfs_qgroup_setup_print_column(enum btrfs_qgroup_column_enum column);
struct btrfs_qgroup_filter_set *btrfs_qgroup_alloc_filter_set(void);
void btrfs_qgroup_free_filter_set(struct btrfs_qgroup_filter_set *filter_set);
int btrfs_qgroup_setup_filter(struct btrfs_qgroup_filter_set **filter_set,
			      enum btrfs_qgroup_filter_enum, u64 data);
struct btrfs_qgroup_comparer_set *btrfs_qgroup_alloc_comparer_set(void);
void btrfs_qgroup_free_comparer_set(struct btrfs_qgroup_comparer_set *comp_set);
int btrfs_qgroup_setup_comparer(struct btrfs_qgroup_comparer_set **comp_set,
				enum btrfs_qgroup_comp_enum comparer,
				int is_descending);
u64 parse_qgroupid(char *p);
int qgroup_inherit_size(struct btrfs_qgroup_inherit *p);
int qgroup_inherit_add_group(struct btrfs_qgroup_inherit **inherit, char *arg);
int qgroup_inherit_add_copy(struct btrfs_qgroup_inherit **inherit, char *arg,
			    int type);

#endif
