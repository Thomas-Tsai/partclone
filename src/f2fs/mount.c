/**
 * mount.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "fsck.h"

void print_inode_info(struct f2fs_inode *inode)
{
	int i = 0;
	int namelen = le32_to_cpu(inode->i_namelen);

	DISP_u32(inode, i_mode);
	DISP_u32(inode, i_uid);
	DISP_u32(inode, i_gid);
	DISP_u32(inode, i_links);
	DISP_u64(inode, i_size);
	DISP_u64(inode, i_blocks);

	DISP_u64(inode, i_atime);
	DISP_u32(inode, i_atime_nsec);
	DISP_u64(inode, i_ctime);
	DISP_u32(inode, i_ctime_nsec);
	DISP_u64(inode, i_mtime);
	DISP_u32(inode, i_mtime_nsec);

	DISP_u32(inode, i_generation);
	DISP_u32(inode, i_current_depth);
	DISP_u32(inode, i_xattr_nid);
	DISP_u32(inode, i_flags);
	DISP_u32(inode, i_pino);

	if (namelen) {
		DISP_u32(inode, i_namelen);
		inode->i_name[namelen] = '\0';
		DISP_utf(inode, i_name);
	}

	printf("i_ext: fofs:%x blkaddr:%x len:%x\n",
			inode->i_ext.fofs,
			inode->i_ext.blk_addr,
			inode->i_ext.len);

	DISP_u32(inode, i_addr[0]);	/* Pointers to data blocks */
	DISP_u32(inode, i_addr[1]);	/* Pointers to data blocks */
	DISP_u32(inode, i_addr[2]);	/* Pointers to data blocks */
	DISP_u32(inode, i_addr[3]);	/* Pointers to data blocks */

	for (i = 4; i < ADDRS_PER_INODE(inode); i++) {
		if (inode->i_addr[i] != 0x0) {
			printf("i_addr[0x%x] points data block\r\t\t\t\t[0x%4x]\n",
					i, inode->i_addr[i]);
			break;
		}
	}

	DISP_u32(inode, i_nid[0]);	/* direct */
	DISP_u32(inode, i_nid[1]);	/* direct */
	DISP_u32(inode, i_nid[2]);	/* indirect */
	DISP_u32(inode, i_nid[3]);	/* indirect */
	DISP_u32(inode, i_nid[4]);	/* double indirect */

	printf("\n");
}

void print_node_info(struct f2fs_node *node_block)
{
	nid_t ino = le32_to_cpu(node_block->footer.ino);
	nid_t nid = le32_to_cpu(node_block->footer.nid);
	/* Is this inode? */
	if (ino == nid) {
		DBG(0, "Node ID [0x%x:%u] is inode\n", nid, nid);
		print_inode_info(&node_block->i);
	} else {
		int i;
		u32 *dump_blk = (u32 *)node_block;
		DBG(0, "Node ID [0x%x:%u] is direct node or indirect node.\n", nid, nid);
		for (i = 0; i <= 10; i++)
			MSG(0, "[%d]\t\t\t[0x%8x : %d]\n", i, dump_blk[i], dump_blk[i]);
	}
}

void print_raw_sb_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);

	if (!config.dbg_lv)
		return;

	printf("\n");
	printf("+--------------------------------------------------------+\n");
	printf("| Super block                                            |\n");
	printf("+--------------------------------------------------------+\n");

	DISP_u32(sb, magic);
	DISP_u32(sb, major_ver);
	DISP_u32(sb, minor_ver);
	DISP_u32(sb, log_sectorsize);
	DISP_u32(sb, log_sectors_per_block);

	DISP_u32(sb, log_blocksize);
	DISP_u32(sb, log_blocks_per_seg);
	DISP_u32(sb, segs_per_sec);
	DISP_u32(sb, secs_per_zone);
	DISP_u32(sb, checksum_offset);
	DISP_u64(sb, block_count);

	DISP_u32(sb, section_count);
	DISP_u32(sb, segment_count);
	DISP_u32(sb, segment_count_ckpt);
	DISP_u32(sb, segment_count_sit);
	DISP_u32(sb, segment_count_nat);

	DISP_u32(sb, segment_count_ssa);
	DISP_u32(sb, segment_count_main);
	DISP_u32(sb, segment0_blkaddr);

	DISP_u32(sb, cp_blkaddr);
	DISP_u32(sb, sit_blkaddr);
	DISP_u32(sb, nat_blkaddr);
	DISP_u32(sb, ssa_blkaddr);
	DISP_u32(sb, main_blkaddr);

	DISP_u32(sb, root_ino);
	DISP_u32(sb, node_ino);
	DISP_u32(sb, meta_ino);
	printf("\n");
}

