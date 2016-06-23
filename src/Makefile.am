AUTOMAKE_OPTIONS = subdir-objects 
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -D_FILE_OFFSET_BITS=64
LDADD = $(LIBINTL)
sbin_PROGRAMS=partclone.info partclone.dd partclone.restore partclone.chkimg partclone.imager
TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox

BTRFS_SOURCE=\
btrfs/bitops.h btrfs/ctree.h btrfs/extent_io.c btrfs/inode-item.c btrfs/math.h btrfs/radix-tree.c btrfs/root-tree.c btrfs/ulist.c btrfs/volumes.c\
btrfs/btrfsck.h btrfs/dir-item.c btrfs/extent_io.h btrfs/inode-map.c btrfs/print-tree.c btrfs/radix-tree.h btrfs/send.h btrfs/ulist.h btrfs/volumes.h\
btrfs/btrfs-list.c btrfs/disk-io.c btrfs/extent-tree.c btrfs/ioctl.h btrfs/print-tree.h btrfs/raid6.c btrfs/send-stream.c btrfs/utils.c\
btrfs/btrfs-list.h btrfs/disk-io.h btrfs/file-item.c btrfs/kerncompat.h btrfs/qgroup.c btrfs/rbtree.c btrfs/send-stream.h btrfs/utils.h\
btrfs/crc32c.c btrfs/free-space-cache.c btrfs/list.h btrfs/qgroup.h btrfs/rbtree.h btrfs/send-utils.c btrfs/utils-lib.c btrfs/rbtree-utils.c\
btrfs/crc32c.h btrfs/extent-cache.c btrfs/free-space-cache.h btrfs/list_sort.c btrfs/qgroup-verify.c btrfs/repair.c btrfs/send-utils.h\
btrfs/uuid-tree.c btrfs/ctree.c btrfs/extent-cache.h btrfs/hash.h btrfs/list_sort.h btrfs/qgroup-verify.h btrfs/repair.h btrfs/transaction.h\
btrfs/version.h btrfs/rbtree-utils.h btrfs/rbtree_augmented.h btrfs/internal.h btrfs/commands.h

XFS_SOURCE=\
xfs/atomic.h xfs/libxfs_priv.h xfs/xfs_arch.h xfs/xfs_da_format.h xfs/xfs_inode_fork.h xfs/bitops.h xfs/libxlog.h xfs/xfs_attr.c xfs/xfs_dir2_block.c xfs/xfs_inode.h xfs/cache.c xfs/linux.c xfs/xfs_attr_leaf.c xfs/xfs_dir2.c xfs/xfs_log_format.h xfs/cache.h xfs/linux.h xfs/xfs_attr_leaf.h xfs/xfs_dir2_data.c xfs/xfs_log_recover.h xfs/command.h xfs/list.h xfs/xfs_attr_remote.c xfs/xfs_dir2.h xfs/xfs_log_rlimit.c xfs/crc32.c xfs/logitem.c xfs/xfs_attr_remote.h xfs/xfs_dir2_leaf.c xfs/xfs_metadump.h xfs/crc32defs.h xfs/parent.h xfs/xfs_attr_sf.h xfs/xfs_dir2_node.c xfs/xfs_mount.h xfs/path.h xfs/xfs_bit.c xfs/xfs_dir2_priv.h xfs/xfs_quota_defs.h  xfs/platform_defs.h xfs/xfs_bit.h xfs/xfs_dir2_sf.c xfs/xfs_rtbitmap.c xfs/handle.h xfs/platform_defs.h.in xfs/xfs_bmap_btree.c xfs/xfs_dquot_buf.c xfs/xfs_sb.c xfs/hlist.h xfs/project.h xfs/xfs_bmap_btree.h xfs/xfs_format.h xfs/xfs_sb.h xfs/init.c xfs/radix-tree.c xfs/xfs_bmap.c xfs/xfs_fs.h xfs/xfs_shared.h xfs/init.h xfs/radix-tree.h xfs/xfs_bmap.h xfs/xfs.h xfs/xfs_symlink_remote.c xfs/input.h xfs/rdwr.c xfs/xfs_btree.c xfs/xfs_ialloc_btree.c xfs/xfs_trace.h xfs/jdm.h xfs/trans.c xfs/xfs_btree.h xfs/xfs_ialloc_btree.h xfs/xfs_trans.h xfs/kmem.c xfs/util.c xfs/xfs_btree_trace.h xfs/xfs_ialloc.c xfs/xfs_trans_resv.c xfs/kmem.h xfs/xfs_alloc_btree.c xfs/xfs_cksum.h xfs/xfs_ialloc.h xfs/xfs_trans_resv.h xfs/libxfs_api_defs.h xfs/xfs_alloc_btree.h xfs/xfs_da_btree.c xfs/xfs_inode_buf.c xfs/xfs_trans_space.h xfs/libxfs.h xfs/xfs_alloc.c xfs/xfs_da_btree.h xfs/xfs_inode_buf.h xfs/xfs_types.h xfs/libxfs_io.h xfs/xfs_alloc.h xfs/xfs_da_format.c xfs/xfs_inode_fork.c xfs/xqm.h xfs/crc32table.h

