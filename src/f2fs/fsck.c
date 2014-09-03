/**
 * fsck.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "fsck.h"

char *tree_mark;
int tree_mark_size = 256;

static int add_into_hard_link_list(struct f2fs_sb_info *sbi, u32 nid, u32 link_cnt)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct hard_link_node *node = NULL, *tmp = NULL, *prev = NULL;

	node = calloc(sizeof(struct hard_link_node), 1);
	ASSERT(node != NULL);

	node->nid = nid;
	node->links = link_cnt;
	node->next = NULL;

	if (fsck->hard_link_list_head == NULL) {
		fsck->hard_link_list_head = node;
		goto out;
	}

	tmp = fsck->hard_link_list_head;

	/* Find insertion position */
	while (tmp && (nid < tmp->nid)) {
		ASSERT(tmp->nid != nid);
		prev = tmp;
		tmp = tmp->next;
	}

	if (tmp == fsck->hard_link_list_head) {
		node->next = tmp;
		fsck->hard_link_list_head = node;
	} else {
		prev->next = node;
		node->next = tmp;
	}

out:
	DBG(2, "ino[0x%x] has hard links [0x%x]\n", nid, link_cnt);
	return 0;
}

static int find_and_dec_hard_link_list(struct f2fs_sb_info *sbi, u32 nid)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct hard_link_node *node = NULL, *prev = NULL;

	if (fsck->hard_link_list_head == NULL) {
		ASSERT(0);
		return -1;
	}

	node = fsck->hard_link_list_head;

	while (node && (nid < node->nid)) {
		prev = node;
		node = node->next;
	}

	if (node == NULL || (nid != node->nid)) {
		ASSERT(0);
		return -1;
	}

	/* Decrease link count */
	node->links = node->links - 1;

	/* if link count becomes one, remove the node */
	if (node->links == 1) {
		if (fsck->hard_link_list_head == node)
			fsck->hard_link_list_head = node->next;
		else
			prev->next = node->next;
		free(node);
	}

	return 0;

}

static int is_valid_ssa_node_blk(struct f2fs_sb_info *sbi, u32 nid, u32 blk_addr)
{
	int ret = 0;
	struct f2fs_summary sum_entry;

	ret = get_sum_entry(sbi, blk_addr, &sum_entry);
	ASSERT(ret >= 0);

	if (ret == SEG_TYPE_DATA || ret == SEG_TYPE_CUR_DATA) {
		ASSERT_MSG(0, "Summary footer is not a node segment summary\n");;
	} else if (ret == SEG_TYPE_NODE) {
		if (le32_to_cpu(sum_entry.nid) != nid) {
			DBG(0, "nid                       [0x%x]\n", nid);
			DBG(0, "target blk_addr           [0x%x]\n", blk_addr);
			DBG(0, "summary blk_addr          [0x%x]\n",
					GET_SUM_BLKADDR(sbi, GET_SEGNO(sbi, blk_addr)));
			DBG(0, "seg no / offset           [0x%x / 0x%x]\n",
					GET_SEGNO(sbi, blk_addr), OFFSET_IN_SEG(sbi, blk_addr));
			DBG(0, "summary_entry.nid         [0x%x]\n", le32_to_cpu(sum_entry.nid));
			DBG(0, "--> node block's nid      [0x%x]\n", nid);
			ASSERT_MSG(0, "Invalid node seg summary\n");
		}
	} else if (ret == SEG_TYPE_CUR_NODE) {
		/* current node segment has no ssa */
	} else {
		ASSERT_MSG(0, "Invalid return value of 'get_sum_entry'");
	}

	return 1;
}

static int is_valid_ssa_data_blk(struct f2fs_sb_info *sbi, u32 blk_addr,
		u32 parent_nid, u16 idx_in_node, u8 version)
{
	int ret = 0;
	struct f2fs_summary sum_entry;

	ret = get_sum_entry(sbi, blk_addr, &sum_entry);
	ASSERT(ret == SEG_TYPE_DATA || ret == SEG_TYPE_CUR_DATA);

	if (le32_to_cpu(sum_entry.nid) != parent_nid ||
			sum_entry.version != version ||
			le16_to_cpu(sum_entry.ofs_in_node) != idx_in_node) {

		DBG(0, "summary_entry.nid         [0x%x]\n", le32_to_cpu(sum_entry.nid));
		DBG(0, "summary_entry.version     [0x%x]\n", sum_entry.version);
		DBG(0, "summary_entry.ofs_in_node [0x%x]\n", le16_to_cpu(sum_entry.ofs_in_node));

		DBG(0, "parent nid                [0x%x]\n", parent_nid);
		DBG(0, "version from nat          [0x%x]\n", version);
		DBG(0, "idx in parent node        [0x%x]\n", idx_in_node);

		DBG(0, "Target data block addr    [0x%x]\n", blk_addr);
		ASSERT_MSG(0, "Invalid data seg summary\n");
	}

	return 1;
}

int fsck_chk_node_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		enum NODE_TYPE ntype,
		u32 *blk_cnt)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct node_info ni;
	struct f2fs_node *node_blk = NULL;
	int ret = 0;

	IS_VALID_NID(sbi, nid);

	if (ftype != F2FS_FT_ORPHAN ||
			f2fs_test_bit(nid, fsck->nat_area_bitmap) != 0x0)
		f2fs_clear_bit(nid, fsck->nat_area_bitmap);
	else
		ASSERT_MSG(0, "nid duplicated [0x%x]\n", nid);

	ret = get_node_info(sbi, nid, &ni);
	ASSERT(ret >= 0);

	/* Is it reserved block?
	 * if block addresss was 0xffff,ffff,ffff,ffff
	 * it means that block was already allocated, but not stored in disk
	 */
	if (ni.blk_addr == NEW_ADDR) {
		fsck->chk.valid_blk_cnt++;
		fsck->chk.valid_node_cnt++;
		if (ntype == TYPE_INODE)
			fsck->chk.valid_inode_cnt++;
		return 0;
	}

	IS_VALID_BLK_ADDR(sbi, ni.blk_addr);

	is_valid_ssa_node_blk(sbi, nid, ni.blk_addr);

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->sit_area_bitmap) == 0x0) {
		DBG(0, "SIT bitmap is 0x0. blk_addr[0x%x] %i\n", ni.blk_addr, ni.blk_addr);
		ASSERT(0);
	}else{
		DBG(0, "SIT bitmap is NOT 0x0. blk_addr[0x%x] %i\n", ni.blk_addr, ni.blk_addr);
		//ASSERT(0);
	}

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->main_area_bitmap) == 0x0) {
		DBG(0, "SIT and main bitmap is 0x0. blk_addr[0x%x] %i\n", ni.blk_addr, ni.blk_addr);
		fsck->chk.valid_blk_cnt++;
		fsck->chk.valid_node_cnt++;
	}else{
		DBG(0, "SIT and main bitmap is NOT 0x0. blk_addr[0x%x] %i\n", ni.blk_addr, ni.blk_addr);
	    
	}

	node_blk = (struct f2fs_node *)calloc(BLOCK_SZ, 1);
	ASSERT(node_blk != NULL);

	ret = dev_read_block(node_blk, ni.blk_addr);
	ASSERT(ret >= 0);

	ASSERT_MSG(nid == le32_to_cpu(node_blk->footer.nid),
			"nid[0x%x] blk_addr[0x%x] footer.nid[0x%x]\n",
			nid, ni.blk_addr, le32_to_cpu(node_blk->footer.nid));

	if (ntype == TYPE_INODE) {
		ret = fsck_chk_inode_blk(sbi,
				nid,
				ftype,
				node_blk,
				blk_cnt,
				&ni);
	} else {
		/* it's not inode */
		ASSERT(node_blk->footer.nid != node_blk->footer.ino);

		if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->main_area_bitmap) != 0) {
			DBG(0, "Duplicated node block. ino[0x%x][0x%x]\n", nid, ni.blk_addr);
			ASSERT(0);
		}
		f2fs_set_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->main_area_bitmap);

		switch (ntype) {
		case TYPE_DIRECT_NODE:
			ret = fsck_chk_dnode_blk(sbi,
					inode,
					nid,
					ftype,
					node_blk,
					blk_cnt,
					&ni);
			break;
		case TYPE_INDIRECT_NODE:
			ret = fsck_chk_idnode_blk(sbi,
					inode,
					nid,
					ftype,
					node_blk,
					blk_cnt);
			break;
		case TYPE_DOUBLE_INDIRECT_NODE:
			ret = fsck_chk_didnode_blk(sbi,
					inode,
					nid,
					ftype,
					node_blk,
					blk_cnt);
			break;
		default:
			ASSERT(0);
		}
	}
	ASSERT(ret >= 0);

	free(node_blk);
	return 0;
}

