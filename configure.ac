AC_INIT([Partclone],[0.3.37],[thomas@clonezilla.org])
AM_INIT_AUTOMAKE([-Wall foreign])
AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])
AC_PROG_CC
AM_PROG_CC_C_O
AM_PROG_AS
AC_PROG_INSTALL
AC_PATH_PROG(RM, rm, rm)
AC_PROG_LN_S

# Enable large file support.
AC_SYS_LARGEFILE

# default value##
supported_fs=""

##enable-all##
AC_ARG_ENABLE([all],
    AS_HELP_STRING(
        [--enable-all],
        [enable all supported file system])
)
AM_CONDITIONAL(ENABLE_ALL, test "$enable_all" = yes)
if test "$enable_all" = "yes"; then
enable_fuse="yes"
enable_xfs="yes"
enable_extfs="yes"
enable_reiserfs="yes"
enable_reiser4="yes"
enable_hfsp="yes"
enable_apfs="yes"
enable_fat="yes"
enable_exfat="yes"
enable_ntfs="yes"
enable_ufs="yes"
enable_vmfs="yes"
enable_jfs="yes"
enable_btrfs="yes"
enable_minix="yes"
enable_f2fs="yes"
enable_nilfs2="yes"
fi

ORIGINAL_CFLAGS="$CFLAGS"
AC_SUBST(ORIGINAL_CFLAGS)

if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi

dnl Check for uuid 
PKG_CHECK_MODULES(UUID, uuid,,exit)
uuidcfg=`pkg-config --cflags --libs uuid`

AC_ARG_ENABLE([fuse],
    AS_HELP_STRING(
        [--enable-fuse],
        [enable imgfuse, mount image file to block files])
)
AM_CONDITIONAL(ENABLE_FUSE, test "$enable_fuse" = yes)
if test "$enable_fuse" = "yes"; then
AS_MESSAGE([checking  for FUSE Library and Header files ... ...])
    PKG_CHECK_MODULES([FUSE], [fuse],,exit)
    uuidcfg=`pkg-config --cflags --libs fuse`
AC_MSG_CHECKING(version of FUSE)

AC_COMPUTE_INT([fuse_version_major], [FUSE_MAJOR_VERSION], [[
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <fuse/fuse.h>
]],
    AC_MSG_FAILURE([fuse/fuse.h: could not determine 'FUSE_MAJOR_VERSION' value]))
AC_COMPUTE_INT([fuse_version_minor], [FUSE_MINOR_VERSION], [[
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include <fuse/fuse.h>
]],
    AC_MSG_FAILURE([fuse/fuse.h: could not determine 'FUSE_MINOR_VERSION' value]))
fuse_version="${fuse_version_major}.${fuse_version_minor}"
AC_MSG_RESULT([$fuse_version])
fi

AC_CHECK_LIB([pthread], [pthread_create], [], AC_MSG_ERROR([*** pthread library (libpthread) not found]))

##ext2/3##
AC_ARG_ENABLE([extfs],
    AS_HELP_STRING(
        [--enable-extfs],
        [enable ext2/3/4 file system])
)
AM_CONDITIONAL(ENABLE_EXTFS, test "$enable_extfs" = yes)

if test "$enable_extfs" = "yes"; then
#check library of some filesystems
dnl Check for EXT2/3
AS_MESSAGE([checking  for EXT2/3 Library and Header files ... ...])
AC_CHECK_HEADERS([ext2fs/ext2fs.h], ,
  AC_MSG_ERROR([*** EXT2/3 header files (ext2fs/ext2fs.h) not found])
)
AC_CHECK_LIB([ext2fs], [ext2fs_initialize], true,
    AC_MSG_ERROR([*** EXT2/3 library (libext2fs) not found]))

AC_MSG_CHECKING(version of libextfs)
PKG_CHECK_MODULES([EXT2FS], [ext2fs], [extfs_version=`pkg-config --modversion ext2fs`], )
supported_fs=$supported_fs" extfs"

fi
#end of check extfs

##XFS##
AC_ARG_ENABLE([xfs],
    AS_HELP_STRING(
        [--enable-xfs],
        [enable XFS file system])
)
AM_CONDITIONAL(ENABLE_XFS, test "$enable_xfs" = yes)

