AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
LDADD = $(LIBINTL)
sbin_PROGRAMS=clone.extfs clone.info
TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox

version.c: FORCE
	$(TOOLBOX) --update-version

clone_extfs_SOURCES=main.c partclone.c progress.c extfsclone.c extfsclone.h partclone.h progress.h gettext.h
clone_extfs_CFLAGS=-DEXTFS
clone_extfs_LDFLAGS=-lext2fs

clone_info_SOURCES=infoclone.c partclone.c partclone.h

if ENABLE_REISERFS
sbin_PROGRAMS += clone.reiserfs
clone_reiserfs_SOURCES=main.c partclone.c progress.c reiserfsclone.c reiserfsclone.h partclone.h progress.h gettext.h
clone_reiserfs_CFLAGS=-DREISERFS
clone_reiserfs_LDFLAGS=-lreiserfs -ldal
endif

if ENABLE_REISER4
sbin_PROGRAMS += clone.reiser4
clone_reiser4_SOURCES=main.c partclone.c progress.c reiser4clone.c reiser4clone.h partclone.h progress.h gettext.h
clone_reiser4_CFLAGS=-DREISER4
clone_reiser4_LDADD=-lreiser4 -laal
endif

if ENABLE_HFSP
sbin_PROGRAMS += clone.hfsp
clone_hfsp_SOURCES=main.c partclone.c progress.c hfsplusclone.c hfsplusclone.h partclone.h progress.h gettext.h
clone_hfsp_CFLAGS=-DHFSPLUS
endif

if ENABLE_XFS
sbin_PROGRAMS += clone.xfs
clone_xfs_SOURCES=main.c partclone.c progress.c xfsclone.c xfsclone.h partclone.h progress.h gettext.h
clone_xfs_CFLAGS=-DXFS
clone_xfs_LDFLAGS=
clone_xfs_LDADD=-lxfs -lrt -lpthread
endif

if ENABLE_FAT
sbin_PROGRAMS += clone.fat
clone_fat_SOURCES=main.c partclone.c progress.c fatclone.c fatclone.h partclone.h progress.h gettext.h
clone_fat_CFLAGS=-DFAT
endif

if ENABLE_STATIC
LDFLAGS+=-static
endif

# Extra
install-exec-hook:
	$(LN_S) -f clone.extfs $(DESTDIR)$(sbindir)/clone.ext2
	$(LN_S) -f clone.extfs $(DESTDIR)$(sbindir)/clone.ext3
if ENABLE_HFSP
	$(LN_S) -f clone.hfsp $(DESTDIR)$(sbindir)/clone.hfs+
endif
if ENABLE_FAT
	$(LN_S) -f clone.fat $(DESTDIR)$(sbindir)/clone.fat16
	$(LN_S) -f clone.fat $(DESTDIR)$(sbindir)/clone.fat32
	$(LN_S) -f clone.fat $(DESTDIR)$(sbindir)/clone.vfat
endif

uninstall-local:
	$(RM) -f $(sbindir)/clone.ext3
	$(RM) -f $(sbindir)/clone.ext2
if ENABLE_HFSP
	$(RM) -f $(sbindir)/clone.hfs+
endif
if ENABLE_FAT
	$(RM) -f $(sbindir)/clone.fat16
	$(RM) -f $(sbindir)/clone.fat32
	$(RM) -f $(sbindir)/clone.vfat
endif

FORCE:
