/**
 * dump.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <inttypes.h>

#include "node.h"
#include "fsck.h"
#include "xattr.h"
#ifdef HAVE_ATTR_XATTR_H
#include <attr/xattr.h>
#endif
#ifdef HAVE_LINUX_XATTR_H
#include <linux/xattr.h>
#endif
#include <locale.h>

#define BUF_SZ	80

/* current extent info */
struct extent_info dump_extent;

const char *seg_type_name[SEG_TYPE_MAX + 1] = {
	"SEG_TYPE_DATA",
	"SEG_TYPE_CUR_DATA",
	"SEG_TYPE_NODE",
	"SEG_TYPE_CUR_NODE",
	"SEG_TYPE_NONE",
};

void nat_dump(struct f2fs_sb_info *sbi, nid_t start_nat, nid_t end_nat)
{
	struct f2fs_nat_block *nat_block;
	struct f2fs_node *node_block;
	nid_t nid;
	pgoff_t block_addr;
	char buf[BUF_SZ];
	int fd, ret, pack;

	nat_block = (struct f2fs_nat_block *)calloc(BLOCK_SZ, 1);
	ASSERT(nat_block);
	node_block = (struct f2fs_node *)calloc(BLOCK_SZ, 1);
	ASSERT(node_block);

	fd = open("dump_nat", O_CREAT|O_WRONLY|O_TRUNC, 0666);
	ASSERT(fd >= 0);

	for (nid = start_nat; nid < end_nat; nid++) {
		struct f2fs_nat_entry raw_nat;
		struct node_info ni;
		if(nid == 0 || nid == F2FS_NODE_INO(sbi) ||
					nid == F2FS_META_INO(sbi))
			continue;

		ni.nid = nid;
		block_addr = current_nat_addr(sbi, nid, &pack);

		if (lookup_nat_in_journal(sbi, nid, &raw_nat) >= 0) {
			node_info_from_raw_nat(&ni, &raw_nat);
			ret = dev_read_block(node_block, ni.blk_addr);
			ASSERT(ret >= 0);
			if (ni.blk_addr != 0x0) {
				memset(buf, 0, BUF_SZ);
				snprintf(buf, BUF_SZ,
					"nid:%5u\tino:%5u\toffset:%5u"
					"\tblkaddr:%10u\tpack:%d\n",
					ni.nid, ni.ino,
					le32_to_cpu(node_block->footer.flag) >>
						OFFSET_BIT_SHIFT,
					ni.blk_addr, pack);
				ret = write(fd, buf, strlen(buf));
				ASSERT(ret >= 0);
			}
		} else {
			ret = dev_read_block(nat_block, block_addr);
			ASSERT(ret >= 0);
			node_info_from_raw_nat(&ni,
					&nat_block->entries[nid % NAT_ENTRY_PER_BLOCK]);
			if (ni.blk_addr == 0)
				continue;

			ret = dev_read_block(node_block, ni.blk_addr);
			ASSERT(ret >= 0);
			memset(buf, 0, BUF_SZ);
			snprintf(buf, BUF_SZ,
				"nid:%5u\tino:%5u\toffset:%5u"
				"\tblkaddr:%10u\tpack:%d\n",
				ni.nid, ni.ino,
				le32_to_cpu(node_block->footer.flag) >>
					OFFSET_BIT_SHIFT,
				ni.blk_addr, pack);
			ret = write(fd, buf, strlen(buf));
			ASSERT(ret >= 0);
		}
	}

	free(nat_block);
	free(node_block);

	close(fd);
}