if test "$enable_xfs" = "yes"; then
supported_fs=$supported_fs" xfs"
xfs_version="build-in"
fi
#end of check xfs

##reiserfs##
AC_ARG_ENABLE([reiserfs],
    AS_HELP_STRING(
        [--enable-reiserfs],
        [enable REISERFS 3.6/3.6 file system])
)
AM_CONDITIONAL(ENABLE_REISERFS, test "$enable_reiserfs" = yes)

if test "$enable_reiserfs" = "yes"; then

dnl Check for REISERFS
AS_MESSAGE([checking  for Reiserfs Library and Header files ... ...])
AC_CHECK_HEADERS([reiserfs/reiserfs.h], ,
    AC_MSG_ERROR([*** reiserfs header files (reiserfs/reiserfs.h) not found])
)
AC_CHECK_LIB([reiserfs], [reiserfs_fs_open], true,
    AC_MSG_ERROR([*** Reiserfs library (libreiserfs) not found]))
AC_CHECK_LIB([dal], [file_dal_open], true,
    AC_MSG_ERROR([*** Reiserfs depend library (libdal) not found]))
AC_MSG_CHECKING(version of libreiserfs)
supported_fs=$supported_fs" reiserfs"
reiserfs_version="unknown, suggest 0.3.0.5"

fi
#end of check reiserfs

##reiser4##
AC_ARG_ENABLE([reiser4],
    AS_HELP_STRING(
        [--enable-reiser4],
        [enable Reiser4 file system])
)
AM_CONDITIONAL(ENABLE_REISER4, test "$enable_reiser4" = yes)

if test "$enable_reiser4" = "yes"; then

dnl Check for REISER4
dnl Check uuid library which depended by REISER4
AS_MESSAGE([checking  for Reiser4 Library and Header files ... ...])
AC_CHECK_HEADERS([reiser4/libreiser4.h], ,
  AC_MSG_ERROR([*** reiser4 header files (reiser4/libreiser4.h) not found])
)
AC_CHECK_LIB([aal], [aal_device_open], true,
    AC_MSG_ERROR([*** Reiser4 depend library (libaal) not found]))
supported_fs=$supported_fs" reiser4"
reiser4_version="unknown, suggest 1.0.6"
fi
#end of check reiser4

##hfs plus##
AC_ARG_ENABLE([hfsp],
    AS_HELP_STRING(
        [--enable-hfsp],
        [enable HFS plus file system])
)
AM_CONDITIONAL(ENABLE_HFSP, test "$enable_hfsp" = yes)

if test "$enable_hfsp" = "yes"; then
supported_fs=$supported_fs" hfs-plus"
hfs_plus_version="build-in"
fi
#end of check hfsplus

##apfs##
AC_ARG_ENABLE([apfs],
    AS_HELP_STRING(
        [--enable-apfs],
        [enable APFS file system])
)
AM_CONDITIONAL(ENABLE_APFS, test "$enable_apfs" = yes)

if test "$enable_apfs" = "yes"; then
supported_fs=$supported_fs" apfs"
apfs_version="build-in"
fi
#end of check hfsplus

##fat##
AC_ARG_ENABLE([fat],
    AS_HELP_STRING(
        [--enable-fat],
        [enable FAT file system])
)
AM_CONDITIONAL(ENABLE_FAT, test "$enable_fat" = yes)

if test "$enable_fat" = "yes"; then
supported_fs=$supported_fs" fat"
fat_version="build-in"
fi
#end of check fat

##exfat##
AC_ARG_ENABLE([exfat],
    AS_HELP_STRING(
        [--enable-exfat],
        [enable EXFAT file system])
)
AM_CONDITIONAL(ENABLE_EXFAT, test "$enable_exfat" = yes)

if test "$enable_exfat" = "yes"; then
supported_fs=$supported_fs" exfat"
exfat_version="build-in"
fi
#end of check exfat

##f2fs##
AC_ARG_ENABLE([f2fs],
    AS_HELP_STRING(
        [--enable-f2fs],
        [enable f2fs file system])
)

AM_CONDITIONAL(ENABLE_F2FS, test "$enable_f2fs" = yes)

