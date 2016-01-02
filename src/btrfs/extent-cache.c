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
#include <stdio.h>
#include <stdlib.h>
#include "kerncompat.h"
#include "extent-cache.h"
#include "rbtree-utils.h"

struct cache_extent_search_range {
	u64 objectid;
	u64 start;
	u64 size;
};

static int cache_tree_comp_range(struct rb_node *node, void *data)
{
	struct cache_extent *entry;
	struct cache_extent_search_range *range;

	range = (struct cache_extent_search_range *)data;
	entry = rb_entry(node, struct cache_extent, rb_node);

	if (entry->start + entry->size <= range->start)
		return 1;
	else if (range->start + range->size <= entry->start)
		return -1;
	else
		return 0;
}

static int cache_tree_comp_nodes(struct rb_node *node1, struct rb_node *node2)
{
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	entry = rb_entry(node2, struct cache_extent, rb_node);
	range.start = entry->start;
	range.size = entry->size;

	return cache_tree_comp_range(node1, (void *)&range);
}

static int cache_tree_comp_range2(struct rb_node *node, void *data)
{
	struct cache_extent *entry;
	struct cache_extent_search_range *range;

	range = (struct cache_extent_search_range *)data;
	entry = rb_entry(node, struct cache_extent, rb_node);

	if (entry->objectid < range->objectid)
		return 1;
	else if (entry->objectid > range->objectid)
		return -1;
	else if (entry->start + entry->size <= range->start)
		return 1;
	else if (range->start + range->size <= entry->start)
		return -1;
	else
		return 0;
}

static int cache_tree_comp_nodes2(struct rb_node *node1, struct rb_node *node2)
{
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	entry = rb_entry(node2, struct cache_extent, rb_node);
	range.objectid = entry->objectid;
	range.start = entry->start;
	range.size = entry->size;

	return cache_tree_comp_range2(node1, (void *)&range);
}

void cache_tree_init(struct cache_tree *tree)
{
	tree->root = RB_ROOT;
}

static struct cache_extent *
alloc_cache_extent(u64 objectid, u64 start, u64 size)
{
	struct cache_extent *pe = malloc(sizeof(*pe));

	if (!pe)
		return pe;

	pe->objectid = objectid;
	pe->start = start;
	pe->size = size;
	return pe;
}

static int __add_cache_extent(struct cache_tree *tree,
			      u64 objectid, u64 start, u64 size)
{
	struct cache_extent *pe = alloc_cache_extent(objectid, start, size);
	int ret;

	if (!pe) {
		fprintf(stderr, "memory allocation failed\n");
		exit(1);
	}

	ret = insert_cache_extent(tree, pe);
	if (ret)
		free(pe);

	return ret;
}

int add_cache_extent(struct cache_tree *tree, u64 start, u64 size)
{
	return __add_cache_extent(tree, 0, start, size);
}

int add_cache_extent2(struct cache_tree *tree,
		      u64 objectid, u64 start, u64 size)
{
	return __add_cache_extent(tree, objectid, start, size);
}

int insert_cache_extent(struct cache_tree *tree, struct cache_extent *pe)
{
	return rb_insert(&tree->root, &pe->rb_node, cache_tree_comp_nodes);
}

int insert_cache_extent2(struct cache_tree *tree, struct cache_extent *pe)
{
	return rb_insert(&tree->root, &pe->rb_node, cache_tree_comp_nodes2);
}

struct cache_extent *lookup_cache_extent(struct cache_tree *tree,
					 u64 start, u64 size)
{
	struct rb_node *node;
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	range.start = start;
	range.size = size;
	node = rb_search(&tree->root, &range, cache_tree_comp_range, NULL);
	if (!node)
		return NULL;

	entry = rb_entry(node, struct cache_extent, rb_node);
	return entry;
}

struct cache_extent *lookup_cache_extent2(struct cache_tree *tree,
					 u64 objectid, u64 start, u64 size)
{
	struct rb_node *node;
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	range.objectid = objectid;
	range.start = start;
	range.size = size;
	node = rb_search(&tree->root, &range, cache_tree_comp_range2, NULL);
	if (!node)
		return NULL;

	entry = rb_entry(node, struct cache_extent, rb_node);
	return entry;
}

struct cache_extent *search_cache_extent(struct cache_tree *tree, u64 start)
{
	struct rb_node *next;
	struct rb_node *node;
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	range.start = start;
	range.size = 1;
	node = rb_search(&tree->root, &range, cache_tree_comp_range, &next);
	if (!node)
		node = next;
	if (!node)
		return NULL;

	entry = rb_entry(node, struct cache_extent, rb_node);
	return entry;
}

struct cache_extent *search_cache_extent2(struct cache_tree *tree,
					 u64 objectid, u64 start)
{
	struct rb_node *next;
	struct rb_node *node;
	struct cache_extent *entry;
	struct cache_extent_search_range range;

	range.objectid = objectid;
	range.start = start;
	range.size = 1;
	node = rb_search(&tree->root, &range, cache_tree_comp_range2, &next);
	if (!node)
		node = next;
	if (!node)
		return NULL;

	entry = rb_entry(node, struct cache_extent, rb_node);
	return entry;
}

struct cache_extent *first_cache_extent(struct cache_tree *tree)
{
	struct rb_node *node = rb_first(&tree->root);

	if (!node)
		return NULL;
	return rb_entry(node, struct cache_extent, rb_node);
}

struct cache_extent *prev_cache_extent(struct cache_extent *pe)
{
	struct rb_node *node = rb_prev(&pe->rb_node);

	if (!node)
		return NULL;
	return rb_entry(node, struct cache_extent, rb_node);
}

struct cache_extent *next_cache_extent(struct cache_extent *pe)
{
	struct rb_node *node = rb_next(&pe->rb_node);

	if (!node)
		return NULL;
	return rb_entry(node, struct cache_extent, rb_node);
}

void remove_cache_extent(struct cache_tree *tree, struct cache_extent *pe)
{
	rb_erase(&pe->rb_node, &tree->root);
}

void cache_tree_free_extents(struct cache_tree *tree,
			     free_cache_extent free_func)
{
	struct cache_extent *ce;

	while ((ce = first_cache_extent(tree))) {
		remove_cache_extent(tree, ce);
		free_func(ce);
	}
}

static void free_extent_cache(struct cache_extent *pe)
{
	free(pe);
}

void free_extent_cache_tree(struct cache_tree *tree)
{
	cache_tree_free_extents(tree, free_extent_cache);
}
