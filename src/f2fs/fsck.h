/**
 * fsck.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _FSCK_H_
#define _FSCK_H_

#include "f2fs.h"

/* fsck.c */
struct orphan_info {
	u32 nr_inodes;
	u32 *ino_list;
};

struct f2fs_fsck {
	struct f2fs_sb_info sbi;

	struct orphan_info orphani;
	struct chk_result {
		u64 valid_blk_cnt;
		u32 valid_nat_entry_cnt;
		u32 valid_node_cnt;
		u32 valid_inode_cnt;
		u32 multi_hard_link_files;
		u64 sit_valid_blocks;
		u32 sit_free_segs;
	} chk;

	struct hard_link_node *hard_link_list_head;

	char *main_seg_usage;
	char *main_area_bitmap;
	char *nat_area_bitmap;
	char *sit_area_bitmap;

	u64 main_area_bitmap_sz;
	u32 nat_area_bitmap_sz;
	u32 sit_area_bitmap_sz;

	u64 nr_main_blks;
	u32 nr_nat_entries;

	u32 dentry_depth;
};

#define BLOCK_SZ		4096
struct block {
	unsigned char buf[BLOCK_SZ];
};

enum NODE_TYPE {
	TYPE_INODE = 37,
	TYPE_DIRECT_NODE = 43,
	TYPE_INDIRECT_NODE = 53,
	TYPE_DOUBLE_INDIRECT_NODE = 67
};

struct hard_link_node {
	u32 nid;
	u32 links;
	struct hard_link_node *next;
};

enum seg_type {
	SEG_TYPE_DATA,
	SEG_TYPE_CUR_DATA,
	SEG_TYPE_NODE,
	SEG_TYPE_CUR_NODE,
	SEG_TYPE_MAX,
};

extern int fsck_chk_xattr_blk(struct f2fs_sb_info *sbi, u32 ino, u32 x_nid, u32 *blk_cnt);
extern int fsck_chk_orphan_node(struct f2fs_sb_info *sbi);

extern int fsck_chk_node_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		enum NODE_TYPE ntype,
		u32 *blk_cnt);

extern int fsck_chk_inode_blk(struct f2fs_sb_info *sbi,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt,
		struct node_info *ni);

extern int fsck_chk_dnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt,
		struct node_info *ni);

extern int fsck_chk_idnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt);

extern int fsck_chk_didnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt);

extern int fsck_chk_data_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 blk_addr,
		u32 *child_cnt,
		u32 *child_files,
		int last_blk,
		enum FILE_TYPE ftype,
		u32 parent_nid,
		u16 idx_in_node,
		u8 ver);

extern int fsck_chk_dentry_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 blk_addr,
		u32 *child_cnt,
		u32 *child_files,
		int last_blk);

extern void print_node_info(struct f2fs_node *node_block);
extern void print_inode_info(struct f2fs_inode *inode);
extern struct seg_entry *get_seg_entry(struct f2fs_sb_info *sbi, unsigned int segno);
extern int get_sum_block(struct f2fs_sb_info *sbi, unsigned int segno, struct f2fs_summary_block *sum_blk);
extern int get_sum_entry(struct f2fs_sb_info *sbi, u32 blk_addr, struct f2fs_summary *sum_entry);
extern int get_node_info(struct f2fs_sb_info *sbi, nid_t nid, struct node_info *ni);
extern void build_nat_area_bitmap(struct f2fs_sb_info *sbi);
extern int build_sit_area_bitmap(struct f2fs_sb_info *sbi);
extern int fsck_init(struct f2fs_sb_info *sbi);
extern int fsck_verify(struct f2fs_sb_info *sbi);
extern void fsck_free(struct f2fs_sb_info *sbi);
extern int f2fs_do_mount(struct f2fs_sb_info *sbi);
extern void f2fs_do_umount(struct f2fs_sb_info *sbi);

/* dump.c */
struct dump_option {
	nid_t nid;
	int start_sit;
	int end_sit;
	int start_ssa;
	int end_ssa;
	u32 blk_addr;
};

extern void sit_dump(struct f2fs_sb_info *sbi, int start_sit, int end_sit);
extern void ssa_dump(struct f2fs_sb_info *sbi, int start_ssa, int end_ssa);
extern int dump_node(struct f2fs_sb_info *sbi, nid_t nid);
extern int dump_inode_from_blkaddr(struct f2fs_sb_info *sbi, u32 blk_addr);

#endif /* _FSCK_H_ */
