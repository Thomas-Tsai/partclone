AUTOMAKE_OPTIONS = subdir-objects
AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\" -D_FILE_OFFSET_BITS=64
LDADD = $(LIBINTL) -lcrypto
sbin_PROGRAMS=partclone.info partclone.dd partclone.restore partclone.chkimg partclone.imager #partclone.imgfuse #partclone.block
TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox


BTRFS_SOURCE=btrfs/crypto/blake2b-ref.c btrfs/crypto/crc32c.c btrfs/crypto/hash.c btrfs/crypto/sha224-256.c btrfs/crypto/xxhash.c btrfs/kernel-lib/list_sort.c btrfs/kernel-lib/raid56.c btrfs/kernel-lib/rbtree.c btrfs/kernel-lib/tables.c btrfs/kernel-lib/bitmap.h btrfs/kernel-shared/ulist.c btrfs/kernel-shared/zoned.c btrfs/kernel-shared/extent-tree.c btrfs/kernel-shared/ctree.c btrfs/kernel-shared/volumes.c btrfs/kernel-shared/disk-io.c btrfs/kernel-shared/delayed-ref.c btrfs/kernel-shared/dir-item.c  btrfs/kernel-shared/extent_io.c btrfs/kernel-shared/file-item.c btrfs/kernel-shared/free-space-cache.c btrfs/kernel-shared/free-space-tree.c btrfs/kernel-shared/inode-item.c btrfs/kernel-shared/print-tree.c btrfs/kernel-shared/root-tree.c btrfs/kernel-shared/transaction.c  btrfs/kernel-shared/uuid-tree.c btrfs/kernel-shared/compression.h btrfs/kernel-shared/accessors.c btrfs/kernel-shared/extent-io-tree.c btrfs/kernel-shared/locking.c btrfs/kernel-shared/messages.c btrfs/kernel-shared/async-thread.c btrfs/kernel-shared/tree-checker.c btrfs/libbtrfsutil/stubs.c btrfs/libbtrfsutil/subvolume.c btrfs/libbtrfsutil/qgroup.c btrfs/libbtrfs/send-stream.c btrfs/libbtrfs/ioctl.h btrfs/libbtrfs/kerncompat.h btrfs/common/device-scan.c btrfs/common/messages.c btrfs/common/path-utils.c btrfs/common/rbtree-utils.c btrfs/common/utils.c btrfs/common/device-utils.c btrfs/common/fsfeatures.c  btrfs/common/units.c btrfs/common/open-utils.c btrfs/common/parse-utils.c btrfs/common/extent-cache.c btrfs/common/string-utils.c btrfs/common/filesystem-utils.c btrfs/common/cpu-utils.c btrfs/common/sysfs-utils.c  btrfs/check/repair.c
if X64
BTRFS_SOURCE+=btrfs/crypto/crc32c-pcl-intel-asm_64.S
endif

XFS_SOURCE=xfs/libxfs/cache.c xfs/libxfs/crc32.c xfs/libxfs/defer_item.c xfs/libxfs/init.c xfs/libxfs/kmem.c xfs/libxfs/list_sort.c xfs/libxfs/logitem.c xfs/libxfs/radix-tree.c xfs/libxfs/rdwr.c xfs/libxfs/trans.c xfs/libxfs/util.c xfs/libxfs/xfs_ag_resv.c xfs/libxfs/xfs_alloc.c xfs/libxfs/xfs_alloc_btree.c xfs/libxfs/xfs_attr.c xfs/libxfs/xfs_attr_leaf.c xfs/libxfs/xfs_attr_remote.c xfs/libxfs/xfs_bit.c xfs/libxfs/xfs_bmap.c xfs/libxfs/xfs_bmap_btree.c xfs/libxfs/xfs_btree.c xfs/libxfs/xfs_da_btree.c xfs/libxfs/xfs_da_format.c xfs/libxfs/xfs_defer.c xfs/libxfs/xfs_dir2.c xfs/libxfs/xfs_dir2_block.c xfs/libxfs/xfs_dir2_data.c xfs/libxfs/xfs_dir2_leaf.c xfs/libxfs/xfs_dir2_node.c xfs/libxfs/xfs_dir2_sf.c xfs/libxfs/xfs_dquot_buf.c xfs/libxfs/xfs_ialloc.c xfs/libxfs/xfs_inode_buf.c xfs/libxfs/xfs_inode_fork.c xfs/libxfs/xfs_ialloc_btree.c xfs/libxfs/xfs_log_rlimit.c xfs/libxfs/xfs_refcount.c xfs/libxfs/xfs_refcount_btree.c xfs/libxfs/xfs_rmap.c xfs/libxfs/xfs_rmap_btree.c xfs/libxfs/xfs_rtbitmap.c xfs/libxfs/xfs_sb.c xfs/libxfs/xfs_symlink_remote.c xfs/libxfs/xfs_trans_resv.c xfs/libxfs/linux.c xfs/libxfs/xfs_types.h xfs/libxfs/xfs_types.c xfs/libxfs/xfs_iext_tree.c 

