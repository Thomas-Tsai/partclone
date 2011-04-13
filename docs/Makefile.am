man_MANS = partclone.info.8 partclone.chkimg.8

if ENABLE_EXTFS
man_MANS += partclone.extfs.8
endif

if ENABLE_REISERFS
$(LN_S) -f partclone.extfs.8.in partclone.reiserfs.8.in
man_MANS += partclone.reiserfs.8
endif

if ENABLE_REISER4
$(LN_S) -f partclone.extfs.8.in partclone.reiser4.8.in
man_MANS += partclone.reiser4.8
endif

if ENABLE_HFSP
$(LN_S) -f partclone.extfs.8.in partclone.hfsp.8.in
man_MANS += partclone.hfsp.8
endif

if ENABLE_XFS
$(LN_S) -f partclone.extfs.8.in partclone.xfs.8.in
man_MANS += partclone.xfs.8
endif

if ENABLE_FAT
$(LN_S) -f partclone.extfs.8.in partclone.fat.8.in
man_MANS += partclone.fat.8
endif

if ENABLE_UFS
$(LN_S) -f partclone.extfs.8.in partclone.ufs.8.in
man_MANS += partclone.ufs.8
endif

if ENABLE_VMFS
$(LN_S) -f partclone.extfs.8.in partclone.vmfs.8.in
man_MANS += partclone.vmfs.8
endif

if ENABLE_JFS
$(LN_S) -f partclone.extfs.8.in partclone.jfs.8.in
man_MANS += partclone.jfs.8
endif

if ENABLE_BTRFS
$(LN_S) -f partclone.extfs.8.in partclone.btrfs.8.in
man_MANS += partclone.btrfs.8
endif

if ENABLE_NTFS
$(LN_S) -f partclone.extfs.8.in partclone.ntfs.8.in
man_MANS += partclone.ntfs.8
endif