void print_ckpt_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_checkpoint *cp = F2FS_CKPT(sbi);

	if (!config.dbg_lv)
		return;

	printf("\n");
	printf("+--------------------------------------------------------+\n");
	printf("| Checkpoint                                             |\n");
	printf("+--------------------------------------------------------+\n");

	DISP_u64(cp, checkpoint_ver);
	DISP_u64(cp, user_block_count);
	DISP_u64(cp, valid_block_count);
	DISP_u32(cp, rsvd_segment_count);
	DISP_u32(cp, overprov_segment_count);
	DISP_u32(cp, free_segment_count);

	DISP_u32(cp, alloc_type[CURSEG_HOT_NODE]);
	DISP_u32(cp, alloc_type[CURSEG_WARM_NODE]);
	DISP_u32(cp, alloc_type[CURSEG_COLD_NODE]);
	DISP_u32(cp, cur_node_segno[0]);
	DISP_u32(cp, cur_node_segno[1]);
	DISP_u32(cp, cur_node_segno[2]);

	DISP_u32(cp, cur_node_blkoff[0]);
	DISP_u32(cp, cur_node_blkoff[1]);
	DISP_u32(cp, cur_node_blkoff[2]);


	DISP_u32(cp, alloc_type[CURSEG_HOT_DATA]);
	DISP_u32(cp, alloc_type[CURSEG_WARM_DATA]);
	DISP_u32(cp, alloc_type[CURSEG_COLD_DATA]);
	DISP_u32(cp, cur_data_segno[0]);
	DISP_u32(cp, cur_data_segno[1]);
	DISP_u32(cp, cur_data_segno[2]);

	DISP_u32(cp, cur_data_blkoff[0]);
	DISP_u32(cp, cur_data_blkoff[1]);
	DISP_u32(cp, cur_data_blkoff[2]);

	DISP_u32(cp, ckpt_flags);
	DISP_u32(cp, cp_pack_total_block_count);
	DISP_u32(cp, cp_pack_start_sum);
	DISP_u32(cp, valid_node_count);
	DISP_u32(cp, valid_inode_count);
	DISP_u32(cp, next_free_nid);
	DISP_u32(cp, sit_ver_bitmap_bytesize);
	DISP_u32(cp, nat_ver_bitmap_bytesize);
	DISP_u32(cp, checksum_offset);
	DISP_u64(cp, elapsed_time);

	DISP_u32(cp, sit_nat_version_bitmap[0]);
	printf("\n\n");
}

int sanity_check_raw_super(struct f2fs_super_block *raw_super)
{
	unsigned int blocksize;

	if (F2FS_SUPER_MAGIC != le32_to_cpu(raw_super->magic)) {
		return -1;
	}

	if (F2FS_BLKSIZE != PAGE_CACHE_SIZE) {
		return -1;
	}

	blocksize = 1 << le32_to_cpu(raw_super->log_blocksize);
	if (F2FS_BLKSIZE != blocksize) {
		return -1;
	}

	if (F2FS_LOG_SECTOR_SIZE != le32_to_cpu(raw_super->log_sectorsize)) {
		return -1;
	}

	if (F2FS_LOG_SECTORS_PER_BLOCK != le32_to_cpu(raw_super->log_sectors_per_block)) {
		return -1;
	}

	return 0;
}

int validate_super_block(struct f2fs_sb_info *sbi, int block)
{
	u64 offset = (block + 1) * F2FS_SUPER_OFFSET;
	sbi->raw_super = malloc(sizeof(struct f2fs_super_block));

	if (dev_read(sbi->raw_super, offset, sizeof(struct f2fs_super_block)))
		return -1;

	if (!sanity_check_raw_super(sbi->raw_super))
		return 0;

	free(sbi->raw_super);
	MSG(0, "\tCan't find a valid F2FS filesystem in %d superblock\n", block);

	return -EINVAL;
}

int init_sb_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_super = sbi->raw_super;

	sbi->log_sectors_per_block =
		le32_to_cpu(raw_super->log_sectors_per_block);
	sbi->log_blocksize = le32_to_cpu(raw_super->log_blocksize);
	sbi->blocksize = 1 << sbi->log_blocksize;
	sbi->log_blocks_per_seg = le32_to_cpu(raw_super->log_blocks_per_seg);
	sbi->blocks_per_seg = 1 << sbi->log_blocks_per_seg;
	sbi->segs_per_sec = le32_to_cpu(raw_super->segs_per_sec);
	sbi->secs_per_zone = le32_to_cpu(raw_super->secs_per_zone);
	sbi->total_sections = le32_to_cpu(raw_super->section_count);
	sbi->total_node_count =
		(le32_to_cpu(raw_super->segment_count_nat) / 2)
		* sbi->blocks_per_seg * NAT_ENTRY_PER_BLOCK;
	sbi->root_ino_num = le32_to_cpu(raw_super->root_ino);
	sbi->node_ino_num = le32_to_cpu(raw_super->node_ino);
	sbi->meta_ino_num = le32_to_cpu(raw_super->meta_ino);
	sbi->cur_victim_sec = NULL_SEGNO;
	return 0;
}

