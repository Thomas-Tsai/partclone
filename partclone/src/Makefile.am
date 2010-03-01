AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
LDADD = $(LIBINTL)
sbin_PROGRAMS=partclone.info partclone.dd partclone.restore partclone.chkimg
TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox

version.h: FORCE
	$(TOOLBOX) --update-version

partclone_info_SOURCES=infoclone.c partclone.c partclone.h fs_common.h

partclone_restore_SOURCES=partclone.c partclone.h progress.c progress.h gettext.h restore.c
partclone_restore_CFLAGS=-DRESTORE

partclone_chkimg_SOURCES=partclone.c partclone.h progress.c progress.h gettext.h chkimg.c

partclone_dd_SOURCES=partclone.c partclone.h progress.c progress.h gettext.h ddclone.c

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
partclone_xfs_SOURCES=main.c partclone.c progress.c xfsclone.c xfsclone.h partclone.h progress.h gettext.h
partclone_xfs_CFLAGS=-DXFS
partclone_xfs_LDFLAGS=
partclone_xfs_LDADD=-lxfs -lrt -lpthread
endif

if ENABLE_FAT
sbin_PROGRAMS += partclone.fat
partclone_fat_SOURCES=main.c partclone.c progress.c fatclone.c fatclone.h partclone.h progress.h gettext.h
partclone_fat_CFLAGS=-DFAT
endif

if ENABLE_NTFS
sbin_PROGRAMS += partclone.ntfs
partclone_ntfs_SOURCES=main.c partclone.c progress.c ntfsclone-ng.c ntfsclone-ng.h partclone.h progress.h gettext.h
partclone_ntfs_CFLAGS=-DNTFS
partclone_ntfs_LDADD=-lntfs
sbin_PROGRAMS += partclone.ntfsfixboot
partclone_ntfsfixboot_SOURCES=ntfsfixboot.c
endif


if ENABLE_STATIC
LDFLAGS+=-static 
endif

if ENABLE_UFS
sbin_PROGRAMS += partclone.ufs
partclone_ufs_SOURCES=main.c partclone.c progress.c ufsclone.c ufsclone.h partclone.h progress.h gettext.h
partclone_ufs_CFLAGS=-DUFS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
partclone_ufs_LDADD=-lufs -lbsd  
endif

if ENABLE_VMFS
sbin_PROGRAMS += partclone.vmfs
partclone_vmfs_SOURCES=main.c partclone.c progress.c vmfsclone.c vmfsclone.h partclone.h progress.h gettext.h
partclone_vmfs_CFLAGS=-DVMFS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(UUID_CFLAGS)
partclone_vmfs_LDADD=-lvmfs -luuid

sbin_PROGRAMS += partclone.fstype
partclone_fstype_SOURCES=fstype.c
partclone_fstype_CFLAGS=-DVMFS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 $(UUID_CFLAGS)
partclone_fstype_LDADD=-lvmfs -luuid

endif

# Extra
install-exec-hook:
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext2
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext3
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4
	$(LN_S) -f partclone.extfs $(DESTDIR)$(sbindir)/partclone.ext4dev
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

uninstall-local:
	$(RM) -f $(sbindir)/partclone.ext4dev
	$(RM) -f $(sbindir)/partclone.ext4
	$(RM) -f $(sbindir)/partclone.ext3
	$(RM) -f $(sbindir)/partclone.ext2
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

FORCE:
