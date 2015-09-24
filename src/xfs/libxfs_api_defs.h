/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __LIBXFS_API_DEFS_H__
#define __LIBXFS_API_DEFS_H__

/*
 * This file defines all the kernel based functions we expose to userspace
 * via the libxfs_* namespace. This is kept in a separate header file so
 * it can be included in both the internal and external libxfs header files
 * without introducing any depenencies between the two.
 */
#define xfs_highbit32			libxfs_highbit32
#define xfs_highbit64			libxfs_highbit64

#define xfs_fs_repair_cmn_err		libxfs_fs_repair_cmn_err
#define xfs_fs_cmn_err			libxfs_fs_cmn_err

#define xfs_trans_alloc			libxfs_trans_alloc
#define xfs_trans_add_item		libxfs_trans_add_item
#define xfs_trans_bhold			libxfs_trans_bhold
#define xfs_trans_binval		libxfs_trans_binval
#define xfs_trans_bjoin			libxfs_trans_bjoin
#define xfs_trans_brelse		libxfs_trans_brelse
#define xfs_trans_commit		libxfs_trans_commit
#define xfs_trans_cancel		libxfs_trans_cancel
#define xfs_trans_del_item		libxfs_trans_del_item
#define xfs_trans_get_buf		libxfs_trans_get_buf
#define xfs_trans_getsb			libxfs_trans_getsb
#define xfs_trans_iget			libxfs_trans_iget
#define xfs_trans_ichgtime		libxfs_trans_ichgtime
#define xfs_trans_ijoin			libxfs_trans_ijoin
#define xfs_trans_ijoin_ref		libxfs_trans_ijoin_ref
#define xfs_trans_init			libxfs_trans_init
#define xfs_trans_inode_alloc_buf	libxfs_trans_inode_alloc_buf
#define xfs_trans_log_buf		libxfs_trans_log_buf
#define xfs_trans_log_inode		libxfs_trans_log_inode
#define xfs_trans_mod_sb		libxfs_trans_mod_sb
#define xfs_trans_read_buf		libxfs_trans_read_buf
#define xfs_trans_read_buf_map		libxfs_trans_read_buf_map
#define xfs_trans_roll			libxfs_trans_roll
#define xfs_trans_get_buf_map		libxfs_trans_get_buf_map
#define xfs_trans_reserve		libxfs_trans_reserve
#define xfs_trans_resv_calc		libxfs_trans_resv_calc

#define xfs_attr_get			libxfs_attr_get
#define xfs_attr_set			libxfs_attr_set
#define xfs_attr_remove			libxfs_attr_remove
#define xfs_attr_leaf_newentsize	libxfs_attr_leaf_newentsize

#define xfs_alloc_fix_freelist		libxfs_alloc_fix_freelist
#define xfs_bmap_cancel			libxfs_bmap_cancel
#define xfs_bmap_last_offset		libxfs_bmap_last_offset
#define xfs_bmap_search_extents		libxfs_bmap_search_extents
#define xfs_bmap_finish			libxfs_bmap_finish
#define xfs_bmapi_write			libxfs_bmapi_write
#define xfs_bmapi_read			libxfs_bmapi_read
#define xfs_bunmapi			libxfs_bunmapi
#define xfs_bmbt_get_all		libxfs_bmbt_get_all
#define xfs_rtfree_extent		libxfs_rtfree_extent

#define xfs_da_brelse			libxfs_da_brelse
#define xfs_da_hashname			libxfs_da_hashname
#define xfs_da_shrink_inode		libxfs_da_shrink_inode
#define xfs_da_read_buf			libxfs_da_read_buf
#define xfs_dir_createname		libxfs_dir_createname
#define xfs_dir_init			libxfs_dir_init
#define xfs_dir_lookup			libxfs_dir_lookup
#define xfs_dir_replace			libxfs_dir_replace
#define xfs_dir2_isblock		libxfs_dir2_isblock
#define xfs_dir2_isleaf			libxfs_dir2_isleaf
#define __xfs_dir2_data_freescan	libxfs_dir2_data_freescan
#define xfs_dir2_data_log_entry		libxfs_dir2_data_log_entry
#define xfs_dir2_data_log_header	libxfs_dir2_data_log_header
#define xfs_dir2_data_make_free		libxfs_dir2_data_make_free
#define xfs_dir2_data_use_free		libxfs_dir2_data_use_free
#define xfs_dir2_shrink_inode		libxfs_dir2_shrink_inode

#define xfs_dinode_from_disk		libxfs_dinode_from_disk
#define xfs_dinode_to_disk		libxfs_dinode_to_disk
#define xfs_dinode_calc_crc		libxfs_dinode_calc_crc
#define xfs_idata_realloc		libxfs_idata_realloc
#define xfs_idestroy_fork		libxfs_idestroy_fork

#define xfs_log_sb			libxfs_log_sb
#define xfs_sb_from_disk		libxfs_sb_from_disk
#define xfs_sb_quota_from_disk		libxfs_sb_quota_from_disk
#define xfs_sb_to_disk			libxfs_sb_to_disk

#define xfs_symlink_blocks		libxfs_symlink_blocks
#define xfs_symlink_hdr_ok		libxfs_symlink_hdr_ok

#define xfs_verify_cksum		libxfs_verify_cksum

#endif /* __LIBXFS_API_DEFS_H__ */