int fsck_chk_inode_blk(struct f2fs_sb_info *sbi,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt,
		struct node_info *ni)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	u32 child_cnt = 0, child_files = 0;
	enum NODE_TYPE ntype;
	u32 i_links = le32_to_cpu(node_blk->i.i_links);
	u64 i_blocks = le64_to_cpu(node_blk->i.i_blocks);
	int idx = 0;
	int ret = 0;

	ASSERT(node_blk->footer.nid == node_blk->footer.ino);
	ASSERT(le32_to_cpu(node_blk->footer.nid) == nid);

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni->blk_addr), fsck->main_area_bitmap) == 0x0)
		fsck->chk.valid_inode_cnt++;

	/* Orphan node. i_links should be 0 */
	if (ftype == F2FS_FT_ORPHAN) {
		ASSERT(i_links == 0);
	} else {
		ASSERT(i_links > 0);
	}

	if (ftype == F2FS_FT_DIR) {

		/* not included '.' & '..' */
		if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni->blk_addr), fsck->main_area_bitmap) != 0) {
			DBG(0, "Duplicated inode blk. ino[0x%x][0x%x]\n", nid, ni->blk_addr);
			ASSERT(0);
		}
		f2fs_set_bit(BLKOFF_FROM_MAIN(sbi, ni->blk_addr), fsck->main_area_bitmap);

	} else {

		if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni->blk_addr), fsck->main_area_bitmap) == 0x0) {
			f2fs_set_bit(BLKOFF_FROM_MAIN(sbi, ni->blk_addr), fsck->main_area_bitmap);
			if (i_links > 1) {
				/* First time. Create new hard link node */
				add_into_hard_link_list(sbi, nid, i_links);
				fsck->chk.multi_hard_link_files++;
			}
		} else {
			if (i_links <= 1) {
				DBG(0, "Error. Node ID [0x%x]."
						" There are one more hard links."
						" But i_links is [0x%x]\n",
						nid, i_links);
				ASSERT(0);
			}

			DBG(3, "ino[0x%x] has hard links [0x%x]\n", nid, i_links);
			ret = find_and_dec_hard_link_list(sbi, nid);
			ASSERT(ret >= 0);

			/* No need to go deep into the node */
			goto out;
		}
	}

	fsck_chk_xattr_blk(sbi, nid, le32_to_cpu(node_blk->i.i_xattr_nid), blk_cnt);

	if (ftype == F2FS_FT_CHRDEV || ftype == F2FS_FT_BLKDEV ||
			ftype == F2FS_FT_FIFO || ftype == F2FS_FT_SOCK)
		goto check;
	if((node_blk->i.i_inline & F2FS_INLINE_DATA)){
		DBG(3, "ino[0x%x] has inline data!\n", nid);
		goto check;
	}

	/* check data blocks in inode */
	for (idx = 0; idx < ADDRS_PER_INODE(&node_blk->i); idx++) {
		if (le32_to_cpu(node_blk->i.i_addr[idx]) != 0) {
			*blk_cnt = *blk_cnt + 1;
			ret = fsck_chk_data_blk(sbi,
					&node_blk->i,
					le32_to_cpu(node_blk->i.i_addr[idx]),
					&child_cnt,
					&child_files,
					(i_blocks == *blk_cnt),
					ftype,
					nid,
					idx,
					ni->version);
			ASSERT(ret >= 0);
		}
	}

	/* check node blocks in inode */
	for (idx = 0; idx < 5; idx++) {
		if (idx == 0 || idx == 1)
			ntype = TYPE_DIRECT_NODE;
		else if (idx == 2 || idx == 3)
			ntype = TYPE_INDIRECT_NODE;
		else if (idx == 4)
			ntype = TYPE_DOUBLE_INDIRECT_NODE;
		else
			ASSERT(0);

		if (le32_to_cpu(node_blk->i.i_nid[idx]) != 0) {
			*blk_cnt = *blk_cnt + 1;
			ret = fsck_chk_node_blk(sbi,
					&node_blk->i,
					le32_to_cpu(node_blk->i.i_nid[idx]),
					ftype,
					ntype,
					blk_cnt);
			ASSERT(ret >= 0);
		}
	}