void sit_dump(struct f2fs_sb_info *sbi, unsigned int start_sit,
					unsigned int end_sit)
{
	struct seg_entry *se;
	struct sit_info *sit_i = SIT_I(sbi);
	unsigned int segno;
	char buf[BUF_SZ];
	u32 free_segs = 0;;
	u64 valid_blocks = 0;
	int ret;
	int fd, i;
	unsigned int offset;

	fd = open("dump_sit", O_CREAT|O_WRONLY|O_TRUNC, 0666);
	ASSERT(fd >= 0);

	snprintf(buf, BUF_SZ, "segment_type(0:HD, 1:WD, 2:CD, "
						"3:HN, 4:WN, 5:CN)\n");
	ret = write(fd, buf, strlen(buf));
	ASSERT(ret >= 0);

	for (segno = start_sit; segno < end_sit; segno++) {
		se = get_seg_entry(sbi, segno);
		offset = SIT_BLOCK_OFFSET(sit_i, segno);
		memset(buf, 0, BUF_SZ);
		snprintf(buf, BUF_SZ,
		"\nsegno:%8u\tvblocks:%3u\tseg_type:%d\tsit_pack:%d\n\n",
			segno, se->valid_blocks, se->type,
			f2fs_test_bit(offset, sit_i->sit_bitmap) ? 2 : 1);

		ret = write(fd, buf, strlen(buf));
		ASSERT(ret >= 0);

		if (se->valid_blocks == 0x0) {
			free_segs++;
			continue;
		}

		ASSERT(se->valid_blocks <= 512);
		valid_blocks += se->valid_blocks;

		for (i = 0; i < 64; i++) {
			memset(buf, 0, BUF_SZ);
			snprintf(buf, BUF_SZ, "  %02x",
					*(se->cur_valid_map + i));
			ret = write(fd, buf, strlen(buf));
			ASSERT(ret >= 0);

			if ((i + 1) % 16 == 0) {
				snprintf(buf, BUF_SZ, "\n");
				ret = write(fd, buf, strlen(buf));
				ASSERT(ret >= 0);
			}
		}
	}

	memset(buf, 0, BUF_SZ);
	snprintf(buf, BUF_SZ,
		"valid_blocks:[0x%" PRIx64 "]\tvalid_segs:%d\t free_segs:%d\n",
			valid_blocks,
			SM_I(sbi)->main_segments - free_segs,
			free_segs);
	ret = write(fd, buf, strlen(buf));
	ASSERT(ret >= 0);

	close(fd);
}

void ssa_dump(struct f2fs_sb_info *sbi, int start_ssa, int end_ssa)
{
	struct f2fs_summary_block *sum_blk;
	char buf[BUF_SZ];
	int segno, type, i, ret;
	int fd;

	fd = open("dump_ssa", O_CREAT|O_WRONLY|O_TRUNC, 0666);
	ASSERT(fd >= 0);

	snprintf(buf, BUF_SZ, "Note: dump.f2fs -b blkaddr = 0x%x + segno * "
				" 0x200 + offset\n",
				sbi->sm_info->main_blkaddr);
	ret = write(fd, buf, strlen(buf));
	ASSERT(ret >= 0);

	for (segno = start_ssa; segno < end_ssa; segno++) {
		sum_blk = get_sum_block(sbi, segno, &type);

		memset(buf, 0, BUF_SZ);
		switch (type) {
		case SEG_TYPE_CUR_NODE:
			snprintf(buf, BUF_SZ, "\n\nsegno: %x, Current Node\n", segno);
			break;
		case SEG_TYPE_CUR_DATA:
			snprintf(buf, BUF_SZ, "\n\nsegno: %x, Current Data\n", segno);
			break;
		case SEG_TYPE_NODE:
			snprintf(buf, BUF_SZ, "\n\nsegno: %x, Node\n", segno);
			break;
		case SEG_TYPE_DATA:
			snprintf(buf, BUF_SZ, "\n\nsegno: %x, Data\n", segno);
			break;
		}
		ret = write(fd, buf, strlen(buf));
		ASSERT(ret >= 0);

		for (i = 0; i < ENTRIES_IN_SUM; i++) {
			memset(buf, 0, BUF_SZ);
			if (i % 10 == 0) {
				buf[0] = '\n';
				ret = write(fd, buf, strlen(buf));
				ASSERT(ret >= 0);
			}
			snprintf(buf, BUF_SZ, "[%3d: %6x]", i,
					le32_to_cpu(sum_blk->entries[i].nid));
			ret = write(fd, buf, strlen(buf));
			ASSERT(ret >= 0);
		}
		if (type == SEG_TYPE_NODE || type == SEG_TYPE_DATA ||
					type == SEG_TYPE_MAX)
			free(sum_blk);
	}
	close(fd);
}

static void print_extent(bool last)
{
	if (dump_extent.len == 0)
		goto out;

	if (dump_extent.len == 1)
		printf(" %d", dump_extent.blk);
	else
		printf(" %d-%d",
			dump_extent.blk,
			dump_extent.blk + dump_extent.len - 1);
	dump_extent.len = 0;
out:
	if (last)
		printf("\n");
}

