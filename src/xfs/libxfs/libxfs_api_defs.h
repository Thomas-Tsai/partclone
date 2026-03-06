// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#ifndef __LIBXFS_API_DEFS_H__
#define __LIBXFS_API_DEFS_H__

/*
 * This file defines all the kernel based functions we expose to userspace
 * via the libxfs_* namespace. This is kept in a separate header file so
 * it can be included in both the internal and external libxfs header files
 * without introducing any depenencies between the two.
 */
#define LIBXFS_ATTR_ROOT		XFS_ATTR_ROOT
#define LIBXFS_ATTR_SECURE		XFS_ATTR_SECURE
#define LIBXFS_ATTR_PARENT		XFS_ATTR_PARENT

#define xfs_agfl_size			libxfs_agfl_size
#define xfs_agfl_walk			libxfs_agfl_walk

#define xfs_ag_init_headers		libxfs_ag_init_headers
#define xfs_ag_block_count		libxfs_ag_block_count
#define xfs_ag_resv_init		libxfs_ag_resv_init
#define xfs_ag_resv_free		libxfs_ag_resv_free

#define xfs_alloc_ag_max_usable		libxfs_alloc_ag_max_usable
#define xfs_allocbt_calc_size		libxfs_allocbt_calc_size
#define xfs_allocbt_maxlevels_ondisk	libxfs_allocbt_maxlevels_ondisk
#define xfs_allocbt_maxrecs		libxfs_allocbt_maxrecs
#define xfs_allocbt_stage_cursor	libxfs_allocbt_stage_cursor
#define xfs_alloc_fix_freelist		libxfs_alloc_fix_freelist
#define xfs_alloc_min_freelist		libxfs_alloc_min_freelist
#define xfs_alloc_read_agf		libxfs_alloc_read_agf
#define xfs_alloc_vextent_start_ag	libxfs_alloc_vextent_start_ag

#define xfs_ascii_ci_hashname		libxfs_ascii_ci_hashname

#define xfs_attr3_leaf_hdr_from_disk	libxfs_attr3_leaf_hdr_from_disk
#define xfs_attr3_leaf_read		libxfs_attr3_leaf_read
#define xfs_attr_check_namespace	libxfs_attr_check_namespace
#define xfs_attr_get			libxfs_attr_get
#define xfs_attr_hashname		libxfs_attr_hashname
#define xfs_attr_hashval		libxfs_attr_hashval
#define xfs_attr_is_leaf		libxfs_attr_is_leaf
#define xfs_attr_leaf_newentsize	libxfs_attr_leaf_newentsize
#define xfs_attr_namecheck		libxfs_attr_namecheck
#define xfs_attr_removename		libxfs_attr_removename
#define xfs_attr_set			libxfs_attr_set
#define xfs_attr_sethash		libxfs_attr_sethash
#define xfs_attr_sf_firstentry		libxfs_attr_sf_firstentry
#define xfs_attr_shortform_verify	libxfs_attr_shortform_verify

#define __xfs_bmap_add_free		__libxfs_bmap_add_free
#define xfs_bmap_add_attrfork		libxfs_bmap_add_attrfork
#define xfs_bmap_validate_extent	libxfs_bmap_validate_extent
#define xfs_bmapi_read			libxfs_bmapi_read
#define xfs_bmapi_remap			libxfs_bmapi_remap
#define xfs_bmapi_write			libxfs_bmapi_write
#define xfs_bmap_last_offset		libxfs_bmap_last_offset
#define xfs_bmbt_calc_size		libxfs_bmbt_calc_size
#define xfs_bmbt_commit_staged_btree	libxfs_bmbt_commit_staged_btree
#define xfs_bmbt_disk_get_startoff	libxfs_bmbt_disk_get_startoff
#define xfs_bmbt_disk_set_all		libxfs_bmbt_disk_set_all
#define xfs_bmbt_init_cursor		libxfs_bmbt_init_cursor
#define xfs_bmbt_maxlevels_ondisk	libxfs_bmbt_maxlevels_ondisk
#define xfs_bmbt_maxrecs		libxfs_bmbt_maxrecs
#define xfs_bmbt_stage_cursor		libxfs_bmbt_stage_cursor
#define xfs_bmdr_maxrecs		libxfs_bmdr_maxrecs