check:
	if (ftype == F2FS_FT_DIR)
		DBG(1, "Directory Inode: ino: %x name: %s depth: %d child files: %d\n\n",
				le32_to_cpu(node_blk->footer.ino), node_blk->i.i_name,
				le32_to_cpu(node_blk->i.i_current_depth), child_files);
	if (ftype == F2FS_FT_ORPHAN)
		DBG(1, "Orphan Inode: ino: %x name: %s i_blocks: %lu\n\n",
				le32_to_cpu(node_blk->footer.ino), node_blk->i.i_name,
				i_blocks);
	if ((ftype == F2FS_FT_DIR && i_links != child_cnt) ||
			(i_blocks != *blk_cnt)) {
		print_node_info(node_blk);
		DBG(1, "blk   cnt [0x%x]\n", *blk_cnt);
		DBG(1, "child cnt [0x%x]\n", child_cnt);
	}

	ASSERT(i_blocks == *blk_cnt);
	if (ftype == F2FS_FT_DIR)
		ASSERT(i_links == child_cnt);
out:
	return 0;
}

int fsck_chk_dnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt,
		struct node_info *ni)
{
	int idx;
	u32 child_cnt = 0, child_files = 0;

	for (idx = 0; idx < ADDRS_PER_BLOCK; idx++) {
		if (le32_to_cpu(node_blk->dn.addr[idx]) == 0x0)
			continue;
		*blk_cnt = *blk_cnt + 1;
		fsck_chk_data_blk(sbi,
				inode,
				le32_to_cpu(node_blk->dn.addr[idx]),
				&child_cnt,
				&child_files,
				le64_to_cpu(inode->i_blocks) == *blk_cnt,
				ftype,
				nid,
				idx,
				ni->version);
	}

	return 0;
}

int fsck_chk_idnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt)
{
	int i = 0;

	for (i = 0 ; i < NIDS_PER_BLOCK; i++) {
		if (le32_to_cpu(node_blk->in.nid[i]) == 0x0)
			continue;
		*blk_cnt = *blk_cnt + 1;
		fsck_chk_node_blk(sbi,
				inode,
				le32_to_cpu(node_blk->in.nid[i]),
				ftype,
				TYPE_DIRECT_NODE,
				blk_cnt);
	}

	return 0;
}

int fsck_chk_didnode_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 nid,
		enum FILE_TYPE ftype,
		struct f2fs_node *node_blk,
		u32 *blk_cnt)
{
	int i = 0;

	for (i = 0; i < NIDS_PER_BLOCK; i++) {
		if (le32_to_cpu(node_blk->in.nid[i]) == 0x0)
			continue;
		*blk_cnt = *blk_cnt + 1;
		fsck_chk_node_blk(sbi,
				inode,
				le32_to_cpu(node_blk->in.nid[i]),
				ftype,
				TYPE_INDIRECT_NODE,
				blk_cnt);
	}

	return 0;
}

static void print_dentry(__u32 depth, __u8 *name,
		struct f2fs_dentry_block *de_blk, int idx, int last_blk)
{
	int last_de = 0;
	int next_idx = 0;
	int name_len;
	int i;
	int bit_offset;

	if (config.dbg_lv != -1)
		return;

	name_len = le16_to_cpu(de_blk->dentry[idx].name_len);
	next_idx = idx + (name_len + F2FS_SLOT_LEN - 1) / F2FS_SLOT_LEN;

