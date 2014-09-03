/**
 * f2fs.h
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef _F2FS_H_
#define _F2FS_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <assert.h>

#include "list.h"
#include "f2fs_fs.h"

#define EXIT_ERR_CODE		(-1)
#define ver_after(a, b) (typecheck(unsigned long long, a) &&            \
		typecheck(unsigned long long, b) &&                     \
		((long long)((a) - (b)) > 0))

enum {
	NAT_BITMAP,
	SIT_BITMAP
};

struct node_info {
	nid_t nid;
	nid_t ino;
	u32 blk_addr;
	unsigned char version;
};

struct f2fs_nm_info {
	block_t nat_blkaddr;
	nid_t max_nid;
	nid_t init_scan_nid;
	nid_t next_scan_nid;

	unsigned int nat_cnt;
	unsigned int fcnt;

	char *nat_bitmap;
	int bitmap_size;
};

struct seg_entry {
	unsigned short valid_blocks;    /* # of valid blocks */
	unsigned char *cur_valid_map;   /* validity bitmap of blocks */
	/*
	 * # of valid blocks and the validity bitmap stored in the the last
	 * checkpoint pack. This information is used by the SSR mode.
	 */
	unsigned short ckpt_valid_blocks;
	unsigned char *ckpt_valid_map;
	unsigned char type;             /* segment type like CURSEG_XXX_TYPE */
	unsigned long long mtime;       /* modification time of the segment */
};

struct sec_entry {
	unsigned int valid_blocks;      /* # of valid blocks in a section */
};

struct sit_info {

	block_t sit_base_addr;          /* start block address of SIT area */
	block_t sit_blocks;             /* # of blocks used by SIT area */
	block_t written_valid_blocks;   /* # of valid blocks in main area */
	char *sit_bitmap;               /* SIT bitmap pointer */
	unsigned int bitmap_size;       /* SIT bitmap size */

	unsigned long *dirty_sentries_bitmap;   /* bitmap for dirty sentries */
	unsigned int dirty_sentries;            /* # of dirty sentries */
	unsigned int sents_per_block;           /* # of SIT entries per block */
	struct seg_entry *sentries;             /* SIT segment-level cache */
	struct sec_entry *sec_entries;          /* SIT section-level cache */

	unsigned long long elapsed_time;        /* elapsed time after mount */
	unsigned long long mounted_time;        /* mount time */
	unsigned long long min_mtime;           /* min. modification time */
	unsigned long long max_mtime;           /* max. modification time */
};

struct curseg_info {
	struct f2fs_summary_block *sum_blk;     /* cached summary block */
	unsigned char alloc_type;               /* current allocation type */
	unsigned int segno;                     /* current segment number */
	unsigned short next_blkoff;             /* next block offset to write */
	unsigned int zone;                      /* current zone number */
	unsigned int next_segno;                /* preallocated segment */
};

struct f2fs_sm_info {
	struct sit_info *sit_info;
	struct curseg_info *curseg_array;

	block_t seg0_blkaddr;
	block_t main_blkaddr;
	block_t ssa_blkaddr;

	unsigned int segment_count;
	unsigned int main_segments;
	unsigned int reserved_segments;
	unsigned int ovp_segments;
};

struct f2fs_sb_info {
	struct f2fs_fsck *fsck;

	struct f2fs_super_block *raw_super;
	struct f2fs_nm_info *nm_info;
	struct f2fs_sm_info *sm_info;
	struct f2fs_checkpoint *ckpt;

	struct list_head orphan_inode_list;
	unsigned int n_orphans;

	/* basic file system units */
	unsigned int log_sectors_per_block;     /* log2 sectors per block */
	unsigned int log_blocksize;             /* log2 block size */
	unsigned int blocksize;                 /* block size */
	unsigned int root_ino_num;              /* root inode number*/
	unsigned int node_ino_num;              /* node inode number*/
	unsigned int meta_ino_num;              /* meta inode number*/
	unsigned int log_blocks_per_seg;        /* log2 blocks per segment */
	unsigned int blocks_per_seg;            /* blocks per segment */
	unsigned int segs_per_sec;              /* segments per section */
	unsigned int secs_per_zone;             /* sections per zone */
	unsigned int total_sections;            /* total section count */
	unsigned int total_node_count;          /* total node block count */
	unsigned int total_valid_node_count;    /* valid node block count */
	unsigned int total_valid_inode_count;   /* valid inode count */
	int active_logs;                        /* # of active logs */