void *validate_checkpoint(struct f2fs_sb_info *sbi, block_t cp_addr, unsigned long long *version)
{
	void *cp_page_1, *cp_page_2;
	struct f2fs_checkpoint *cp_block;
	unsigned long blk_size = sbi->blocksize;
	unsigned long long cur_version = 0, pre_version = 0;
	unsigned int crc = 0;
	size_t crc_offset;

	/* Read the 1st cp block in this CP pack */
	cp_page_1 = malloc(PAGE_SIZE);
	if (dev_read_block(cp_page_1, cp_addr) < 0)
		return NULL;

	cp_block = (struct f2fs_checkpoint *)cp_page_1;
	crc_offset = le32_to_cpu(cp_block->checksum_offset);
	if (crc_offset >= blk_size)
		goto invalid_cp1;

	crc = *(unsigned int *)((unsigned char *)cp_block + crc_offset);
	if (f2fs_crc_valid(crc, cp_block, crc_offset))
		goto invalid_cp1;

	pre_version = le64_to_cpu(cp_block->checkpoint_ver);

	/* Read the 2nd cp block in this CP pack */
	cp_page_2 = malloc(PAGE_SIZE);
	cp_addr += le32_to_cpu(cp_block->cp_pack_total_block_count) - 1;
	if (dev_read_block(cp_page_2, cp_addr) < 0)
		goto invalid_cp2;

	cp_block = (struct f2fs_checkpoint *)cp_page_2;
	crc_offset = le32_to_cpu(cp_block->checksum_offset);
	if (crc_offset >= blk_size)
		goto invalid_cp2;

	crc = *(unsigned int *)((unsigned char *)cp_block + crc_offset);
	if (f2fs_crc_valid(crc, cp_block, crc_offset))
		goto invalid_cp1;

	cur_version = le64_to_cpu(cp_block->checkpoint_ver);

	if (cur_version == pre_version) {
		*version = cur_version;
		free(cp_page_2);
		return cp_page_1;
	}

invalid_cp2:
	free(cp_page_2);
invalid_cp1:
	free(cp_page_1);
	return NULL;
}

int get_valid_checkpoint(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_sb = sbi->raw_super;
	void *cp1, *cp2, *cur_page;
	unsigned long blk_size = sbi->blocksize;
	unsigned long long cp1_version = 0, cp2_version = 0;
	unsigned long long cp_start_blk_no;

	sbi->ckpt = malloc(blk_size);
	if (!sbi->ckpt)
		return -ENOMEM;
	/*
	 * Finding out valid cp block involves read both
	 * sets( cp pack1 and cp pack 2)
	 */
	cp_start_blk_no = le32_to_cpu(raw_sb->cp_blkaddr);
	cp1 = validate_checkpoint(sbi, cp_start_blk_no, &cp1_version);

	/* The second checkpoint pack should start at the next segment */
	cp_start_blk_no += 1 << le32_to_cpu(raw_sb->log_blocks_per_seg);
	cp2 = validate_checkpoint(sbi, cp_start_blk_no, &cp2_version);

	if (cp1 && cp2) {
		if (ver_after(cp2_version, cp1_version))
			cur_page = cp2;
		else
			cur_page = cp1;
	} else if (cp1) {
		cur_page = cp1;
	} else if (cp2) {
		cur_page = cp2;
	} else {
		free(cp1);
		free(cp2);
		goto fail_no_cp;
	}

	memcpy(sbi->ckpt, cur_page, blk_size);

	free(cp1);
	free(cp2);
	return 0;

fail_no_cp:
	free(sbi->ckpt);
	return -EINVAL;
}

int sanity_check_ckpt(struct f2fs_sb_info *sbi)
{
	unsigned int total, fsmeta;
	struct f2fs_super_block *raw_super = F2FS_RAW_SUPER(sbi);
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);

	total = le32_to_cpu(raw_super->segment_count);
	fsmeta = le32_to_cpu(raw_super->segment_count_ckpt);
	fsmeta += le32_to_cpu(raw_super->segment_count_sit);
	fsmeta += le32_to_cpu(raw_super->segment_count_nat);
	fsmeta += le32_to_cpu(ckpt->rsvd_segment_count);
	fsmeta += le32_to_cpu(raw_super->segment_count_ssa);

	if (fsmeta >= total)
		return 1;

	return 0;
}

int init_node_manager(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *sb_raw = F2FS_RAW_SUPER(sbi);
	struct f2fs_nm_info *nm_i = NM_I(sbi);
	unsigned char *version_bitmap;
	unsigned int nat_segs, nat_blocks;

	nm_i->nat_blkaddr = le32_to_cpu(sb_raw->nat_blkaddr);

	/* segment_count_nat includes pair segment so divide to 2. */
	nat_segs = le32_to_cpu(sb_raw->segment_count_nat) >> 1;
	nat_blocks = nat_segs << le32_to_cpu(sb_raw->log_blocks_per_seg);
	nm_i->max_nid = NAT_ENTRY_PER_BLOCK * nat_blocks;
	nm_i->fcnt = 0;
	nm_i->nat_cnt = 0;
	nm_i->init_scan_nid = le32_to_cpu(sbi->ckpt->next_free_nid);
	nm_i->next_scan_nid = le32_to_cpu(sbi->ckpt->next_free_nid);

	nm_i->bitmap_size = __bitmap_size(sbi, NAT_BITMAP);

	nm_i->nat_bitmap = malloc(nm_i->bitmap_size);
	if (!nm_i->nat_bitmap)
		return -ENOMEM;
	version_bitmap = __bitmap_ptr(sbi, NAT_BITMAP);
	if (!version_bitmap)
		return -EFAULT;

	/* copy version bitmap */
	memcpy(nm_i->nat_bitmap, version_bitmap, nm_i->bitmap_size);
	return 0;
}