	bit_offset = find_next_bit((unsigned long *)de_blk->dentry_bitmap,
			NR_DENTRY_IN_BLOCK, next_idx);
	if (bit_offset >= NR_DENTRY_IN_BLOCK && last_blk)
		last_de = 1;

	if (tree_mark_size <= depth) {
		tree_mark_size *= 2;
		tree_mark = realloc(tree_mark, tree_mark_size);
	}

	if (last_de)
		tree_mark[depth] = '`';
	else
		tree_mark[depth] = '|';

	if (tree_mark[depth - 1] == '`')
		tree_mark[depth - 1] = ' ';


	for (i = 1; i < depth; i++)
		printf("%c   ", tree_mark[i]);
	printf("%c-- %s\n", last_de ? '`' : '|', name);
}

int fsck_chk_dentry_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 blk_addr,
		u32 *child_cnt,
		u32 *child_files,
		int last_blk)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	int i;
	int ret = 0;
	int dentries = 0;
	u8 *name;
	u32 hash_code;
	u32 blk_cnt;
	u16 name_len;;

	enum FILE_TYPE ftype;
	struct f2fs_dentry_block *de_blk;

	de_blk = (struct f2fs_dentry_block *)calloc(BLOCK_SZ, 1);
	ASSERT(de_blk != NULL);

	ret = dev_read_block(de_blk, blk_addr);
	ASSERT(ret >= 0);

	fsck->dentry_depth++;

	for (i = 0; i < NR_DENTRY_IN_BLOCK;) {
		if (test_bit(i, (unsigned long *)de_blk->dentry_bitmap) == 0x0) {
			i++;
			continue;
		}

		name_len = le32_to_cpu(de_blk->dentry[i].name_len);
		name = calloc(name_len + 1, 1);
		memcpy(name, de_blk->filename[i], name_len);

		hash_code = f2fs_dentry_hash((const char *)name, name_len);
		ASSERT(le32_to_cpu(de_blk->dentry[i].hash_code) == hash_code);

		ftype = de_blk->dentry[i].file_type;

		/* Becareful. 'dentry.file_type' is not imode. */
		if (ftype == F2FS_FT_DIR) {
			*child_cnt = *child_cnt + 1;
			if ((name[0] == '.' && name[1] == '.' && name_len == 2) ||
					(name[0] == '.' && name_len == 1)) {
				i++;
				free(name);
				continue;
			}
		}

		DBG(2, "[%3u] - no[0x%x] name[%s] len[0x%x] ino[0x%x] type[0x%x]\n",
				fsck->dentry_depth, i, name, name_len,
				le32_to_cpu(de_blk->dentry[i].ino),
				de_blk->dentry[i].file_type);

		print_dentry(fsck->dentry_depth, name, de_blk, i, last_blk);

		blk_cnt = 1;
		ret = fsck_chk_node_blk(sbi,
				NULL,
				le32_to_cpu(de_blk->dentry[i].ino),
				ftype,
				TYPE_INODE,
				&blk_cnt);

		ASSERT(ret >= 0);

		i += (name_len + F2FS_SLOT_LEN - 1) / F2FS_SLOT_LEN;
		dentries++;
		*child_files = *child_files + 1;
		free(name);
	}

	DBG(1, "[%3d] Dentry Block [0x%x] Done : dentries:%d in %d slots (len:%d)\n\n",
			fsck->dentry_depth, blk_addr, dentries, NR_DENTRY_IN_BLOCK, F2FS_NAME_LEN);
	fsck->dentry_depth--;

	free(de_blk);
	return 0;
}