	block_t user_block_count;               /* # of user blocks */
	block_t total_valid_block_count;        /* # of valid blocks */
	block_t alloc_valid_block_count;        /* # of allocated blocks */
	block_t last_valid_block_count;         /* for recovery */
	u32 s_next_generation;                  /* for NFS support */

	unsigned int cur_victim_sec;            /* current victim section num */

};

static inline struct f2fs_super_block *F2FS_RAW_SUPER(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_super_block *)(sbi->raw_super);
}

static inline struct f2fs_checkpoint *F2FS_CKPT(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_checkpoint *)(sbi->ckpt);
}

static inline struct f2fs_fsck *F2FS_FSCK(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_fsck *)(sbi->fsck);
}

static inline struct f2fs_nm_info *NM_I(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_nm_info *)(sbi->nm_info);
}

static inline struct f2fs_sm_info *SM_I(struct f2fs_sb_info *sbi)
{
	return (struct f2fs_sm_info *)(sbi->sm_info);
}

static inline struct sit_info *SIT_I(struct f2fs_sb_info *sbi)
{
	return (struct sit_info *)(SM_I(sbi)->sit_info);
}

static inline unsigned long __bitmap_size(struct f2fs_sb_info *sbi, int flag)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);

	/* return NAT or SIT bitmap */
	if (flag == NAT_BITMAP)
		return le32_to_cpu(ckpt->nat_ver_bitmap_bytesize);
	else if (flag == SIT_BITMAP)
		return le32_to_cpu(ckpt->sit_ver_bitmap_bytesize);

	return 0;
}

static inline void *__bitmap_ptr(struct f2fs_sb_info *sbi, int flag)
{
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	int offset = (flag == NAT_BITMAP) ?
		le32_to_cpu(ckpt->sit_ver_bitmap_bytesize) : 0;
	return &ckpt->sit_nat_version_bitmap + offset;
}

static inline bool is_set_ckpt_flags(struct f2fs_checkpoint *cp, unsigned int f)
{
	unsigned int ckpt_flags = le32_to_cpu(cp->ckpt_flags);
	return ckpt_flags & f;
}

static inline block_t __start_cp_addr(struct f2fs_sb_info *sbi)
{
	block_t start_addr;
	struct f2fs_checkpoint *ckpt = F2FS_CKPT(sbi);
	unsigned long long ckpt_version = le64_to_cpu(ckpt->checkpoint_ver);

	start_addr = le32_to_cpu(F2FS_RAW_SUPER(sbi)->cp_blkaddr);

	/*
	 * odd numbered checkpoint should at cp segment 0
	 * and even segent must be at cp segment 1
	 */
	if (!(ckpt_version & 1))
		start_addr += sbi->blocks_per_seg;

	return start_addr;
}

static inline block_t __start_sum_addr(struct f2fs_sb_info *sbi)
{
	return le32_to_cpu(F2FS_CKPT(sbi)->cp_pack_start_sum);
}

#define GET_ZONENO_FROM_SEGNO(sbi, segno)                               \
	((segno / sbi->segs_per_sec) / sbi->secs_per_zone)

#define IS_DATASEG(t)                                                   \
	((t == CURSEG_HOT_DATA) || (t == CURSEG_COLD_DATA) ||           \
	 (t == CURSEG_WARM_DATA))

#define IS_NODESEG(t)                                                   \
	((t == CURSEG_HOT_NODE) || (t == CURSEG_COLD_NODE) ||           \
	 (t == CURSEG_WARM_NODE))

#define GET_SUM_BLKADDR(sbi, segno)					\
	((sbi->sm_info->ssa_blkaddr) + segno)

#define GET_SEGOFF_FROM_SEG0(sbi, blk_addr)				\
	((blk_addr) - SM_I(sbi)->seg0_blkaddr)

#define GET_SEGNO_FROM_SEG0(sbi, blk_addr)				\
	(GET_SEGOFF_FROM_SEG0(sbi, blk_addr) >> sbi->log_blocks_per_seg)