int build_node_manager(struct f2fs_sb_info *sbi)
{
	int err;
	sbi->nm_info = malloc(sizeof(struct f2fs_nm_info));
	if (!sbi->nm_info)
		return -ENOMEM;

	err = init_node_manager(sbi);
	if (err)
		return err;

	return 0;
}

int build_sit_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_sb = F2FS_RAW_SUPER(sbi);
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct sit_info *sit_i;
	unsigned int sit_segs, start;
	char *src_bitmap, *dst_bitmap;
	unsigned int bitmap_size;

	sit_i = malloc(sizeof(struct sit_info));
	if (!sit_i)
		return -ENOMEM;

	SM_I(sbi)->sit_info = sit_i;

	sit_i->sentries = calloc(TOTAL_SEGS(sbi) * sizeof(struct seg_entry), 1);

	for (start = 0; start < TOTAL_SEGS(sbi); start++) {
		sit_i->sentries[start].cur_valid_map
			= calloc(SIT_VBLOCK_MAP_SIZE, 1);
		sit_i->sentries[start].ckpt_valid_map
			= calloc(SIT_VBLOCK_MAP_SIZE, 1);
		if (!sit_i->sentries[start].cur_valid_map
				|| !sit_i->sentries[start].ckpt_valid_map)
			return -ENOMEM;
	}

	sit_segs = le32_to_cpu(raw_sb->segment_count_sit) >> 1;
	bitmap_size = __bitmap_size(sbi, SIT_BITMAP);
	src_bitmap = __bitmap_ptr(sbi, SIT_BITMAP);

	dst_bitmap = malloc(bitmap_size);
	memcpy(dst_bitmap, src_bitmap, bitmap_size);

	sit_i->sit_base_addr = le32_to_cpu(raw_sb->sit_blkaddr);
	sit_i->sit_blocks = sit_segs << sbi->log_blocks_per_seg;
	sit_i->written_valid_blocks = le64_to_cpu(ckpt->valid_block_count);
	sit_i->sit_bitmap = dst_bitmap;
	sit_i->bitmap_size = bitmap_size;
	sit_i->dirty_sentries = 0;
	sit_i->sents_per_block = SIT_ENTRY_PER_BLOCK;
	sit_i->elapsed_time = le64_to_cpu(ckpt->elapsed_time);
	return 0;
}

void reset_curseg(struct f2fs_sb_info *sbi, int type, int modified)
{
	struct curseg_info *curseg = CURSEG_I(sbi, type);

	curseg->segno = curseg->next_segno;
	curseg->zone = GET_ZONENO_FROM_SEGNO(sbi, curseg->segno);
	curseg->next_blkoff = 0;
	curseg->next_segno = NULL_SEGNO;

}

int read_compacted_summaries(struct f2fs_sb_info *sbi)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct curseg_info *curseg;
	block_t start;
	char *kaddr;
	unsigned int i, j, offset;

	start = start_sum_block(sbi);

	kaddr = (char *)malloc(PAGE_SIZE);
	dev_read_block(kaddr, start++);

	curseg = CURSEG_I(sbi, CURSEG_HOT_DATA);
	memcpy(&curseg->sum_blk->n_nats, kaddr, SUM_JOURNAL_SIZE);

	curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
	memcpy(&curseg->sum_blk->n_sits, kaddr + SUM_JOURNAL_SIZE, SUM_JOURNAL_SIZE);

	offset = 2 * SUM_JOURNAL_SIZE;
	for (i = CURSEG_HOT_DATA; i <= CURSEG_COLD_DATA; i++) {
		unsigned short blk_off;
		unsigned int segno;

		curseg = CURSEG_I(sbi, i);
		segno = le32_to_cpu(ckpt->cur_data_segno[i]);
		blk_off = le16_to_cpu(ckpt->cur_data_blkoff[i]);
		curseg->next_segno = segno;
		reset_curseg(sbi, i, 0);
		curseg->alloc_type = ckpt->alloc_type[i];
		curseg->next_blkoff = blk_off;

		if (curseg->alloc_type == SSR)
			blk_off = sbi->blocks_per_seg;

		for (j = 0; j < blk_off; j++) {
			struct f2fs_summary *s;
			s = (struct f2fs_summary *)(kaddr + offset);
			curseg->sum_blk->entries[j] = *s;
			offset += SUMMARY_SIZE;
			if (offset + SUMMARY_SIZE <= PAGE_CACHE_SIZE - SUM_FOOTER_SIZE)
				continue;
			memset(kaddr, 0, PAGE_SIZE);
			dev_read_block(kaddr, start++);
			offset = 0;
		}
	}

	free(kaddr);
	return 0;
}

int restore_node_summary(struct f2fs_sb_info *sbi,
		unsigned int segno, struct f2fs_summary_block *sum_blk)
{
	struct f2fs_node *node_blk;
	struct f2fs_summary *sum_entry;
	void *page;
	block_t addr;
	int i;

	page = malloc(PAGE_SIZE);
	if (!page)
		return -ENOMEM;

	/* scan the node segment */
	addr = START_BLOCK(sbi, segno);
	sum_entry = &sum_blk->entries[0];

	for (i = 0; i < sbi->blocks_per_seg; i++, sum_entry++) {
		if (dev_read_block(page, addr))
			goto out;

		node_blk = (struct f2fs_node *)page;
		sum_entry->nid = node_blk->footer.nid;
		/* do not change original value */
#if 0
		sum_entry->version = 0;
		sum_entry->ofs_in_node = 0;
#endif
		addr++;

	}
out:
	free(page);
	return 0;
}