int fsck_chk_data_blk(struct f2fs_sb_info *sbi,
		struct f2fs_inode *inode,
		u32 blk_addr,
		u32 *child_cnt,
		u32 *child_files,
		int last_blk,
		enum FILE_TYPE ftype,
		u32 parent_nid,
		u16 idx_in_node,
		u8 ver)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);

	/* Is it reserved block? */
	if (blk_addr == NEW_ADDR) {
		fsck->chk.valid_blk_cnt++;
		return 0;
	}

	IS_VALID_BLK_ADDR(sbi, blk_addr);

	is_valid_ssa_data_blk(sbi, blk_addr, parent_nid, idx_in_node, ver);

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, blk_addr), fsck->sit_area_bitmap) == 0x0) {
		ASSERT_MSG(0, "SIT bitmap is 0x0. blk_addr[0x%x]\n", blk_addr);
	}

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, blk_addr), fsck->main_area_bitmap) != 0) {
		ASSERT_MSG(0, "Duplicated data block. pnid[0x%x] idx[0x%x] blk_addr[0x%x]\n",
				parent_nid, idx_in_node, blk_addr);
	}
	f2fs_set_bit(BLKOFF_FROM_MAIN(sbi, blk_addr), fsck->main_area_bitmap);

	fsck->chk.valid_blk_cnt++;

	if (ftype == F2FS_FT_DIR) {
		fsck_chk_dentry_blk(sbi,
				inode,
				blk_addr,
				child_cnt,
				child_files,
				last_blk);
	}

	return 0;
}

int fsck_chk_orphan_node(struct f2fs_sb_info *sbi)
{
	int ret = 0;
	u32 blk_cnt = 0;

	block_t start_blk, orphan_blkaddr, i, j;
	struct f2fs_orphan_block *orphan_blk;

	if (!is_set_ckpt_flags(F2FS_CKPT(sbi), CP_ORPHAN_PRESENT_FLAG))
		return 0;

	start_blk = __start_cp_addr(sbi) + 1;
	orphan_blkaddr = __start_sum_addr(sbi) - 1;

	orphan_blk = calloc(BLOCK_SZ, 1);

	for (i = 0; i < orphan_blkaddr; i++) {
		dev_read_block(orphan_blk, start_blk + i);

		for (j = 0; j < le32_to_cpu(orphan_blk->entry_count); j++) {
			nid_t ino = le32_to_cpu(orphan_blk->ino[j]);
			DBG(1, "[%3d] ino [0x%x]\n", i, ino);
			blk_cnt = 1;
			ret = fsck_chk_node_blk(sbi,
					NULL,
					ino,
					F2FS_FT_ORPHAN,
					TYPE_INODE,
					&blk_cnt);
			ASSERT(ret >= 0);
		}
		memset(orphan_blk, 0, BLOCK_SZ);
	}
	free(orphan_blk);


	return 0;
}

int fsck_chk_xattr_blk(struct f2fs_sb_info *sbi, u32 ino, u32 x_nid, u32 *blk_cnt)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct node_info ni;

	if (x_nid == 0x0)
		return 0;

	if (f2fs_test_bit(x_nid, fsck->nat_area_bitmap) != 0x0) {
		f2fs_clear_bit(x_nid, fsck->nat_area_bitmap);
	} else {
		ASSERT_MSG(0, "xattr_nid duplicated [0x%x]\n", x_nid);
	}

	*blk_cnt = *blk_cnt + 1;
	fsck->chk.valid_blk_cnt++;
	fsck->chk.valid_node_cnt++;

	ASSERT(get_node_info(sbi, x_nid, &ni) >= 0);

	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->main_area_bitmap) != 0) {
		ASSERT_MSG(0, "Duplicated node block for x_attr. "
				"x_nid[0x%x] block addr[0x%x]\n",
				x_nid, ni.blk_addr);
	}
	f2fs_set_bit(BLKOFF_FROM_MAIN(sbi, ni.blk_addr), fsck->main_area_bitmap);

	DBG(2, "ino[0x%x] x_nid[0x%x]\n", ino, x_nid);
	return 0;
}