supported_fs=$supported_fs" f2fs"
f2fs_version="build-in 1.16"

##end of check f2fs

##nilfs2##
AC_ARG_ENABLE([nilfs2],
    AS_HELP_STRING(
        [--enable-nilfs2],
        [enable nilfs2 file system])
)
AM_CONDITIONAL(ENABLE_NILFS2, test "$enable_nilfs2" = yes)

if test "$enable_nilfs2" = "yes"; then
dnl Check headers which depended by NILFS2
AS_MESSAGE([checking  for NILFS2 header files ... ...])
AC_CHECK_HEADERS([ctype.h fcntl.h grp.h libintl.h limits.h linux/magic.h \
		  linux/types.h locale.h mntent.h mqueue.h paths.h pwd.h \
		  semaphore.h stdlib.h string.h strings.h sys/ioctl.h \
		  sys/mount.h sys/time.h syslog.h sys/stat.h time.h unistd.h])
AC_CHECK_HEADERS([nilfs.h], ,
  AC_MSG_ERROR([*** nilfs2 header files (nilfs.h) not found])
)

AC_CHECK_LIB([nilfs], [nilfs_open], true,
    AC_MSG_ERROR([*** nilfs2 depend library (libnilfs) not found]))

supported_fs=$supported_fs" nilfs2"
nilfs2_version="unknown, suggest 2.3.2"
fi
#end of check nilfs2


##NTFS##
AC_ARG_ENABLE([ntfs],
    AS_HELP_STRING(
        [--enable-ntfs],
        [enable NTFS file system])
)

if test "$enable_ntfs" = "yes"; then
    ntfs_3g_h=0
    ntfs_3g_l=0
    ntfsprogs_h=0
    ntfsprogs_l=0
    #check library of some filesystems
    dnl Check for NTFS-3g
    AS_MESSAGE([checking  for NTFS-3g Library and Header files ... ...])
    AC_CHECK_HEADERS([sys/time.h sys/stat.h time.h])
    AC_CHECK_HEADERS([ntfs-3g/misc.h], ntfs_3g_h=1,
    AC_MSG_WARN([*** NTFS(libntfs-3g-dev) header not found]))
    AC_CHECK_LIB([ntfs-3g], [ntfs_mount], ntfs_3g_l=1,
    AC_MSG_WARN([*** ntfs depend library (libntfs-3g) not found]))

    dnl Check for NTFSPROGS
    AS_MESSAGE([checking  for NTFS Library and Header files ... ...])
    AC_CHECK_HEADERS([ntfs/version.h], ntfsprogs_h=1,
    AC_MSG_WARN([*** NTFS(ntfsprogs) header not found]))
    AC_CHECK_LIB([ntfs], [ntfs_mount], ntfsprogs_l=1,
    AC_MSG_WARN([*** ntfsprogs library (libntfs) not found]))
fi

AM_CONDITIONAL(ENABLE_NTFS_3G, test "$ntfs_3g_l" = 1)
AM_CONDITIONAL(ENABLE_NTFS_PROGS, test "$ntfsprogs_l" = 1)
AM_CONDITIONAL(ENABLE_NTFS, test "$ntfsprogs_l" = 1 -o "$ntfs_3g_l" = 1)
define_ntfs_version="ntfs-3g"
if test "$ntfs_3g_h" = 1 -a "$ntfs_3g_l" = 1; then
    supported_fs=$supported_fs" ntfs"
    ntfs_version="ntfs-3g"
    PKG_CHECK_MODULES([NTFS], [libntfs-3g >= 2010], [ntfs_version=`pkg-config --modversion libntfs-3g`])
elif test "$ntfsprogs_h" = 1 -a "$ntfsprogs_l" = 1; then
    ntfs_version="unknown ntfsprogs, suggest 0:9:0"
fi
#end of check ntfs


##UFS##
AC_ARG_ENABLE([ufs],
    AS_HELP_STRING(
        [--enable-ufs],
        [enable UFS(1/2) file system])
)
AM_CONDITIONAL(ENABLE_UFS, test "$enable_ufs" = yes)

if test "$enable_ufs" = "yes"; then
#check library of some filesystems