int read_normal_summaries(struct f2fs_sb_info *sbi, int type)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct f2fs_summary_block *sum_blk;
	struct curseg_info *curseg;
	unsigned short blk_off;
	unsigned int segno = 0;
	block_t blk_addr = 0;

	if (IS_DATASEG(type)) {
		segno = le32_to_cpu(ckpt->cur_data_segno[type]);
		blk_off = le16_to_cpu(ckpt->cur_data_blkoff[type - CURSEG_HOT_DATA]);

		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG))
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_TYPE, type);
		else
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_DATA_TYPE, type);
	} else {
		segno = le32_to_cpu(ckpt->cur_node_segno[type - CURSEG_HOT_NODE]);
		blk_off = le16_to_cpu(ckpt->cur_node_blkoff[type - CURSEG_HOT_NODE]);

		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG))
			blk_addr = sum_blk_addr(sbi, NR_CURSEG_NODE_TYPE, type - CURSEG_HOT_NODE);
		else
			blk_addr = GET_SUM_BLKADDR(sbi, segno);
	}

	sum_blk = (struct f2fs_summary_block *)malloc(PAGE_SIZE);
	dev_read_block(sum_blk, blk_addr);

	if (IS_NODESEG(type)) {
		if (is_set_ckpt_flags(ckpt, CP_UMOUNT_FLAG)) {
			struct f2fs_summary *sum_entry = &sum_blk->entries[0];
			int i;
			for (i = 0; i < sbi->blocks_per_seg; i++, sum_entry++) {
				/* do not change original value */
#if 0
				sum_entry->version = 0;
				sum_entry->ofs_in_node = 0;
#endif
			}
		} else {
			if (restore_node_summary(sbi, segno, sum_blk)) {
				free(sum_blk);
				return -EINVAL;
			}
		}
	}

	curseg = CURSEG_I(sbi, type);
	memcpy(curseg->sum_blk, sum_blk, PAGE_CACHE_SIZE);
	curseg->next_segno = segno;
	reset_curseg(sbi, type, 0);
	curseg->alloc_type = ckpt->alloc_type[type];
	curseg->next_blkoff = blk_off;
	free(sum_blk);

	return 0;
}

int restore_curseg_summaries(struct f2fs_sb_info *sbi)
{
	int type = CURSEG_HOT_DATA;

	if (is_set_ckpt_flags(F2FS_CKPT(sbi), CP_COMPACT_SUM_FLAG)) {
		if (read_compacted_summaries(sbi))
			return -EINVAL;
		type = CURSEG_HOT_NODE;
	}

	for (; type <= CURSEG_COLD_NODE; type++) {
		if (read_normal_summaries(sbi, type))
			return -EINVAL;
	}
	return 0;
}

int build_curseg(struct f2fs_sb_info *sbi)
{
	struct curseg_info *array;
	int i;

	array = malloc(sizeof(*array) * NR_CURSEG_TYPE);

	SM_I(sbi)->curseg_array = array;

	for (i = 0; i < NR_CURSEG_TYPE; i++) {
		array[i].sum_blk = malloc(PAGE_CACHE_SIZE);
		if (!array[i].sum_blk)
			return -ENOMEM;
		array[i].segno = NULL_SEGNO;
		array[i].next_blkoff = 0;
	}
	return restore_curseg_summaries(sbi);
}

inline void check_seg_range(struct f2fs_sb_info *sbi, unsigned int segno)
{
	unsigned int end_segno = SM_I(sbi)->segment_count - 1;
	ASSERT(segno <= end_segno);
}

struct f2fs_sit_block *get_current_sit_page(struct f2fs_sb_info *sbi, unsigned int segno)
{
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int offset = SIT_BLOCK_OFFSET(sit_i, segno);
	block_t blk_addr = sit_i->sit_base_addr + offset;
	struct f2fs_sit_block *sit_blk = calloc(BLOCK_SZ, 1);

	check_seg_range(sbi, segno);

	/* calculate sit block address */
	if (f2fs_test_bit(offset, sit_i->sit_bitmap))
		blk_addr += sit_i->sit_blocks;

	dev_read_block(sit_blk, blk_addr);

	return sit_blk;
}

void check_block_count(struct f2fs_sb_info *sbi,
		int segno, struct f2fs_sit_entry *raw_sit)
{
	struct f2fs_sm_info *sm_info = SM_I(sbi);
	unsigned int end_segno = sm_info->segment_count - 1;
	int valid_blocks = 0;
	int i;

	/* check segment usage */
	ASSERT(GET_SIT_VBLOCKS(raw_sit) <= sbi->blocks_per_seg);

	/* check boundary of a given segment number */
	ASSERT(segno <= end_segno);

	/* check bitmap with valid block count */
	for (i = 0; i < sbi->blocks_per_seg; i++)
		if (f2fs_test_bit(i, (char *)raw_sit->valid_map))
			valid_blocks++;
	ASSERT(GET_SIT_VBLOCKS(raw_sit) == valid_blocks);
}