EXFATFS_SOURCE=exfat/cluster.c exfat/utf.c exfat/utils.c exfat/lookup.c exfat/io.c exfat/log.c exfat/node.c exfat/mount.c exfat/time.c

F2FS_SOURCE=f2fs/fsck.c f2fs/libf2fs.c f2fs/fsck.h f2fs/mount.c f2fs/f2fs_fs.h f2fs/list.h f2fs/f2fs.h

if ENABLE_STATIC
AM_LDFLAGS=-static
endif

if ENABLE_TINFO
LIBS+=-ltinfo
endif

AM_CFLAGS=-D_FILE_OFFSET_BITS=64

if ENABLE_MEMTRACE
AM_CFLAGS+=-DMEMTRACE -DMEMWATCH -DMW_STDIO
endif

if ENABLE_NCURSESW
AM_CFLAGS+=-I/usr/include/ncursesw
endif

version.h: FORCE
	$(TOOLBOX) --update-version

partclone_info_SOURCES=info.c partclone.c partclone.h fs_common.h

partclone_restore_SOURCES=main.c partclone.c progress.c ddclone.c ddclone.h partclone.h progress.h gettext.h
partclone_restore_CFLAGS=-DRESTORE -DDD

partclone_chkimg_SOURCES=main.c partclone.c progress.c ddclone.c ddclone.h partclone.h progress.h gettext.h
partclone_chkimg_CFLAGS=-DCHKIMG -DDD

partclone_dd_SOURCES=main.c partclone.c progress.c ddclone.c ddclone.h partclone.h progress.h gettext.h
partclone_dd_CFLAGS=-DDD

partclone_imager_SOURCES=main.c partclone.c progress.c ddclone.c ddclone.h partclone.h progress.h gettext.h
partclone_imager_CFLAGS=-DIMG

if ENABLE_EXTFS
sbin_PROGRAMS += partclone.extfs
partclone_extfs_SOURCES=main.c partclone.c progress.c extfsclone.c extfsclone.h partclone.h progress.h gettext.h
partclone_extfs_CFLAGS=-DEXTFS
partclone_extfs_LDADD=-lext2fs -lcom_err -lpthread
endif

if ENABLE_REISERFS
sbin_PROGRAMS += partclone.reiserfs
partclone_reiserfs_SOURCES=main.c partclone.c progress.c reiserfsclone.c reiserfsclone.h partclone.h progress.h gettext.h
partclone_reiserfs_CFLAGS=-DREISERFS
partclone_reiserfs_LDADD=-lreiserfs -ldal
endif

if ENABLE_REISER4
sbin_PROGRAMS += partclone.reiser4
partclone_reiser4_SOURCES=main.c partclone.c progress.c reiser4clone.c reiser4clone.h partclone.h progress.h gettext.h
partclone_reiser4_CFLAGS=-DREISER4
partclone_reiser4_LDADD=-lreiser4 -laal
endif

if ENABLE_HFSP
sbin_PROGRAMS += partclone.hfsp
partclone_hfsp_SOURCES=main.c partclone.c progress.c hfsplusclone.c hfsplusclone.h partclone.h progress.h gettext.h
partclone_hfsp_CFLAGS=-DHFSPLUS
endif

if ENABLE_XFS
sbin_PROGRAMS += partclone.xfs
partclone_xfs_SOURCES=main.c partclone.c progress.c xfsclone.c xfsclone.h partclone.h progress.h gettext.h $(XFS_SOURCE)
partclone_xfs_CFLAGS=-DXFS -D_GNU_SOURCE -DNDEBUG $(UUID_CFLAGS) 
partclone_xfs_LDADD=-lrt -lpthread -luuid
endif

if ENABLE_EXFAT
sbin_PROGRAMS += partclone.exfat
partclone_exfat_SOURCES=main.c partclone.c progress.c exfatclone.c exfatclone.h partclone.h progress.h gettext.h $(EXFATFS_SOURCE)
partclone_exfat_CFLAGS=-DEXFAT -D_GNU_SOURCE -std=c99
endif

if ENABLE_F2FS
sbin_PROGRAMS += partclone.f2fs
partclone_f2fs_SOURCES=main.c partclone.c progress.c f2fsclone.c f2fsclone.h partclone.h progress.h gettext.h $(F2FS_SOURCE)
partclone_f2fs_CFLAGS=-DF2FS
partclone_f2fs_LDADD=-luuid
endif

if ENABLE_NILFS2
sbin_PROGRAMS += partclone.nilfs2
partclone_nilfs2_SOURCES=main.c partclone.c progress.c nilfsclone.c nilfsclone.h partclone.h progress.h gettext.h
partclone_nilfs2_CFLAGS=-DNILFS
partclone_nilfs2_LDADD=-lnilfs
endif

if ENABLE_FAT
sbin_PROGRAMS += partclone.fat
partclone_fat_SOURCES=main.c partclone.c progress.c fatclone.c fatclone.h partclone.h progress.h gettext.h
partclone_fat_CFLAGS=-DFAT
endif

