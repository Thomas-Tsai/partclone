TESTS =  dd.test

if ENABLE_FS_TEST
if ENABLE_EXTFS
TESTS += ext2.test
TESTS += ext3.test
TESTS += ext4.test
endif

if ENABLE_BTRFS
TESTS += btrfs.test
endif

if ENABLE_FAT
TESTS += fat.test
endif

if ENABLE_REISERFS
TESTS += reiserfs.test
endif

#if ENABLE_REISER4
#TESTS += reiser4.test
#endif

if ENABLE_HFSP
TESTS += hfsplus.test
endif

if ENABLE_XFS
TESTS += xfs.test
endif

if ENABLE_NTFS
TESTS += ntfs.test
endif

#if ENABLE_UFS
#TESTS += ufs.test
#endif

#if ENABLE_VMFS
#TESTS += vmfs.test
#endif

if ENABLE_JFS
TESTS += jfs.test
endif

if ENABLE_BTRFS
TESTS += btrfs.test
endif
endif
CLEANFILES = floppy*