static void dump_data_blk(struct f2fs_sb_info *sbi, __u64 offset, u32 blkaddr)
{
	char buf[F2FS_BLKSIZE];

	if (c.show_file_map) {
		if (c.show_file_map_max_offset < offset) {
			ASSERT(blkaddr == NULL_ADDR);
			return;
		}
		if (!is_valid_data_blkaddr(blkaddr)) {
			print_extent(false);
			dump_extent.blk = 0;
			dump_extent.len = 1;
			print_extent(false);
		} else if (dump_extent.len == 0) {
			dump_extent.blk = blkaddr;
			dump_extent.len = 1;
		} else if (dump_extent.blk + dump_extent.len == blkaddr) {
			dump_extent.len++;
		} else {
			print_extent(false);
			dump_extent.blk = blkaddr;
			dump_extent.len = 1;
		}
		return;
	}

	if (blkaddr == NULL_ADDR)
		return;

	/* get data */
	if (blkaddr == NEW_ADDR || !IS_VALID_BLK_ADDR(sbi, blkaddr)) {
		memset(buf, 0, F2FS_BLKSIZE);
	} else {
		int ret;

		ret = dev_read_block(buf, blkaddr);
		ASSERT(ret >= 0);
	}

	/* write blkaddr */
	dev_write_dump(buf, offset, F2FS_BLKSIZE);
}

static void dump_node_blk(struct f2fs_sb_info *sbi, int ntype,
				u32 nid, u32 addr_per_block, u64 *ofs)
{
	struct node_info ni;
	struct f2fs_node *node_blk;
	u32 skip = 0;
	u32 i, idx = 0;

	switch (ntype) {
	case TYPE_DIRECT_NODE:
		skip = idx = addr_per_block;
		break;
	case TYPE_INDIRECT_NODE:
		idx = NIDS_PER_BLOCK;
		skip = idx * addr_per_block;
		break;
	case TYPE_DOUBLE_INDIRECT_NODE:
		skip = 0;
		idx = NIDS_PER_BLOCK;
		break;
	}

	if (nid == 0) {
		*ofs += skip;
		return;
	}

	get_node_info(sbi, nid, &ni);

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	dev_read_block(node_blk, ni.blk_addr);

	for (i = 0; i < idx; i++) {
		switch (ntype) {
		case TYPE_DIRECT_NODE:
			dump_data_blk(sbi, *ofs * F2FS_BLKSIZE,
					le32_to_cpu(node_blk->dn.addr[i]));
			(*ofs)++;
			break;
		case TYPE_INDIRECT_NODE:
			dump_node_blk(sbi, TYPE_DIRECT_NODE,
					le32_to_cpu(node_blk->in.nid[i]),
					addr_per_block,
					ofs);
			break;
		case TYPE_DOUBLE_INDIRECT_NODE:
			dump_node_blk(sbi, TYPE_INDIRECT_NODE,
					le32_to_cpu(node_blk->in.nid[i]),
					addr_per_block,
					ofs);
			break;
		}
	}
	free(node_blk);
}