int fsck_init(struct f2fs_sb_info *sbi)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct f2fs_sm_info *sm_i = SM_I(sbi);

	/*
	 * We build three bitmap for main/sit/nat so that may check consistency of filesystem.
	 * 1. main_area_bitmap will be used to check whether all blocks of main area is used or not.
	 * 2. nat_area_bitmap has bitmap information of used nid in NAT.
	 * 3. sit_area_bitmap has bitmap information of used main block.
	 * At Last sequence, we compare main_area_bitmap with sit_area_bitmap.
	 */
	fsck->nr_main_blks = sm_i->main_segments << sbi->log_blocks_per_seg;
	fsck->main_area_bitmap_sz = (fsck->nr_main_blks + 7) / 8;
	fsck->main_area_bitmap = calloc(fsck->main_area_bitmap_sz, 1);
	ASSERT(fsck->main_area_bitmap != NULL);

	build_nat_area_bitmap(sbi);

	build_sit_area_bitmap(sbi);

	tree_mark = calloc(tree_mark_size, 1);
	return 0;
}

int fsck_verify(struct f2fs_sb_info *sbi)
{
	int i = 0;
	int ret = 0;
	u32 nr_unref_nid = 0;
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct hard_link_node *node = NULL;

	printf("\n");

	for (i = 0; i < fsck->nr_nat_entries; i++) {
		if (f2fs_test_bit(i, fsck->nat_area_bitmap) != 0) {
			printf("NID[0x%x] is unreachable\n", i);
			nr_unref_nid++;
		}
	}

	if (fsck->hard_link_list_head != NULL) {
		node = fsck->hard_link_list_head;
		while (node) {
			printf("NID[0x%x] has [0x%x] more unreachable links\n",
					node->nid, node->links);
			node = node->next;
		}
	}

	printf("[FSCK] Unreachable nat entries                       ");
	if (nr_unref_nid == 0x0) {
		printf(" [Ok..] [0x%x]\n", nr_unref_nid);
	} else {
		printf(" [Fail] [0x%x]\n", nr_unref_nid);
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] SIT valid block bitmap checking                ");
	if (memcmp(fsck->sit_area_bitmap, fsck->main_area_bitmap, fsck->sit_area_bitmap_sz) == 0x0) {
		printf("[Ok..]\n");
	} else {
		printf("[Fail]\n");
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] Hard link checking for regular file           ");
	if (fsck->hard_link_list_head == NULL) {
		printf(" [Ok..] [0x%x]\n", fsck->chk.multi_hard_link_files);
	} else {
		printf(" [Fail] [0x%x]\n", fsck->chk.multi_hard_link_files);
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] valid_block_count matching with CP            ");
	if (sbi->total_valid_block_count == fsck->chk.valid_blk_cnt) {
		printf(" [Ok..] [0x%lx]\n", fsck->chk.valid_blk_cnt);
	} else {
		printf(" [Fail] [0x%lx]\n", fsck->chk.valid_blk_cnt);
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] valid_node_count matcing with CP (de lookup)  ");
	if (sbi->total_valid_node_count == fsck->chk.valid_node_cnt) {
		printf(" [Ok..] [0x%x]\n", fsck->chk.valid_node_cnt);
	} else {
		printf(" [Fail] [0x%x]\n", fsck->chk.valid_node_cnt);
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] valid_node_count matcing with CP (nat lookup) ");
	if (sbi->total_valid_node_count == fsck->chk.valid_nat_entry_cnt) {
		printf(" [Ok..] [0x%x]\n", fsck->chk.valid_nat_entry_cnt);
	} else {
		printf(" [Fail] [0x%x]\n", fsck->chk.valid_nat_entry_cnt);
		ret = EXIT_ERR_CODE;
	}

	printf("[FSCK] valid_inode_count matched with CP             ");
	if (sbi->total_valid_inode_count == fsck->chk.valid_inode_cnt) {
		printf(" [Ok..] [0x%x]\n", fsck->chk.valid_inode_cnt);
	} else {
		printf(" [Fail] [0x%x]\n", fsck->chk.valid_inode_cnt);
		ret = EXIT_ERR_CODE;
	}

	return ret;
}

void fsck_free(struct f2fs_sb_info *sbi)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	if (fsck->main_area_bitmap)
		free(fsck->main_area_bitmap);

	if (fsck->nat_area_bitmap)
		free(fsck->nat_area_bitmap);

	if (fsck->sit_area_bitmap)
		free(fsck->sit_area_bitmap);

	if (tree_mark)
		free(tree_mark);
}