dnl Check for UFS 
AS_MESSAGE([checking  for UFS Library ... ...])
AC_CHECK_LIB([ufs], [ufs_disk_fillout], true,
        AC_MSG_ERROR([*** ufs depend library (libufs2) not found]))
supported_fs=$supported_fs" ufs"
ufs_version="unknown, suggest 7.0-2"
fi
#end of check ufs

##VMFS##
AC_ARG_ENABLE([vmfs],
    AS_HELP_STRING(
        [--enable-vmfs],
        [enable vmfs file system])
)
AM_CONDITIONAL(ENABLE_VMFS, test "$enable_vmfs" = yes)

if test "$enable_vmfs" = "yes"; then
#check library of some filesystems

dnl Check for VMFS 
##AS_MESSAGE([checking  for VMFS Library and Header files ... ...])
##AC_CHECK_HEADERS([uuid/uuid.h vmfs/vmfs.h], ,
##   AC_MSG_ERROR([*** VMFS(vmfs-tools) header not found]))
AS_MESSAGE([checking for VMFS Library files ... ...])
AC_CHECK_LIB([vmfs -luuid], [vmfs_host_init], true,
        AC_MSG_ERROR([*** vmfs depend library (libvmfs) not found]))
supported_fs=$supported_fs" vmfs"
vmfs_version="unknown"
fi
#end of check vmfs

##JFS##
AC_ARG_ENABLE([jfs],
    AS_HELP_STRING(
        [--enable-jfs],
        [enable jfs file system])
)
AM_CONDITIONAL(ENABLE_JFS, test "$enable_jfs" = yes)

if test "$enable_jfs" = "yes"; then
#check library of some filesystems

dnl Check for JFS 
##AS_MESSAGE([checking  for JFS Library and Header files ... ...])
##AC_CHECK_HEADERS([uuid/uuid.h jfs/jfs_superblock.h], ,
##   AC_MSG_ERROR([*** JFS(libjfs) header not found]))
AS_MESSAGE([checking for JFS Library files ... ...])
AC_CHECK_LIB([jfs -luuid], [ujfs_get_superblk], true,
        AC_MSG_ERROR([*** jfs depend library (libjfs) not found]))
supported_fs=$supported_fs" jfs"
jfs_version="unknown, suggest 1.1.12"
fi
#end of check jfs

##btrfs##
AC_ARG_ENABLE([btrfs],
    AS_HELP_STRING(
        [--enable-btrfs],
        [enable btrfs file system])
)
AM_CONDITIONAL(ENABLE_BTRFS, test "$enable_btrfs" = yes)

if test "$enable_btrfs" = yes; then

AC_CHECK_HEADERS([blkid/blkid.h],,
AC_MSG_ERROR([*** btrfs depend library (libblkid-dev) header not found]))
supported_fs=$supported_fs" btrfs"
btrfs_version="unknown"
fi

#condition for CPU target
AC_CANONICAL_TARGET
AM_CONDITIONAL([X64], [test "$target_cpu" = x86_64])

#end of check btrfs

##minix##
AC_ARG_ENABLE([minix],
    AS_HELP_STRING(
        [--enable-minix],
        [enable minix file system])
)
AM_CONDITIONAL(ENABLE_MINIX, test "$enable_minix" = yes)

if test "$enable_minix" = "yes"; then
supported_fs=$supported_fs" minix"
minix_version="build-in"
fi
#end of check minix


##libncursesw##
AC_ARG_ENABLE([ncursesw],
    AS_HELP_STRING(
        [--enable-ncursesw],
        [enable TEXT User Interface])
)
AM_CONDITIONAL(ENABLE_NCURSESW, test "$enable_ncursesw" = yes)

if test "$enable_ncursesw" = "yes"; then

dnl Check for NCURSESW
AS_MESSAGE([checking  for Ncursesw Library and Header files ... ...])
AC_CHECK_HEADERS([ncurses.h], ,
    AC_MSG_ERROR([*** ncurses header files (ncurses.h) not found])
)
AC_CHECK_LIB([ncursesw], [initscr], ,
    AC_MSG_ERROR([*** Ncursesw library (libncursesw5) not found]))
fi

