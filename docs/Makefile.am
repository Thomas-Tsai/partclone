XSLTPROC=xsltproc
MAN_STYLESHEET=/usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl

man_MANS = partclone.info.8 partclone.chkimg.8 partclone.dd.8 partclone.restore.8 partclone.8

if ENABLE_EXTFS
man_MANS += partclone.extfs.8
man_MANS += partclone.ext2.8
man_MANS += partclone.ext3.8
man_MANS += partclone.ext4.8
man_MANS += partclone.ext4dev.8
endif

if ENABLE_REISERFS
man_MANS += partclone.reiserfs.8
endif

if ENABLE_REISER4
man_MANS += partclone.reiser4.8
endif

if ENABLE_HFSP
man_MANS += partclone.hfsplus.8
man_MANS += partclone.hfs+.8
man_MANS += partclone.hfsp.8
endif

if ENABLE_XFS
man_MANS += partclone.xfs.8
endif

if ENABLE_FAT
man_MANS += partclone.fat.8
man_MANS += partclone.vfat.8
man_MANS += partclone.fat12.8
man_MANS += partclone.fat16.8
man_MANS += partclone.fat32.8
endif

if ENABLE_UFS
man_MANS += partclone.ufs.8
endif

if ENABLE_VMFS
man_MANS += partclone.vmfs.8
man_MANS += partclone.vmfs3.8
man_MANS += partclone.vmfs5.8
man_MANS += partclone.VMFS_volume_member.8
man_MANS += partclone.fstype.8
endif

if ENABLE_JFS
man_MANS += partclone.jfs.8
endif

if ENABLE_BTRFS
man_MANS += partclone.btrfs.8
endif

if ENABLE_NTFS
man_MANS += partclone.ntfs.8
man_MANS += partclone.ntfsfixboot.8
man_MANS += partclone.ntfsreloc.8
endif

partclone.dd.8: partclone.dd.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.dd.xml)
partclone.chkimg.8: partclone.chkimg.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.chkimg.xml)
partclone.info.8: partclone.info.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.info.xml)
partclone.restore.8: partclone.restore.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.restore.xml)
partclone.8: partclone.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.xml)
partclone.ntfsfixboot.8: partclone.ntfsfixboot.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.ntfsfixboot.xml)
partclone.fstype.8: partclone.fstype.xml
	-@($(XSLTPROC) --nonet $(MAN_STYLESHEET) partclone.fstype.xml)