#ifdef HAVE_FSETXATTR
static void dump_xattr(struct f2fs_sb_info *sbi, struct f2fs_node *node_blk)
{
	void *xattr;
	struct f2fs_xattr_entry *ent;
	char xattr_name[F2FS_NAME_LEN] = {0};
	int ret;

	xattr = read_all_xattrs(sbi, node_blk);
	if (!xattr)
		return;

	list_for_each_xattr(ent, xattr) {
		char *name = strndup(ent->e_name, ent->e_name_len);
		void *value = ent->e_name + ent->e_name_len;

		if (!name)
			continue;

		switch (ent->e_name_index) {
		case F2FS_XATTR_INDEX_USER:
			ret = snprintf(xattr_name, F2FS_NAME_LEN, "%s%s",
				       XATTR_USER_PREFIX, name);
			break;

		case F2FS_XATTR_INDEX_SECURITY:
			ret = snprintf(xattr_name, F2FS_NAME_LEN, "%s%s",
				       XATTR_SECURITY_PREFIX, name);
			break;
		case F2FS_XATTR_INDEX_TRUSTED:
			ret = snprintf(xattr_name, F2FS_NAME_LEN, "%s%s",
				       XATTR_TRUSTED_PREFIX, name);
			break;
		default:
			MSG(0, "Unknown xattr index 0x%x\n", ent->e_name_index);
			free(name);
			continue;
		}
		if (ret >= F2FS_NAME_LEN) {
			MSG(0, "XATTR index 0x%x name too long\n", ent->e_name_index);
			free(name);
			continue;
		}

		DBG(1, "fd %d xattr_name %s\n", c.dump_fd, xattr_name);
#if defined(__linux__)
		ret = fsetxattr(c.dump_fd, xattr_name, value,
				le16_to_cpu(ent->e_value_size), 0);
#elif defined(__APPLE__)
		ret = fsetxattr(c.dump_fd, xattr_name, value,
				le16_to_cpu(ent->e_value_size), 0,
				XATTR_CREATE);
#endif
		if (ret)
			MSG(0, "XATTR index 0x%x set xattr failed error %d\n",
			    ent->e_name_index, errno);

		free(name);
	}

	free(xattr);
}
#else
static void dump_xattr(struct f2fs_sb_info *UNUSED(sbi),
				struct f2fs_node *UNUSED(node_blk))
{
	MSG(0, "XATTR does not support\n");
}
#endif

static int dump_inode_blk(struct f2fs_sb_info *sbi, u32 nid,
					struct f2fs_node *node_blk)
{
	u32 i = 0;
	u64 ofs = 0;
	u32 addr_per_block;

	if((node_blk->i.i_inline & F2FS_INLINE_DATA)) {
		DBG(3, "ino[0x%x] has inline data!\n", nid);
		/* recover from inline data */
		dev_write_dump(((unsigned char *)node_blk) + INLINE_DATA_OFFSET,
						0, MAX_INLINE_DATA(node_blk));
		return -1;
	}

	c.show_file_map_max_offset = f2fs_max_file_offset(&node_blk->i);
	addr_per_block = ADDRS_PER_BLOCK(&node_blk->i);

	/* check data blocks in inode */
	for (i = 0; i < ADDRS_PER_INODE(&node_blk->i); i++, ofs++)
		dump_data_blk(sbi, ofs * F2FS_BLKSIZE, le32_to_cpu(
			node_blk->i.i_addr[get_extra_isize(node_blk) + i]));

	/* check node blocks in inode */
	for (i = 0; i < 5; i++) {
		if (i == 0 || i == 1)
			dump_node_blk(sbi, TYPE_DIRECT_NODE,
					le32_to_cpu(node_blk->i.i_nid[i]),
					addr_per_block,
					&ofs);
		else if (i == 2 || i == 3)
			dump_node_blk(sbi, TYPE_INDIRECT_NODE,
					le32_to_cpu(node_blk->i.i_nid[i]),
					addr_per_block,
					&ofs);
		else if (i == 4)
			dump_node_blk(sbi, TYPE_DOUBLE_INDIRECT_NODE,
					le32_to_cpu(node_blk->i.i_nid[i]),
					addr_per_block,
					&ofs);
		else
			ASSERT(0);
	}
	/* last block in extent cache */
	print_extent(true);

	dump_xattr(sbi, node_blk);
	return 0;
}

static int dump_file(struct f2fs_sb_info *sbi, struct node_info *ni,
				struct f2fs_node *node_blk, int force)
{
	struct f2fs_inode *inode = &node_blk->i;
	u32 imode = le16_to_cpu(inode->i_mode);
	u32 namelen = le32_to_cpu(inode->i_namelen);
	char name[F2FS_NAME_LEN + 1] = {0};
	char path[1024] = {0};
	char ans[255] = {0};
	int is_encrypted = file_is_encrypt(inode);
	int ret;

	if (is_encrypted) {
		MSG(force, "File is encrypted\n");
		return -1;
	}

	if ((!S_ISREG(imode) && !S_ISLNK(imode)) ||
				namelen == 0 || namelen > F2FS_NAME_LEN) {
		MSG(force, "Not a regular file or wrong name info\n\n");
		return -1;
	}
	if (force)
		goto dump;

	/* dump file's data */
	if (c.show_file_map)
		return dump_inode_blk(sbi, ni->ino, node_blk);