EXFATFS_SOURCE=exfat/cluster.c exfat/utf.c exfat/utils.c exfat/lookup.c exfat/io.c exfat/log.c exfat/node.c exfat/mount.c exfat/time.c

F2FS_SOURCE=f2fs/fsck.c f2fs/libf2fs.c f2fs/fsck.h f2fs/mount.c f2fs/f2fs_fs.h f2fs/list.h f2fs/f2fs.h

if ENABLE_STATIC
AM_LDFLAGS=-static
LDADD_static=-lcrypto -ldl
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

main_files=main.c partclone.c progress.c checksum.c torrent_helper.c partclone.h progress.h gettext.h checksum.h torrent_helper.h bitmap.h

partclone_info_SOURCES=info.c partclone.c checksum.c partclone.h fs_common.h checksum.h
partclone_restore_SOURCES=$(main_files) ddclone.c ddclone.h
partclone_restore_CFLAGS=-DRESTORE -DDD
partclone_restore_LDADD=-lcrypto ${LDADD_static}

partclone_chkimg_SOURCES=$(main_files) ddclone.c ddclone.h
partclone_chkimg_CFLAGS=-DCHKIMG -DDD
partclone_chkimg_LDADD=-lcrypto ${LDADD_static}

partclone_dd_SOURCES=$(main_files) ddclone.c ddclone.h
partclone_dd_CFLAGS=-DDD
partclone_dd_LDADD=-lcrypto ${LDADD_static}

partclone_imager_SOURCES=$(main_files) ddclone.c ddclone.h
partclone_imager_CFLAGS=-DIMG
partclone_imager_LDADD=-lcrypto ${LDADD_static}

if ENABLE_EXTFS
sbin_PROGRAMS += partclone.extfs
partclone_extfs_SOURCES=$(main_files) extfsclone.c extfsclone.h
partclone_extfs_CFLAGS=-DEXTFS
partclone_extfs_LDADD=-lext2fs -lcom_err -lpthread -lcrypto ${LDADD_static}
endif

if ENABLE_REISERFS
sbin_PROGRAMS += partclone.reiserfs
partclone_reiserfs_SOURCES=$(main_files) reiserfsclone.c reiserfsclone.h
partclone_reiserfs_CFLAGS=-DREISERFS
partclone_reiserfs_LDADD=-lreiserfs -ldal -lcrypto ${LDADD_static}
endif

if ENABLE_REISER4
sbin_PROGRAMS += partclone.reiser4
partclone_reiser4_SOURCES=$(main_files) reiser4clone.c reiser4clone.h
partclone_reiser4_CFLAGS=-DREISER4
partclone_reiser4_LDADD=-lreiser4 -laal -lcrypto ${LDADD_static}
endif

if ENABLE_HFSP
sbin_PROGRAMS += partclone.hfsp
partclone_hfsp_SOURCES=$(main_files) hfsplusclone.c hfsplusclone.h
partclone_hfsp_CFLAGS=-DHFSPLUS
partclone_hfsp_LDADD=-lcrypto ${LDADD_static}
endif

if ENABLE_APFS
sbin_PROGRAMS += partclone.apfs
partclone_apfs_SOURCES=$(main_files) apfsclone.c apfsclone.h
partclone_apfs_CFLAGS=-DAPFS
partclone_apfs_LDADD=-lcrypto ${LDADD_static}
endif


if ENABLE_XFS
sbin_PROGRAMS += partclone.xfs
partclone_xfs_SOURCES=$(main_files) xfsclone.c xfsclone.h $(XFS_SOURCE)
partclone_xfs_CFLAGS=-I$(srcdir)/xfs/include -I$(srcdir)/xfs/libxfs/ -I$(srcdir)/xfs/include/xfs/ -DXFS -D_GNU_SOURCE -DNDEBUG  $(UUID_CFLAGS) 
partclone_xfs_LDADD=-lrt -lpthread -luuid -lcrypto ${LDADD_static}
endif

if ENABLE_EXFAT
sbin_PROGRAMS += partclone.exfat
partclone_exfat_SOURCES=$(main_files) exfatclone.c exfatclone.h $(EXFATFS_SOURCE)
partclone_exfat_CFLAGS=-DEXFAT -D_GNU_SOURCE -std=c99
partclone_exfat_LDADD=-lcrypto ${LDADD_static}
endif

if ENABLE_F2FS
sbin_PROGRAMS += partclone.f2fs
partclone_f2fs_SOURCES=$(main_files) f2fsclone.c f2fsclone.h $(F2FS_SOURCE)
partclone_f2fs_CFLAGS=-DF2FS
partclone_f2fs_LDADD=-luuid -lcrypto ${LDADD_static}
endif