##libcrypto##
AC_CHECK_LIB(crypto, EVP_MD_CTX_new, [AC_DEFINE([HAVE_EVP_MD_CTX_new],1,[OpenSSL 1.1.0+])])
AC_CHECK_LIB(crypto, EVP_MD_CTX_create, [AC_DEFINE([HAVE_EVP_MD_CTX_create],1,[OpenSSL 1.0+])])
AC_CHECK_LIB(crypto, EVP_MD_CTX_reset, [AC_DEFINE([HAVE_EVP_MD_CTX_reset],1,[OpenSSL 1.1.0+])])

##libm##
AC_CHECK_DECLS([isnormal], [], [], [[#include <math.h>]])
if test "$ac_cv_have_decl_isnormal" = "yes" ; then
    AC_TRY_LINK([#include <math.h>], [float f = 0.0; return isnormal(f)], [], [LIBS="$LIBS -lm"])
fi

##bswap_64()##
AC_CHECK_HEADERS([byteswap.h])

##static linking##
AC_ARG_ENABLE([static],
    AS_HELP_STRING(
        [--enable-static],
        [enable static linking])
)
AM_CONDITIONAL(ENABLE_STATIC, test "$enable_static" = yes)

AC_CHECK_LIB([tinfo], [initscr], tinfo=1,
    AC_MSG_WARN([*** tinfo library (libtinfo) not found]))
AM_CONDITIONAL(ENABLE_TINFO, test "$tinfo" = 1)

##memory tracing##
AC_ARG_ENABLE([mtrace],
    AS_HELP_STRING(
        [--enable-mtrace],
        [enable memory tracing])
)
if test "$enable_mtrace" = "yes"; then
AC_CHECK_HEADERS([mtrace.h], ,
    AC_MSG_ERROR([*** mtrace header file (mtrace.h) not found]))
fi
enable_memtrace=$enable_mtrace
AM_CONDITIONAL(ENABLE_MEMTRACE, test "$enable_memtrace" = yes)


##extra test
AC_ARG_ENABLE([fs-test],
    AS_HELP_STRING(
        [--enable-fs-test],
        [enable file system clone/restore test])
)
AM_CONDITIONAL(ENABLE_FS_TEST, test "$enable_fs_test" = yes)
AM_CONDITIONAL(ENABLE_FEATURE_TEST, test "$enable_feature_test" = yes)

##bash-completion
PKG_CHECK_VAR([bashcompletiondir], [bash-completion], [completionsdir], [],
    [bashcompletiondir="${sysconfdir}/bash_completion.d"])
AC_SUBST(bashcompletiondir)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	po/Makefile.in
	docs/Makefile
	tests/Makefile
	])
AC_OUTPUT

echo ""
echo "Support File System:"
echo "fuse.......... ${enable_fuse:-no}, ${fuse_version:-}"
echo "ext2/3/4...... ${enable_extfs:-no}, ${extfs_version:-}"
echo "reiserfs...... ${enable_reiserfs:-no}, ${reiserfs_version:-}"
echo "reiser4....... ${enable_reiser4:-no}, ${reiser4_version:-}"
echo "xfs........... ${enable_xfs:-no}, ${xfs_version:-}"
echo "ntfs.......... ${enable_ntfs:-no}, ${ntfs_version:-}"
echo "fat12/16/32... ${enable_fat:-no}, ${fat_version:-}"
echo "exfat......... ${enable_exfat:-no}, ${exfat_version:-}"
echo "hfs plus...... ${enable_hfsp:-no}, ${hfs_plus_version:-}"
echo "apfs.......... ${enable_apfs:-no}, ${apfs_version:-}"
echo "ufs .......... ${enable_ufs:-no}, ${ufs_version:-}"
echo "vmfs ......... ${enable_vmfs:-no}, ${vmfs_version:-}"
echo "jfs .......... ${enable_jfs:-no}, ${jfs_version:-}"
echo "btrfs......... ${enable_btrfs:-no}, ${btrfs_version:-}"
echo "minix......... ${enable_minix:-no}, ${minix_version:-}"
echo "f2fs.......... ${enable_f2fs:-no}, ${f2fs_version:-}"
echo "nilfs2.........${enable_nilfs2:-no}, ${nilfs2_version:-}"
#echo $supported_fs