sbin_PROGRAMS += partclone.ntfsfixboot
partclone_ntfsfixboot_SOURCES=ntfsfixboot.c
if ENABLE_NTFS
sbin_PROGRAMS += partclone.ntfs
partclone_ntfs_SOURCES=main.c partclone.c progress.c ntfsclone-ng.c ntfsclone-ng.h partclone.h progress.h gettext.h
if ENABLE_NTFS_3G
partclone_ntfs_CFLAGS=-DNTFS3G
partclone_ntfs_LDADD=-lntfs-3g
else
partclone_ntfs_CFLAGS=-DNTFS
partclone_ntfs_LDADD=-lntfs
endif
endif

if ENABLE_UFS
sbin_PROGRAMS += partclone.ufs
partclone_ufs_SOURCES=main.c partclone.c progress.c ufsclone.c ufsclone.h partclone.h progress.h gettext.h
partclone_ufs_CFLAGS=-DUFS -D_GNU_SOURCE
partclone_ufs_LDADD=-lufs -lbsd
endif

if ENABLE_VMFS
sbin_PROGRAMS += partclone.vmfs
partclone_vmfs_SOURCES=main.c partclone.c progress.c vmfsclone.c vmfsclone.h partclone.h progress.h gettext.h
partclone_vmfs_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_vmfs_LDADD=-lvmfs -luuid

sbin_PROGRAMS += partclone.vmfs5
partclone_vmfs5_SOURCES=main.c partclone.c progress.c vmfs5clone.c vmfsclone.h partclone.h progress.h gettext.h
partclone_vmfs5_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_vmfs5_LDADD=-lvmfs -luuid

sbin_PROGRAMS += partclone.fstype
partclone_fstype_SOURCES=fstype.c
partclone_fstype_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_fstype_LDADD=-lvmfs -luuid

endif

if ENABLE_JFS
sbin_PROGRAMS += partclone.jfs
#partclone_jfs_SOURCES=main.c partclone.c progress.c jfs_devices.c jfs_devices.h jfsclone.c jfsclone.h partclone.h progress.h gettext.h
partclone_jfs_SOURCES=main.c partclone.c progress.c jfsclone.c jfsclone.h partclone.h progress.h gettext.h
partclone_jfs_CFLAGS=-DJFS
partclone_jfs_LDADD=-luuid -ljfs
endif

if ENABLE_BTRFS
sbin_PROGRAMS += partclone.btrfs
partclone_btrfs_SOURCES=main.c partclone.c progress.c btrfsclone.c btrfsclone.h partclone.h progress.h gettext.h $(BTRFS_SOURCE)
partclone_btrfs_CFLAGS=-DBTRFS -DBTRFS_FLAT_INCLUDES -D_XOPEN_SOURCE=700 -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE=1
partclone_btrfs_LDADD=-luuid -lblkid
endif

if ENABLE_MINIX
sbin_PROGRAMS += partclone.minix
partclone_minix_SOURCES=main.c partclone.c progress.c minixclone.c minixclone.h partclone.h progress.h gettext.h
partclone_minix_CFLAGS=-DMINIX
endif


# Extra
install-exec-hook:
if ENABLE_EXTFS
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext2
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext3
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4dev
endif
if ENABLE_HFSP
	$(LN_S) -f partclone.hfsp $(DESTDIR)$(sbindir)/partclone.hfs+
	$(LN_S) -f partclone.hfsp $(DESTDIR)$(sbindir)/partclone.hfsplus
endif
if ENABLE_NTFS
	$(LN_S) -f partclone.ntfsfixboot $(DESTDIR)$(sbindir)/partclone.ntfsreloc
endif
if ENABLE_FAT
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat12
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat16
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.fat32
	$(LN_S) -f partclone.fat $(DESTDIR)$(sbindir)/partclone.vfat
endif
if ENABLE_VMFS
	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.VMFS_volume_member
	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.vmfs3
#	$(LN_S) -f partclone.vmfs $(DESTDIR)$(sbindir)/partclone.vmfs5
endif

uninstall-local:
if ENABLE_EXTFS
	$(RM) -f $(sbindir)/partclone.ext4dev
	$(RM) -f $(sbindir)/partclone.ext4
	$(RM) -f $(sbindir)/partclone.ext3
	$(RM) -f $(sbindir)/partclone.ext2
endif
if ENABLE_HFSP
	$(RM) -f $(sbindir)/partclone.hfs+
	$(RM) -f $(sbindir)/partclone.hfsplus
endif
if ENABLE_FAT
	$(RM) -f $(sbindir)/partclone.fat12
	$(RM) -f $(sbindir)/partclone.fat16
	$(RM) -f $(sbindir)/partclone.fat32
	$(RM) -f $(sbindir)/partclone.vfat
endif
if ENABLE_NTFS
	$(RM) -f $(sbindir)/partclone.ntfsreloc
endif

if ENABLE_VMFS
	$(RM) -f $(sbindir)/partclone.VMFS_volume_member
endif

FORCE:
