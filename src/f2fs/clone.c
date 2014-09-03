/**
 * main.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include "fsck.h"
#include <libgen.h>

struct f2fs_fsck gfsck = {
    .sbi.fsck = &gfsck,
};
extern struct f2fs_configuration config;
void dump_sit(struct f2fs_sb_info *sbi)
{
    DBG(0, "start dump sit\n");
    struct f2fs_fsck *fsck = F2FS_FSCK(sbi);
    DBG(0, "start fsck sit\n");
    u32 blk = 0;
    for ( blk = 4096 ; blk <= 131071 ; blk++ ){
	DBG(0, "blk = %i\n", blk);
	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, blk), fsck->sit_area_bitmap) == 0x0) {
	    DBG(0, "test SIT bitmap is 0x0. blk_addr[0x%x] %i\n", blk, blk);
	}else{
	    DBG(0, "test SIT bitmap is 0x1. blk_addr[0x%x] %i\n", blk, blk);

	}
	if (f2fs_test_bit(BLKOFF_FROM_MAIN(sbi, blk), fsck->main_area_bitmap) == 0x0) {
	    DBG(0, "test main bitmap is 0x0. blk_addr[0x%x] %i\n", blk, blk);
	}else{
	    DBG(0, "test main bitmap is 0x1. blk_addr[0x%x] %i\n", blk, blk);

	}
    }	


}

int do_fsck(struct f2fs_sb_info *sbi)
{
    int ret;

    ret = fsck_init(sbi);
    if (ret < 0)
	return ret;

    fsck_chk_orphan_node(sbi);

    struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);
    struct f2fs_checkpoint *cp = F2FS_CKPT(sbi);
    DBG(0, "display sb\n");
    DBG(0, "sb block_count %i\n", sb->block_count);
    DBG(0, "sb root ino %i\n", sb->root_ino);
    DBG(0, "sb block_size %i\n", F2FS_BLKSIZE);
    DBG(0, "device size %i\n",  config.total_sectors*config.sector_size);
    DBG(0, "free_seg %i\n", cp->free_segment_count);
    DBG(0, "user_block_count %i\n", cp->user_block_count);
    DBG(0, "segment_count %i\n", sb->segment_count);
    DBG(0, "free_segment_count %i\n", cp->free_segment_count);
    DBG(0, "DEFAULT_BLOCKS_PER_SEGMENT %i\n", DEFAULT_BLOCKS_PER_SEGMENT);

    dump_sit(sbi);
    //ret = fsck_verify(sbi);
    fsck_free(sbi);
    return ret;
}
int main (int argc, char **argv)
{
    struct f2fs_sb_info *sbi = &gfsck.sbi;
    int ret = 0;

    f2fs_init_configuration(&config);
    config.device_name = argv[optind];


    if (f2fs_dev_is_umounted(&config) < 0)
	return -1;

    /* Get device */
    if (f2fs_get_device_info(&config) < 0)
	return -1;

    if (f2fs_do_mount(sbi) < 0)
	return -1;




    ret = do_fsck(sbi);

    f2fs_do_umount(sbi);
    printf("\nDone.\n");
    return ret;
}
