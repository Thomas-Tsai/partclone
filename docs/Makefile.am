XSLTPROC=xsltproc
MAN_STYLESHEET=/usr/share/xml/docbook/stylesheet/docbook-xsl/manpages/docbook.xsl

chunk:
	partclone.xxxfs.xml
	$(XSLTPROC) $(MAN_STYLESHEET) $<
	partclone.chkimg.xml
	$(XSLTPROC) $(MAN_STYLESHEET) $<
	partclone.info.xml
	$(XSLTPROC) $(MAN_STYLESHEET) $<
	partclone.dd.xml
	$(XSLTPROC) $(MAN_STYLESHEET) $<
	partclone.restore.xml
	$(XSLTPROC) $(MAN_STYLESHEET) $<

man_MANS = partclone.info.8 partclone.chkimg.8 partclone.dd.8 partclone.restore.8 partclone.xxxfs.8

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
endif

if ENABLE_XFS
man_MANS += partclone.xfs.8
endif

if ENABLE_FAT
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
endif

if ENABLE_JFS
man_MANS += partclone.jfs.8
endif

if ENABLE_BTRFS
man_MANS += partclone.btrfs.8
endif

if ENABLE_NTFS
man_MANS += partclone.ntfs.8
endif