if ENABLE_NILFS2
sbin_PROGRAMS += partclone.nilfs2
partclone_nilfs2_SOURCES=$(main_files) nilfsclone.c nilfsclone.h
partclone_nilfs2_CFLAGS=-DNILFS
partclone_nilfs2_LDADD=-lnilfs -lcrypto ${LDADD_static}
endif

if ENABLE_FAT
sbin_PROGRAMS += partclone.fat
partclone_fat_SOURCES=$(main_files) fatclone.c fatclone.h
partclone_fat_CFLAGS=-DFAT
partclone_fat_LDADD=-lcrypto ${LDADD_static}
endif

sbin_PROGRAMS += partclone.ntfsfixboot
partclone_ntfsfixboot_SOURCES=ntfsfixboot.c
if ENABLE_NTFS
sbin_PROGRAMS += partclone.ntfs
partclone_ntfs_SOURCES=$(main_files) ntfsclone-ng.c ntfsclone-ng.h
if ENABLE_NTFS_3G
partclone_ntfs_CFLAGS=-DNTFS3G
partclone_ntfs_LDADD=-lntfs-3g -lcrypto ${LDADD_static}
else
partclone_ntfs_CFLAGS=-DNTFS
partclone_ntfs_LDADD=-lntfs -lcrypto ${LDADD_static}
endif
endif

if ENABLE_UFS
sbin_PROGRAMS += partclone.ufs
partclone_ufs_SOURCES=$(main_files) ufsclone.c ufsclone.h
partclone_ufs_CFLAGS=-DUFS -D_GNU_SOURCE
partclone_ufs_LDADD=-lufs -lbsd -lcrypto ${LDADD_static}
endif

if ENABLE_VMFS
sbin_PROGRAMS += partclone.vmfs
partclone_vmfs_SOURCES=$(main_files) vmfsclone.c vmfsclone.h
partclone_vmfs_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_vmfs_LDADD=-lvmfs -luuid -lcrypto ${LDADD_static}

sbin_PROGRAMS += partclone.vmfs5
partclone_vmfs5_SOURCES=$(main_files) vmfs5clone.c vmfsclone.h
partclone_vmfs5_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_vmfs5_LDADD=-lvmfs -luuid -lcrypto ${LDADD_static}

sbin_PROGRAMS += partclone.fstype
partclone_fstype_SOURCES=fstype.c
partclone_fstype_CFLAGS=-DVMFS -D_GNU_SOURCE $(UUID_CFLAGS)
partclone_fstype_LDADD=-lvmfs -luuid -lcrypto ${LDADD_static}

endif

if ENABLE_JFS
sbin_PROGRAMS += partclone.jfs
#partclone_jfs_SOURCES=$(main_files) jfs_devices.c jfs_devices.h jfsclone.c jfsclone.h
partclone_jfs_SOURCES=$(main_files) jfsclone.c jfsclone.h
partclone_jfs_CFLAGS=-DJFS
partclone_jfs_LDADD=-luuid -ljfs -lcrypto ${LDADD_static}
endif

if ENABLE_BTRFS
sbin_PROGRAMS += partclone.btrfs
partclone_btrfs_SOURCES=$(main_files) btrfsclone.c btrfsclone.h $(BTRFS_SOURCE)
partclone_btrfs_CFLAGS=-DBTRFS -DBTRFS_FLAT_INCLUDES -D_XOPEN_SOURCE=700 -D_GNU_SOURCE -DCRYPTOPROVIDER_BUILTIN=1 -I$(srcdir)/btrfs -I$(srcdir)/btrfs/include -I$(srcdir)/btrfs/libbtrfsutil 
partclone_btrfs_LDADD=-luuid -lblkid -lcrypto ${LDADD_static}
endif

if ENABLE_MINIX
sbin_PROGRAMS += partclone.minix
partclone_minix_SOURCES=$(main_files) minixclone.c minixclone.h
partclone_minix_CFLAGS=-DMINIX
partclone_minix_LDADD=-lcrypto ${LDADD_static}
endif

if ENABLE_FUSE
sbin_PROGRAMS+=partclone.imgfuse
partclone_imgfuse_SOURCES=fuseimg.c partclone.c checksum.c partclone.h fs_common.h checksum.h
partclone_imgfuse_LDADD=-lfuse -lcrypto ${LDADD_static}
if ENABLE_STATIC
partclone_imgfuse_LDADD+=-ldl -lcrypto ${LDADD_static}
endif
endif

compconfdir=@bashcompletiondir@
compconf_DATA = partclone-prompt

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
	$(RM) -f $(compconfdir)/bash_completion.d/partclone-prompt
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