	printf("Do you want to dump this file into ./lost_found/? [Y/N] ");
	ret = scanf("%s", ans);
	ASSERT(ret >= 0);

	if (!strcasecmp(ans, "y")) {
dump:
		ret = system("mkdir -p ./lost_found");
		ASSERT(ret >= 0);

		/* make a file */
		strncpy(name, (const char *)inode->i_name, namelen);
		name[namelen] = 0;
		sprintf(path, "./lost_found/%s", name);

		c.dump_fd = open(path, O_TRUNC|O_CREAT|O_RDWR, 0666);
		ASSERT(c.dump_fd >= 0);

		/* dump file's data */
		dump_inode_blk(sbi, ni->ino, node_blk);

		/* adjust file size */
		ret = ftruncate(c.dump_fd, le32_to_cpu(inode->i_size));
		ASSERT(ret >= 0);

		close(c.dump_fd);
	}
	return 0;
}

static bool is_sit_bitmap_set(struct f2fs_sb_info *sbi, u32 blk_addr)
{
	struct seg_entry *se;
	u32 offset;

	se = get_seg_entry(sbi, GET_SEGNO(sbi, blk_addr));
	offset = OFFSET_IN_SEG(sbi, blk_addr);

	return f2fs_test_bit(offset,
			(const char *)se->cur_valid_map) != 0;
}

int dump_node(struct f2fs_sb_info *sbi, nid_t nid, int force)
{
	struct node_info ni;
	struct f2fs_node *node_blk;
	int ret = 0;

	get_node_info(sbi, nid, &ni);

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	DBG(1, "Node ID               [0x%x]\n", nid);
	DBG(1, "nat_entry.block_addr  [0x%x]\n", ni.blk_addr);
	DBG(1, "nat_entry.version     [0x%x]\n", ni.version);
	DBG(1, "nat_entry.ino         [0x%x]\n", ni.ino);

	if (!IS_VALID_BLK_ADDR(sbi, ni.blk_addr)) {
		MSG(force, "Invalid node blkaddr: %u\n\n", ni.blk_addr);
		goto out;
	}

	dev_read_block(node_blk, ni.blk_addr);

	if (ni.blk_addr == 0x0)
		MSG(force, "Invalid nat entry\n\n");
	else if (!is_sit_bitmap_set(sbi, ni.blk_addr))
		MSG(force, "Invalid sit bitmap, %u\n\n", ni.blk_addr);

	DBG(1, "node_blk.footer.ino [0x%x]\n", le32_to_cpu(node_blk->footer.ino));
	DBG(1, "node_blk.footer.nid [0x%x]\n", le32_to_cpu(node_blk->footer.nid));

	if (le32_to_cpu(node_blk->footer.ino) == ni.ino &&
			le32_to_cpu(node_blk->footer.nid) == ni.nid) {
		if (!c.show_file_map)
			print_node_info(sbi, node_blk, force);

		if (ni.ino == ni.nid)
			ret = dump_file(sbi, &ni, node_blk, force);
	} else {
		print_node_info(sbi, node_blk, force);
		MSG(force, "Invalid (i)node block\n\n");
	}
out:
	free(node_blk);
	return ret;
}

static void dump_node_from_blkaddr(struct f2fs_sb_info *sbi, u32 blk_addr)
{
	struct f2fs_node *node_blk;
	int ret;

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	ret = dev_read_block(node_blk, blk_addr);
	ASSERT(ret >= 0);

	if (c.dbg_lv > 0)
		print_node_info(sbi, node_blk, 0);
	else
		print_inode_info(sbi, node_blk, 1);

	free(node_blk);
}

unsigned int start_bidx_of_node(unsigned int node_ofs,
					struct f2fs_node *node_blk)
{
	unsigned int indirect_blks = 2 * NIDS_PER_BLOCK + 4;
	unsigned int bidx;

	if (node_ofs == 0)
		return 0;

	if (node_ofs <= 2) {
		bidx = node_ofs - 1;
	} else if (node_ofs <= indirect_blks) {
		int dec = (node_ofs - 4) / (NIDS_PER_BLOCK + 1);
		bidx = node_ofs - 2 - dec;
	} else {
		int dec = (node_ofs - indirect_blks - 3) / (NIDS_PER_BLOCK + 1);
		bidx = node_ofs - 5 - dec;
	}
	return bidx * ADDRS_PER_BLOCK(&node_blk->i) +
				ADDRS_PER_INODE(&node_blk->i);
}

