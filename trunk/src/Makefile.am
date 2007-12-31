AM_CPPFLAGS = -DLOCALEDIR=\"$(localedir)\"
LDADD = $(LIBINTL)
sbin_PROGRAMS=clone.extfs

clone_extfs_SOURCES=main.c partclone.c progress.c extfsclone.c extfsclone.h partclone.h progress.h gettext.h
clone_extfs_CFLAGS=-DEXTFS
clone_extfs_LDFLAGS=-lext2fs

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
clone_xfs_LDFLAGS=-lrt
clone_xfs_LDADD=-lxfs
endif

if ENABLE_STATIC
LDFLAGS+=-static
endif