void seg_info_from_raw_sit(struct seg_entry *se,
		struct f2fs_sit_entry *raw_sit)
{
	se->valid_blocks = GET_SIT_VBLOCKS(raw_sit);
	se->ckpt_valid_blocks = GET_SIT_VBLOCKS(raw_sit);
	memcpy(se->cur_valid_map, raw_sit->valid_map, SIT_VBLOCK_MAP_SIZE);
	memcpy(se->ckpt_valid_map, raw_sit->valid_map, SIT_VBLOCK_MAP_SIZE);
	se->type = GET_SIT_TYPE(raw_sit);
	se->mtime = le64_to_cpu(raw_sit->mtime);
}

struct seg_entry *get_seg_entry(struct f2fs_sb_info *sbi,
		unsigned int segno)
{
	struct sit_info *sit_i = SIT_I(sbi);
	return &sit_i->sentries[segno];
}

int get_sum_block(struct f2fs_sb_info *sbi, unsigned int segno, struct f2fs_summary_block *sum_blk)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct curseg_info *curseg;
	int type, ret;
	u64 ssa_blk;

	ssa_blk = GET_SUM_BLKADDR(sbi, segno);
	for (type = 0; type < NR_CURSEG_NODE_TYPE; type++) {
		if (segno == ckpt->cur_node_segno[type]) {
			curseg = CURSEG_I(sbi, CURSEG_HOT_NODE + type);
			memcpy(sum_blk, curseg->sum_blk, BLOCK_SZ);
			return SEG_TYPE_CUR_NODE; /* current node seg was not stored */
		}
	}

	for (type = 0; type < NR_CURSEG_DATA_TYPE; type++) {
		if (segno == ckpt->cur_data_segno[type]) {
			curseg = CURSEG_I(sbi, type);
			memcpy(sum_blk, curseg->sum_blk, BLOCK_SZ);
			ASSERT(!IS_SUM_NODE_SEG(sum_blk->footer));
			DBG(2, "segno [0x%x] is current data seg[0x%x]\n", segno, type);
			return SEG_TYPE_CUR_DATA; /* current data seg was not stored */
		}
	}

	ret = dev_read_block(sum_blk, ssa_blk);
	ASSERT(ret >= 0);

	if (IS_SUM_NODE_SEG(sum_blk->footer))
		return SEG_TYPE_NODE;
	else
		return SEG_TYPE_DATA;

}

int get_sum_entry(struct f2fs_sb_info *sbi, u32 blk_addr, struct f2fs_summary *sum_entry)
{
	struct f2fs_summary_block *sum_blk;
	u32 segno, offset;
	int ret;

	segno = GET_SEGNO(sbi, blk_addr);
	offset = OFFSET_IN_SEG(sbi, blk_addr);

	sum_blk = calloc(BLOCK_SZ, 1);

	ret = get_sum_block(sbi, segno, sum_blk);

	memcpy(sum_entry, &(sum_blk->entries[offset]), sizeof(struct f2fs_summary));

	free(sum_blk);
	return ret;
}

int get_nat_entry(struct f2fs_sb_info *sbi, nid_t nid, struct f2fs_nat_entry *raw_nat)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct f2fs_nm_info *nm_i = NM_I(sbi);
	struct f2fs_nat_block *nat_block;
	pgoff_t block_off;
	pgoff_t block_addr;
	int seg_off, entry_off;
	int ret;

	if ((nid / NAT_ENTRY_PER_BLOCK) > fsck->nr_nat_entries) {
		DBG(0, "nid is over max nid\n");
		return -EINVAL;
	}

	if (lookup_nat_in_journal(sbi, nid, raw_nat) >= 0)
		return 0;

	nat_block = (struct f2fs_nat_block *)calloc(BLOCK_SZ, 1);

	block_off = nid / NAT_ENTRY_PER_BLOCK;
	entry_off = nid % NAT_ENTRY_PER_BLOCK;

	seg_off = block_off >> sbi->log_blocks_per_seg;
	block_addr = (pgoff_t)(nm_i->nat_blkaddr +
			(seg_off << sbi->log_blocks_per_seg << 1) +
			(block_off & ((1 << sbi->log_blocks_per_seg) - 1)));

	if (f2fs_test_bit(block_off, nm_i->nat_bitmap))
		block_addr += sbi->blocks_per_seg;

	ret = dev_read_block(nat_block, block_addr);
	ASSERT(ret >= 0);

	memcpy(raw_nat, &nat_block->entries[entry_off], sizeof(struct f2fs_nat_entry));
	free(nat_block);

	return 0;
}

int get_node_info(struct f2fs_sb_info *sbi, nid_t nid, struct node_info *ni)
{
	struct f2fs_nat_entry raw_nat;
	int ret;

	ret = get_nat_entry(sbi, nid, &raw_nat);
	ni->nid = nid;
	node_info_from_raw_nat(ni, &raw_nat);
	return ret;
}

void build_sit_entries(struct f2fs_sb_info *sbi)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_COLD_DATA);
	struct f2fs_summary_block *sum = curseg->sum_blk;
	unsigned int segno;

	for (segno = 0; segno < TOTAL_SEGS(sbi); segno++) {
		struct seg_entry *se = &sit_i->sentries[segno];
		struct f2fs_sit_block *sit_blk;
		struct f2fs_sit_entry sit;
		int i;

		for (i = 0; i < sits_in_cursum(sum); i++) {
			if (le32_to_cpu(segno_in_journal(sum, i)) == segno) {
				sit = sit_in_journal(sum, i);
				goto got_it;
			}
		}
		sit_blk = get_current_sit_page(sbi, segno);
		sit = sit_blk->entries[SIT_ENTRY_OFFSET(sit_i, segno)];
		free(sit_blk);
got_it:
		check_block_count(sbi, segno, &sit);
		seg_info_from_raw_sit(se, &sit);
	}

}

int build_segment_manager(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_super = F2FS_RAW_SUPER(sbi);
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	struct f2fs_sm_info *sm_info;

	sm_info = malloc(sizeof(struct f2fs_sm_info));
	if (!sm_info)
		return -ENOMEM;

	/* init sm info */
	sbi->sm_info = sm_info;
	sm_info->seg0_blkaddr = le32_to_cpu(raw_super->segment0_blkaddr);
	sm_info->main_blkaddr = le32_to_cpu(raw_super->main_blkaddr);
	sm_info->segment_count = le32_to_cpu(raw_super->segment_count);
	sm_info->reserved_segments = le32_to_cpu(ckpt->rsvd_segment_count);
	sm_info->ovp_segments = le32_to_cpu(ckpt->overprov_segment_count);
	sm_info->main_segments = le32_to_cpu(raw_super->segment_count_main);
	sm_info->ssa_blkaddr = le32_to_cpu(raw_super->ssa_blkaddr);

	build_sit_info(sbi);

	build_curseg(sbi);

	build_sit_entries(sbi);

	return 0;
}

int build_sit_area_bitmap(struct f2fs_sb_info *sbi)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct f2fs_sm_info *sm_i = SM_I(sbi);
	int segno = 0, j = 0;
	char *ptr = NULL;

	u32 sum_vblocks = 0;
	u32 free_segs = 0;
	u32 vblocks = 0;

	struct seg_entry *se;

	fsck->sit_area_bitmap_sz = sm_i->main_segments * SIT_VBLOCK_MAP_SIZE;
	fsck->sit_area_bitmap = calloc(1, fsck->sit_area_bitmap_sz);
	ptr = fsck->sit_area_bitmap;

	ASSERT(fsck->sit_area_bitmap_sz == fsck->main_area_bitmap_sz);

	for (segno = 0; segno < sm_i->main_segments; segno++) {
		se = get_seg_entry(sbi, segno);

		memcpy(ptr, se->cur_valid_map, SIT_VBLOCK_MAP_SIZE);
		ptr += SIT_VBLOCK_MAP_SIZE;

		vblocks = 0;
		for (j = 0; j < SIT_VBLOCK_MAP_SIZE; j++) {
			vblocks += get_bits_in_byte(se->cur_valid_map[j]);
		}
		ASSERT(vblocks == se->valid_blocks);

		if (se->valid_blocks == 0x0) {

			if (sbi->ckpt->cur_node_segno[0] == segno ||
					sbi->ckpt->cur_data_segno[0] == segno ||
					sbi->ckpt->cur_node_segno[1] == segno ||
					sbi->ckpt->cur_data_segno[1] == segno ||
					sbi->ckpt->cur_node_segno[2] == segno ||
					sbi->ckpt->cur_data_segno[2] == segno) {
				continue;
			} else {
				free_segs++;
			}

		} else {
			ASSERT(se->valid_blocks <= 512);
			sum_vblocks += se->valid_blocks;
		}
	}

	fsck->chk.sit_valid_blocks = sum_vblocks;
	fsck->chk.sit_free_segs = free_segs;

	DBG(1, " build SIT bitmap done: Blocks [0x%x : %d] Free Segs [0x%x : %d]\n\n", sum_vblocks, sum_vblocks,
			free_segs, free_segs);
	return 0;
}

int lookup_nat_in_journal(struct f2fs_sb_info *sbi, u32 nid, struct f2fs_nat_entry *raw_nat)
{
	struct curseg_info *curseg = CURSEG_I(sbi, CURSEG_HOT_DATA);
	struct f2fs_summary_block *sum = curseg->sum_blk;
	int i = 0;

	for (i = 0; i < nats_in_cursum(sum); i++) {
		if (le32_to_cpu(nid_in_journal(sum, i)) == nid) {
			memcpy(raw_nat, &nat_in_journal(sum, i), sizeof(struct f2fs_nat_entry));
			DBG(3, "==> Found nid [0x%x] in nat cache\n", nid);
			return i;
		}
	}
	return -1;
}

void build_nat_area_bitmap(struct f2fs_sb_info *sbi)
{
	struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
	struct f2fs_super_block *raw_sb = F2FS_RAW_SUPER(sbi);
	struct f2fs_nm_info *nm_i = NM_I(sbi);
	struct f2fs_nat_block *nat_block;
	u32 nid, nr_nat_blks;

	pgoff_t block_off;
	pgoff_t block_addr;
	int seg_off;
	int ret, i;


	nat_block = (struct f2fs_nat_block *)calloc(BLOCK_SZ, 1);

	/* Alloc & build nat entry bitmap */
	nr_nat_blks = (le32_to_cpu(raw_sb->segment_count_nat) / 2) << sbi->log_blocks_per_seg;

	fsck->nr_nat_entries = nr_nat_blks * NAT_ENTRY_PER_BLOCK;
	fsck->nat_area_bitmap_sz = (fsck->nr_nat_entries + 7) / 8;
	fsck->nat_area_bitmap = calloc(fsck->nat_area_bitmap_sz, 1);
	ASSERT(fsck->nat_area_bitmap != NULL);

	for (block_off = 0; block_off < nr_nat_blks; block_off++) {

		seg_off = block_off >> sbi->log_blocks_per_seg;
		block_addr = (pgoff_t)(nm_i->nat_blkaddr +
				(seg_off << sbi->log_blocks_per_seg << 1) +
				(block_off & ((1 << sbi->log_blocks_per_seg) - 1)));

		if (f2fs_test_bit(block_off, nm_i->nat_bitmap))
			block_addr += sbi->blocks_per_seg;

		ret = dev_read_block(nat_block, block_addr);
		ASSERT(ret >= 0);

		nid = block_off * NAT_ENTRY_PER_BLOCK;
		for (i = 0; i < NAT_ENTRY_PER_BLOCK; i++) {
			struct f2fs_nat_entry raw_nat;
			struct node_info ni;
			ni.nid = nid + i;

			if ((nid + i) == F2FS_NODE_INO(sbi) || (nid + i) == F2FS_META_INO(sbi)) {
				ASSERT(nat_block->entries[i].block_addr != 0x0);
				continue;
			}

			if (lookup_nat_in_journal(sbi, nid + i, &raw_nat) >= 0) {
				node_info_from_raw_nat(&ni, &raw_nat);
				if (ni.blk_addr != 0x0) {
					f2fs_set_bit(nid + i, fsck->nat_area_bitmap);
					fsck->chk.valid_nat_entry_cnt++;
					DBG(3, "nid[0x%x] in nat cache\n", nid + i);
				}
			} else {
				node_info_from_raw_nat(&ni, &nat_block->entries[i]);
				if (ni.blk_addr != 0) {
					ASSERT(nid + i != 0x0);

					DBG(3, "nid[0x%8x] in nat entry [0x%16x] [0x%8x]\n",
							nid + i,
							ni.blk_addr,
							ni.ino);

					f2fs_set_bit(nid + i, fsck->nat_area_bitmap);
					fsck->chk.valid_nat_entry_cnt++;
				}
			}
		}
	}
	free(nat_block);

	DBG(1, "valid nat entries (block_addr != 0x0) [0x%8x : %u]\n",
			fsck->chk.valid_nat_entry_cnt, fsck->chk.valid_nat_entry_cnt);

}

int f2fs_do_mount(struct f2fs_sb_info *sbi)
{
	int ret;
	sbi->active_logs = NR_CURSEG_TYPE;
	ret = validate_super_block(sbi, 0);
	if (ret) {
		ret = validate_super_block(sbi, 1);
		if (ret)
			return -1;
	}

	print_raw_sb_info(sbi);

	init_sb_info(sbi);

	ret = get_valid_checkpoint(sbi);
	if (ret) {
		ERR_MSG("Can't find valid checkpoint\n");
		return -1;
	}

	if (sanity_check_ckpt(sbi)) {
		ERR_MSG("Checkpoint is polluted\n");
		return -1;
	}

	print_ckpt_info(sbi);

	sbi->total_valid_node_count = le32_to_cpu(sbi->ckpt->valid_node_count);
	sbi->total_valid_inode_count = le32_to_cpu(sbi->ckpt->valid_inode_count);
	sbi->user_block_count = le64_to_cpu(sbi->ckpt->user_block_count);
	sbi->total_valid_block_count = le64_to_cpu(sbi->ckpt->valid_block_count);
	sbi->last_valid_block_count = sbi->total_valid_block_count;
	sbi->alloc_valid_block_count = 0;

	if (build_segment_manager(sbi)) {
		ERR_MSG("build_segment_manager failed\n");
		return -1;
	}

	if (build_node_manager(sbi)) {
		ERR_MSG("build_segment_manager failed\n");
		return -1;
	}

	return ret;
}

void f2fs_do_umount(struct f2fs_sb_info *sbi)
{
	struct sit_info *sit_i = SIT_I(sbi);
	struct f2fs_sm_info *sm_i = SM_I(sbi);
	struct f2fs_nm_info *nm_i = NM_I(sbi);
	int i;

	/* free nm_info */
	free(nm_i->nat_bitmap);
	free(sbi->nm_info);

	/* free sit_info */
	for (i = 0; i < TOTAL_SEGS(sbi); i++) {
		free(sit_i->sentries[i].cur_valid_map);
		free(sit_i->sentries[i].ckpt_valid_map);
	}
	free(sit_i->sit_bitmap);
	free(sm_i->sit_info);

	/* free sm_info */
	for (i = 0; i < NR_CURSEG_TYPE; i++)
		free(sm_i->curseg_array[i].sum_blk);

	free(sm_i->curseg_array);
	free(sbi->sm_info);

	free(sbi->ckpt);
	free(sbi->raw_super);
}