static void dump_data_offset(u32 blk_addr, int ofs_in_node)
{
	struct f2fs_node *node_blk;
	unsigned int bidx;
	unsigned int node_ofs;
	int ret;

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	ret = dev_read_block(node_blk, blk_addr);
	ASSERT(ret >= 0);

	node_ofs = ofs_of_node(node_blk);

	bidx = start_bidx_of_node(node_ofs, node_blk);
	bidx +=  ofs_in_node;

	setlocale(LC_ALL, "");
	MSG(0, " - Data offset       : 0x%x (4KB), %'u (bytes)\n",
				bidx, bidx * 4096);
	free(node_blk);
}

static void dump_node_offset(u32 blk_addr)
{
	struct f2fs_node *node_blk;
	int ret;

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	ret = dev_read_block(node_blk, blk_addr);
	ASSERT(ret >= 0);

	MSG(0, " - Node offset       : 0x%x\n", ofs_of_node(node_blk));
	free(node_blk);
}

static int has_dirent(u32 blk_addr, int is_inline, int *enc_name)
{
	struct f2fs_node *node_blk;
	int ret, is_dentry = 0;

	node_blk = calloc(BLOCK_SZ, 1);
	ASSERT(node_blk);

	ret = dev_read_block(node_blk, blk_addr);
	ASSERT(ret >= 0);

	if (IS_INODE(node_blk) && S_ISDIR(le16_to_cpu(node_blk->i.i_mode)))
		is_dentry = 1;

	if (is_inline && !(node_blk->i.i_inline & F2FS_INLINE_DENTRY))
		is_dentry = 0;

	*enc_name = file_is_encrypt(&node_blk->i);

	free(node_blk);

	return is_dentry;
}

static void dump_dirent(u32 blk_addr, int is_inline, int enc_name)
{
	struct f2fs_dentry_ptr d;
	void *inline_dentry, *blk;
	int ret, i = 0;

	blk = calloc(BLOCK_SZ, 1);
	ASSERT(blk);

	ret = dev_read_block(blk, blk_addr);
	ASSERT(ret >= 0);

	if (is_inline) {
		inline_dentry = inline_data_addr((struct f2fs_node *)blk);
		make_dentry_ptr(&d, blk, inline_dentry, 2);
	} else {
		make_dentry_ptr(&d, NULL, blk, 1);
	}

	DBG(1, "%sDentry block:\n", is_inline ? "Inline " : "");

	while (i < d.max) {
		struct f2fs_dir_entry *de;
		char en[F2FS_PRINT_NAMELEN];
		u16 name_len;
		int enc;

		if (!test_bit_le(i, d.bitmap)) {
			i++;
			continue;
		}

		de = &d.dentry[i];

		if (!de->name_len) {
			i++;
			continue;
		}

		name_len = le16_to_cpu(de->name_len);
		enc = enc_name;

		if (de->file_type == F2FS_FT_DIR) {
			if ((d.filename[i][0] == '.' && name_len == 1) ||
				(d.filename[i][0] == '.' &&
				d.filename[i][1] == '.' && name_len == 2)) {
				enc = 0;
			}
		}

		pretty_print_filename(d.filename[i], name_len, en, enc);

		DBG(1, "bitmap pos[0x%x] name[%s] len[0x%x] hash[0x%x] ino[0x%x] type[0x%x]\n",
				i, en,
				name_len,
				le32_to_cpu(de->hash_code),
				le32_to_cpu(de->ino),
				de->file_type);

		i += GET_DENTRY_SLOTS(name_len);
	}

	free(blk);
}

int dump_info_from_blkaddr(struct f2fs_sb_info *sbi, u32 blk_addr)
{
	nid_t nid;
	int type;
	struct f2fs_summary sum_entry;
	struct node_info ni, ino_ni;
	int enc_name;
	int ret = 0;

	MSG(0, "\n== Dump data from block address ==\n\n");

	if (blk_addr < SM_I(sbi)->seg0_blkaddr) {
		MSG(0, "\nFS Reserved Area for SEG #0: ");
		ret = -EINVAL;
	} else if (blk_addr < SIT_I(sbi)->sit_base_addr) {
		MSG(0, "\nFS Metadata Area: ");
		ret = -EINVAL;
	} else if (blk_addr < NM_I(sbi)->nat_blkaddr) {
		MSG(0, "\nFS SIT Area: ");
		ret = -EINVAL;
	} else if (blk_addr < SM_I(sbi)->ssa_blkaddr) {
		MSG(0, "\nFS NAT Area: ");
		ret = -EINVAL;
	} else if (blk_addr < SM_I(sbi)->main_blkaddr) {
		MSG(0, "\nFS SSA Area: ");
		ret = -EINVAL;
	} else if (blk_addr > __end_block_addr(sbi)) {
		MSG(0, "\nOut of address space: ");
		ret = -EINVAL;
	}

	if (ret) {
		MSG(0, "User data is from 0x%x to 0x%x\n\n",
			SM_I(sbi)->main_blkaddr,
			__end_block_addr(sbi));
		return ret;
	}

	if (!is_sit_bitmap_set(sbi, blk_addr))
		MSG(0, "\nblkaddr is not valid\n");

	type = get_sum_entry(sbi, blk_addr, &sum_entry);
	nid = le32_to_cpu(sum_entry.nid);

	get_node_info(sbi, nid, &ni);

	DBG(1, "Note: blkaddr = main_blkaddr + segno * 512 + offset\n");
	DBG(1, "Block_addr            [0x%x]\n", blk_addr);
	DBG(1, " - Segno              [0x%x]\n", GET_SEGNO(sbi, blk_addr));
	DBG(1, " - Offset             [0x%x]\n", OFFSET_IN_SEG(sbi, blk_addr));
	DBG(1, "SUM.nid               [0x%x]\n", nid);
	DBG(1, "SUM.type              [%s]\n", type >= 0 ?
						seg_type_name[type] :
						"Broken");
	DBG(1, "SUM.version           [%d]\n", sum_entry.version);
	DBG(1, "SUM.ofs_in_node       [0x%x]\n", sum_entry.ofs_in_node);
	DBG(1, "NAT.blkaddr           [0x%x]\n", ni.blk_addr);
	DBG(1, "NAT.ino               [0x%x]\n", ni.ino);

	get_node_info(sbi, ni.ino, &ino_ni);

	/* inode block address */
	if (ni.blk_addr == NULL_ADDR || ino_ni.blk_addr == NULL_ADDR) {
		MSG(0, "FS Userdata Area: Obsolete block from 0x%x\n",
			blk_addr);
		return -EINVAL;
	}

	/* print inode */
	if (c.dbg_lv > 0)
		dump_node_from_blkaddr(sbi, ino_ni.blk_addr);

	if (type == SEG_TYPE_CUR_DATA || type == SEG_TYPE_DATA) {
		MSG(0, "FS Userdata Area: Data block from 0x%x\n", blk_addr);
		MSG(0, " - Direct node block : id = 0x%x from 0x%x\n",
					nid, ni.blk_addr);
		MSG(0, " - Inode block       : id = 0x%x from 0x%x\n",
					ni.ino, ino_ni.blk_addr);
		dump_node_from_blkaddr(sbi, ino_ni.blk_addr);
		dump_data_offset(ni.blk_addr,
			le16_to_cpu(sum_entry.ofs_in_node));

		if (has_dirent(ino_ni.blk_addr, 0, &enc_name))
			dump_dirent(blk_addr, 0, enc_name);
	} else {
		MSG(0, "FS Userdata Area: Node block from 0x%x\n", blk_addr);
		if (ni.ino == ni.nid) {
			MSG(0, " - Inode block       : id = 0x%x from 0x%x\n",
					ni.ino, ino_ni.blk_addr);
			dump_node_from_blkaddr(sbi, ino_ni.blk_addr);

			if (has_dirent(ino_ni.blk_addr, 1, &enc_name))
				dump_dirent(blk_addr, 1, enc_name);
		} else {
			MSG(0, " - Node block        : id = 0x%x from 0x%x\n",
					nid, ni.blk_addr);
			MSG(0, " - Inode block       : id = 0x%x from 0x%x\n",
					ni.ino, ino_ni.blk_addr);
			dump_node_from_blkaddr(sbi, ino_ni.blk_addr);
			dump_node_offset(ni.blk_addr);
		}
	}

	return 0;
}