#define xfs_bnobt_init_cursor		libxfs_bnobt_init_cursor

#define xfs_btree_bload			libxfs_btree_bload
#define xfs_btree_bload_compute_geometry libxfs_btree_bload_compute_geometry
#define xfs_btree_calc_size		libxfs_btree_calc_size
#define xfs_btree_decrement		libxfs_btree_decrement
#define xfs_btree_del_cursor		libxfs_btree_del_cursor
#define xfs_btree_delete		libxfs_btree_delete
#define xfs_btree_get_block		libxfs_btree_get_block
#define xfs_btree_get_rec		libxfs_btree_get_rec
#define xfs_btree_goto_left_edge	libxfs_btree_goto_left_edge
#define xfs_btree_has_more_records	libxfs_btree_has_more_records
#define xfs_btree_increment		libxfs_btree_increment
#define xfs_btree_init_block		libxfs_btree_init_block
#define xfs_btree_insert		libxfs_btree_insert
#define xfs_btree_lookup		libxfs_btree_lookup
#define xfs_btree_mem_head_nlevels	libxfs_btree_mem_head_nlevels
#define xfs_btree_mem_head_read_buf	libxfs_btree_mem_head_read_buf
#define xfs_btree_memblock_verify	libxfs_btree_memblock_verify
#define xfs_btree_rec_addr		libxfs_btree_rec_addr
#define xfs_btree_update		libxfs_btree_update
#define xfs_btree_space_to_height	libxfs_btree_space_to_height
#define xfs_btree_stage_afakeroot	libxfs_btree_stage_afakeroot
#define xfs_btree_stage_ifakeroot	libxfs_btree_stage_ifakeroot
#define xfs_btree_visit_blocks		libxfs_btree_visit_blocks
#define xfs_buf_delwri_queue		libxfs_buf_delwri_queue
#define xfs_buf_delwri_submit		libxfs_buf_delwri_submit
#define xfs_buf_get			libxfs_buf_get
#define xfs_buf_get_uncached		libxfs_buf_get_uncached
#define xfs_buf_lock			libxfs_buf_lock
#define xfs_buf_read			libxfs_buf_read
#define xfs_buf_read_uncached		libxfs_buf_read_uncached
#define xfs_buf_relse			libxfs_buf_relse
#define xfs_buf_unlock			libxfs_buf_unlock
#define xfs_buftarg_drain		libxfs_buftarg_drain
#define xfs_bunmapi			libxfs_bunmapi
#define xfs_bwrite			libxfs_bwrite
#define xfs_calc_dquots_per_chunk	libxfs_calc_dquots_per_chunk
#define xfs_cntbt_init_cursor		libxfs_cntbt_init_cursor
#define xfs_compute_rextslog		libxfs_compute_rextslog
#define xfs_compute_rgblklog		libxfs_compute_rgblklog
#define xfs_create_space_res		libxfs_create_space_res
#define xfs_da3_node_hdr_from_disk	libxfs_da3_node_hdr_from_disk
#define xfs_da3_node_read		libxfs_da3_node_read
#define xfs_da_get_buf			libxfs_da_get_buf
#define xfs_da_hashname			libxfs_da_hashname
#define xfs_da_read_buf			libxfs_da_read_buf
#define xfs_da_shrink_inode		libxfs_da_shrink_inode
#define xfs_defer_cancel		libxfs_defer_cancel
#define xfs_defer_finish		libxfs_defer_finish
#define xfs_dialloc			libxfs_dialloc
#define xfs_dinode_calc_crc		libxfs_dinode_calc_crc
#define xfs_dinode_good_version		libxfs_dinode_good_version
#define xfs_dinode_verify		libxfs_dinode_verify
#define xfs_dinode_verify_metadir	libxfs_dinode_verify_metadir

#define xfs_dir2_data_bestfree_p	libxfs_dir2_data_bestfree_p
#define xfs_dir2_data_entry_tag_p	libxfs_dir2_data_entry_tag_p
#define xfs_dir2_data_entsize		libxfs_dir2_data_entsize
#define xfs_dir2_data_freescan		libxfs_dir2_data_freescan
#define xfs_dir2_data_get_ftype		libxfs_dir2_data_get_ftype
#define xfs_dir2_data_log_entry		libxfs_dir2_data_log_entry
#define xfs_dir2_data_log_header	libxfs_dir2_data_log_header
#define xfs_dir2_data_make_free		libxfs_dir2_data_make_free
#define xfs_dir2_data_put_ftype		libxfs_dir2_data_put_ftype
#define xfs_dir2_data_use_free		libxfs_dir2_data_use_free
#define xfs_dir2_free_hdr_from_disk	libxfs_dir2_free_hdr_from_disk
#define xfs_dir2_hashname		libxfs_dir2_hashname
#define xfs_dir2_leaf_hdr_from_disk	libxfs_dir2_leaf_hdr_from_disk
#define xfs_dir2_format			libxfs_dir2_format
#define xfs_dir2_namecheck		libxfs_dir2_namecheck
#define xfs_dir2_sf_entsize		libxfs_dir2_sf_entsize
#define xfs_dir2_sf_get_ftype		libxfs_dir2_sf_get_ftype
#define xfs_dir2_sf_get_ino		libxfs_dir2_sf_get_ino
#define xfs_dir2_sf_get_parent_ino	libxfs_dir2_sf_get_parent_ino
#define xfs_dir2_sf_nextentry		libxfs_dir2_sf_nextentry
#define xfs_dir2_sf_put_ftype		libxfs_dir2_sf_put_ftype
#define xfs_dir2_sf_put_ino		libxfs_dir2_sf_put_ino
#define xfs_dir2_sf_put_parent_ino	libxfs_dir2_sf_put_parent_ino
#define xfs_dir2_shrink_inode		libxfs_dir2_shrink_inode

#define xfs_dir_createname		libxfs_dir_createname
#define xfs_dir_create_child		libxfs_dir_create_child
#define xfs_dir_init			libxfs_dir_init
#define xfs_dir_ino_validate		libxfs_dir_ino_validate
#define xfs_dir_lookup			libxfs_dir_lookup
#define xfs_dir_removename		libxfs_dir_removename
#define xfs_dir_replace			libxfs_dir_replace

#define xfs_dqblk_repair		libxfs_dqblk_repair
#define xfs_dqinode_load		libxfs_dqinode_load
#define xfs_dqinode_load_parent		libxfs_dqinode_load_parent
#define xfs_dqinode_mkdir_parent	libxfs_dqinode_mkdir_parent
#define xfs_dqinode_metadir_create	libxfs_dqinode_metadir_create
#define xfs_dqinode_metadir_link	libxfs_dqinode_metadir_link
#define xfs_dqinode_path		libxfs_dqinode_path
#define xfs_dquot_from_disk_ts		libxfs_dquot_from_disk_ts
#define xfs_dquot_verify		libxfs_dquot_verify

#define xfs_bumplink			libxfs_bumplink
#define xfs_droplink			libxfs_droplink

#define xfs_finobt_calc_reserves	libxfs_finobt_calc_reserves
#define xfs_finobt_init_cursor		libxfs_finobt_init_cursor
#define xfs_free_extent			libxfs_free_extent
#define xfs_free_extent_later		libxfs_free_extent_later
#define xfs_free_perag_range		libxfs_free_perag_range
#define xfs_free_rtgroups		libxfs_free_rtgroups
#define xfs_fs_geometry			libxfs_fs_geometry
#define xfs_gbno_to_fsb			libxfs_gbno_to_fsb
#define xfs_get_initial_prid		libxfs_get_initial_prid
#define xfs_highbit32			libxfs_highbit32
#define xfs_highbit64			libxfs_highbit64
#define xfs_ialloc_calc_rootino		libxfs_ialloc_calc_rootino
#define xfs_iallocbt_calc_size		libxfs_iallocbt_calc_size
#define xfs_iallocbt_maxlevels_ondisk	libxfs_iallocbt_maxlevels_ondisk
#define xfs_ialloc_read_agi		libxfs_ialloc_read_agi
#define xfs_icreate			libxfs_icreate
#define xfs_idata_realloc		libxfs_idata_realloc
#define xfs_idestroy_fork		libxfs_idestroy_fork
#define xfs_iext_first			libxfs_iext_first
#define xfs_iext_insert_raw		libxfs_iext_insert_raw
#define xfs_iext_lookup_extent		libxfs_iext_lookup_extent
#define xfs_iext_next			libxfs_iext_next
#define xfs_ifork_zap_attr		libxfs_ifork_zap_attr
#define xfs_imap_to_bp			libxfs_imap_to_bp

#define xfs_initialize_perag		libxfs_initialize_perag
#define xfs_initialize_perag_data	libxfs_initialize_perag_data
#define xfs_initialize_rtgroups		libxfs_initialize_rtgroups
#define xfs_init_local_fork		libxfs_init_local_fork

#define xfs_inobt_init_cursor		libxfs_inobt_init_cursor
#define xfs_inobt_maxrecs		libxfs_inobt_maxrecs
#define xfs_inobt_stage_cursor		libxfs_inobt_stage_cursor
#define xfs_inode_from_disk		libxfs_inode_from_disk
#define xfs_inode_from_disk_ts		libxfs_inode_from_disk_ts
#define xfs_inode_hasattr		libxfs_inode_hasattr
#define xfs_inode_init			libxfs_inode_init
#define xfs_inode_to_disk		libxfs_inode_to_disk
#define xfs_inode_validate_cowextsize	libxfs_inode_validate_cowextsize
#define xfs_inode_validate_extsize	libxfs_inode_validate_extsize

#define xfs_is_sb_inum			libxfs_is_sb_inum

#define xfs_iread_extents		libxfs_iread_extents
#define xfs_irele			libxfs_irele
#define xfs_iunlink			libxfs_iunlink
#define xfs_link_space_res		libxfs_link_space_res
#define xfs_log_calc_minimum_size	libxfs_log_calc_minimum_size
#define xfs_log_get_max_trans_res	libxfs_log_get_max_trans_res
#define xfs_log_sb			libxfs_log_sb

#define xfs_metafile_iget		libxfs_metafile_iget
#define xfs_trans_metafile_iget		libxfs_trans_metafile_iget
#define xfs_metafile_set_iflag		libxfs_metafile_set_iflag
#define xfs_metadir_cancel		libxfs_metadir_cancel
#define xfs_metadir_commit		libxfs_metadir_commit
#define xfs_metadir_link		libxfs_metadir_link
#define xfs_metadir_lookup		libxfs_metadir_lookup
#define xfs_metadir_start_create	libxfs_metadir_start_create
#define xfs_metadir_start_link		libxfs_metadir_start_link

#define xfs_mode_to_ftype		libxfs_mode_to_ftype
#define xfs_mkdir_space_res		libxfs_mkdir_space_res

#define xfs_parent_addname		libxfs_parent_addname
#define xfs_parent_finish		libxfs_parent_finish
#define xfs_parent_hashval		libxfs_parent_hashval
#define xfs_parent_lookup		libxfs_parent_lookup
#define xfs_parent_removename		libxfs_parent_removename
#define xfs_parent_set			libxfs_parent_set
#define xfs_parent_start		libxfs_parent_start
#define xfs_parent_from_attr		libxfs_parent_from_attr
#define xfs_parent_unset		libxfs_parent_unset
#define xfs_perag_get			libxfs_perag_get
#define xfs_perag_hold			libxfs_perag_hold
#define xfs_perag_put			libxfs_perag_put
#define xfs_prealloc_blocks		libxfs_prealloc_blocks