#define FREE_I_START_SEGNO(sbi)		GET_SEGNO_FROM_SEG0(sbi, SM_I(sbi)->main_blkaddr)
#define GET_R2L_SEGNO(sbi, segno)	(segno + FREE_I_START_SEGNO(sbi))

#define START_BLOCK(sbi, segno)	(SM_I(sbi)->main_blkaddr + (segno << sbi->log_blocks_per_seg))

static inline struct curseg_info *CURSEG_I(struct f2fs_sb_info *sbi, int type)
{
	return (struct curseg_info *)(SM_I(sbi)->curseg_array + type);
}

static inline block_t start_sum_block(struct f2fs_sb_info *sbi)
{
	return __start_cp_addr(sbi) + le32_to_cpu(F2FS_CKPT(sbi)->cp_pack_start_sum);
}

static inline block_t sum_blk_addr(struct f2fs_sb_info *sbi, int base, int type)
{
	return __start_cp_addr(sbi) + le32_to_cpu(F2FS_CKPT(sbi)->cp_pack_total_block_count)
		- (base + 1) + type;
}


#define nats_in_cursum(sum)             (le16_to_cpu(sum->n_nats))
#define sits_in_cursum(sum)             (le16_to_cpu(sum->n_sits))

#define nat_in_journal(sum, i)          (sum->nat_j.entries[i].ne)
#define nid_in_journal(sum, i)          (sum->nat_j.entries[i].nid)
#define sit_in_journal(sum, i)          (sum->sit_j.entries[i].se)
#define segno_in_journal(sum, i)        (sum->sit_j.entries[i].segno)

#define SIT_ENTRY_OFFSET(sit_i, segno)                                  \
	(segno % sit_i->sents_per_block)
#define SIT_BLOCK_OFFSET(sit_i, segno)                                  \
	(segno / SIT_ENTRY_PER_BLOCK)
#define TOTAL_SEGS(sbi) (SM_I(sbi)->main_segments)

#define IS_VALID_NID(sbi, nid) 			\
	do {						\
		ASSERT(nid <= (NAT_ENTRY_PER_BLOCK *	\
					F2FS_RAW_SUPER(sbi)->segment_count_nat	\
					<< (sbi->log_blocks_per_seg - 1)));	\
	} while (0);

#define IS_VALID_BLK_ADDR(sbi, addr)				\
	do {							\
		if (addr >= F2FS_RAW_SUPER(sbi)->block_count ||	 	\
				addr < SM_I(sbi)->main_blkaddr)	\
		{						\
			DBG(0, "block addr [0x%x]\n", addr);	\
			ASSERT(addr <  F2FS_RAW_SUPER(sbi)->block_count);	\
			ASSERT(addr >= SM_I(sbi)->main_blkaddr);	\
		}						\
	} while (0);

static inline u64 BLKOFF_FROM_MAIN(struct f2fs_sb_info *sbi, u64 blk_addr)
{
	ASSERT(blk_addr >= SM_I(sbi)->main_blkaddr);
	return blk_addr - SM_I(sbi)->main_blkaddr;
}

static inline u32 GET_SEGNO(struct f2fs_sb_info *sbi, u64 blk_addr)
{
	return (u32)(BLKOFF_FROM_MAIN(sbi, blk_addr)
			>> sbi->log_blocks_per_seg);
}

static inline u32 OFFSET_IN_SEG(struct f2fs_sb_info *sbi, u64 blk_addr)
{
	return (u32)(BLKOFF_FROM_MAIN(sbi, blk_addr)
			% (1 << sbi->log_blocks_per_seg));
}

static inline void node_info_from_raw_nat(struct node_info *ni,
		struct f2fs_nat_entry *raw_nat)
{
	ni->ino = le32_to_cpu(raw_nat->ino);
	ni->blk_addr = le32_to_cpu(raw_nat->block_addr);
	ni->version = raw_nat->version;
}

extern int lookup_nat_in_journal(struct f2fs_sb_info *sbi, u32 nid, struct f2fs_nat_entry *ne);
#define IS_SUM_NODE_SEG(footer)		(footer.entry_type == SUM_TYPE_NODE)

#endif /* _F2FS_H_ */