#define xfs_read_agf			libxfs_read_agf
#define xfs_read_agi			libxfs_read_agi
#define xfs_refc_block			libxfs_refc_block
#define xfs_refcountbt_calc_reserves	libxfs_refcountbt_calc_reserves
#define xfs_refcountbt_calc_size	libxfs_refcountbt_calc_size
#define xfs_refcountbt_init_cursor	libxfs_refcountbt_init_cursor
#define xfs_refcountbt_maxlevels_ondisk	libxfs_refcountbt_maxlevels_ondisk
#define xfs_refcountbt_maxrecs		libxfs_refcountbt_maxrecs
#define xfs_refcountbt_stage_cursor	libxfs_refcountbt_stage_cursor
#define xfs_refcount_get_rec		libxfs_refcount_get_rec
#define xfs_refcount_lookup_le		libxfs_refcount_lookup_le
#define xfs_remove_space_res		libxfs_remove_space_res

#define xfs_rmap_alloc			libxfs_rmap_alloc
#define xfs_rmapbt_calc_reserves	libxfs_rmapbt_calc_reserves
#define xfs_rmapbt_calc_size		libxfs_rmapbt_calc_size
#define xfs_rmapbt_init_cursor		libxfs_rmapbt_init_cursor
#define xfs_rmapbt_maxlevels_ondisk	libxfs_rmapbt_maxlevels_ondisk
#define xfs_rmapbt_maxrecs		libxfs_rmapbt_maxrecs
#define xfs_rmapbt_mem_init		libxfs_rmapbt_mem_init
#define xfs_rmapbt_mem_cursor		libxfs_rmapbt_mem_cursor
#define xfs_rmapbt_stage_cursor		libxfs_rmapbt_stage_cursor
#define xfs_rmap_compare		libxfs_rmap_compare
#define xfs_rmap_get_rec		libxfs_rmap_get_rec
#define xfs_rmap_ino_bmbt_owner		libxfs_rmap_ino_bmbt_owner
#define xfs_rmap_irec_offset_pack	libxfs_rmap_irec_offset_pack
#define xfs_rmap_irec_offset_unpack	libxfs_rmap_irec_offset_unpack
#define xfs_rmap_lookup_le		libxfs_rmap_lookup_le
#define xfs_rmap_lookup_le_range	libxfs_rmap_lookup_le_range
#define xfs_rmap_map_raw		libxfs_rmap_map_raw
#define xfs_rmap_query_all		libxfs_rmap_query_all
#define xfs_rmap_query_range		libxfs_rmap_query_range

#define xfs_rtbitmap_create		libxfs_rtbitmap_create
#define xfs_rtbitmap_getword		libxfs_rtbitmap_getword
#define xfs_rtbitmap_setword		libxfs_rtbitmap_setword
#define xfs_rtbitmap_wordcount		libxfs_rtbitmap_wordcount
#define xfs_rtginode_create		libxfs_rtginode_create
#define xfs_rtginode_irele		libxfs_rtginode_irele
#define xfs_rtginode_load		libxfs_rtginode_load
#define xfs_rtginode_load_parent	libxfs_rtginode_load_parent
#define xfs_rtginode_metafile_type	libxfs_rtginode_metafile_type
#define xfs_rtginode_mkdir_parent	libxfs_rtginode_mkdir_parent
#define xfs_rtginode_name		libxfs_rtginode_name
#define xfs_rtsummary_create		libxfs_rtsummary_create

#define xfs_rtgroup_alloc		libxfs_rtgroup_alloc
#define xfs_rtgroup_extents		libxfs_rtgroup_extents
#define xfs_rtgroup_grab		libxfs_rtgroup_grab
#define xfs_rtgroup_rele		libxfs_rtgroup_rele

#define xfs_suminfo_add			libxfs_suminfo_add
#define xfs_suminfo_get			libxfs_suminfo_get
#define xfs_rtsummary_wordcount		libxfs_rtsummary_wordcount
#define xfs_rtfile_initialize_blocks	libxfs_rtfile_initialize_blocks

#define xfs_rtfree_extent		libxfs_rtfree_extent
#define xfs_rtfree_blocks		libxfs_rtfree_blocks
#define xfs_update_rtsb			libxfs_update_rtsb
#define xfs_sb_from_disk		libxfs_sb_from_disk
#define xfs_sb_mount_rextsize		libxfs_sb_mount_rextsize
#define xfs_sb_quota_from_disk		libxfs_sb_quota_from_disk
#define xfs_sb_read_secondary		libxfs_sb_read_secondary
#define xfs_sb_to_disk			libxfs_sb_to_disk
#define xfs_sb_version_to_features	libxfs_sb_version_to_features
#define xfs_symlink_blocks		libxfs_symlink_blocks
#define xfs_symlink_hdr_ok		libxfs_symlink_hdr_ok
#define xfs_symlink_write_target	libxfs_symlink_write_target

#define xfs_trans_add_item		libxfs_trans_add_item
#define xfs_trans_alloc_empty		libxfs_trans_alloc_empty
#define xfs_trans_alloc			libxfs_trans_alloc
#define xfs_trans_alloc_dir		libxfs_trans_alloc_dir
#define xfs_trans_alloc_inode		libxfs_trans_alloc_inode
#define xfs_trans_bdetach		libxfs_trans_bdetach
#define xfs_trans_bhold			libxfs_trans_bhold
#define xfs_trans_bhold_release		libxfs_trans_bhold_release
#define xfs_trans_binval		libxfs_trans_binval
#define xfs_trans_bjoin			libxfs_trans_bjoin
#define xfs_trans_brelse		libxfs_trans_brelse
#define xfs_trans_cancel		libxfs_trans_cancel
#define xfs_trans_commit		libxfs_trans_commit
#define xfs_trans_del_item		libxfs_trans_del_item
#define xfs_trans_dirty_buf		libxfs_trans_dirty_buf
#define xfs_trans_get_buf		libxfs_trans_get_buf
#define xfs_trans_get_buf_map		libxfs_trans_get_buf_map
#define xfs_trans_getrtsb		libxfs_trans_getrtsb
#define xfs_trans_getsb			libxfs_trans_getsb
#define xfs_trans_ichgtime		libxfs_trans_ichgtime
#define xfs_trans_ijoin			libxfs_trans_ijoin
#define xfs_trans_init			libxfs_trans_init
#define xfs_trans_inode_alloc_buf	libxfs_trans_inode_alloc_buf
#define xfs_trans_log_buf		libxfs_trans_log_buf
#define xfs_trans_log_inode		libxfs_trans_log_inode
#define xfs_trans_mod_sb		libxfs_trans_mod_sb
#define xfs_trans_ordered_buf		libxfs_trans_ordered_buf
#define xfs_trans_read_buf		libxfs_trans_read_buf
#define xfs_trans_read_buf_map		libxfs_trans_read_buf_map
#define xfs_trans_resv_calc		libxfs_trans_resv_calc
#define xfs_trans_roll_inode		libxfs_trans_roll_inode
#define xfs_trans_roll			libxfs_trans_roll
#define xfs_trim_extent			libxfs_trim_extent

#define xfs_update_secondary_sbs	libxfs_update_secondary_sbs

#define xfs_validate_rt_geometry	libxfs_validate_rt_geometry
#define xfs_validate_stripe_geometry	libxfs_validate_stripe_geometry
#define xfs_verify_agbno		libxfs_verify_agbno
#define xfs_verify_agbext		libxfs_verify_agbext
#define xfs_verify_agino		libxfs_verify_agino
#define xfs_verify_cksum		libxfs_verify_cksum
#define xfs_verify_dir_ino		libxfs_verify_dir_ino
#define xfs_verify_fsbext		libxfs_verify_fsbext
#define xfs_verify_fsbno		libxfs_verify_fsbno
#define xfs_verify_ino			libxfs_verify_ino
#define xfs_verify_rtbno		libxfs_verify_rtbno
#define xfs_zero_extent			libxfs_zero_extent

/* Please keep this list alphabetized. */

#endif /* __LIBXFS_API_DEFS_H__ */
