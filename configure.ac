AC_INIT([Partclone], [0.2.56], [thomas@nchc.org.tw])
#AC_CONFIG_AUX_DIR([build-aux])
#AC CONFIG SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])
AC_PROG_CC
AC_PROG_INSTALL
AC_PATH_PROG(RM, rm, rm)
AC_PROG_LN_S

# Enable large file support.
AC_SYS_LARGEFILE

# default value##
supported_fs=""

##enable-all##
AC_ARG_ENABLE(all,
	        AS_HELP_STRING(--enable-all,enable all supported file system),
	        enable_all=yes,
)
AM_CONDITIONAL(ENABLE_ALL, test "$enable_all" = yes)
if test "$enable_all" = "yes"; then
enable_xfs="yes"
enable_extfs="yes"
enable_reiserfs="yes"
enable_reiser4="yes"
enable_hfsp="yes"
enable_fat="yes"
enable_ntfs="yes"
enable_ufs="yes"
enable_vmfs="yes"
enable_jfs="yes"
enable_btrfs="yes"
fi

dnl Check for uuid 
PKG_CHECK_MODULES(UUID, uuid,,exit)
uuidcfg=`pkg-config --cflags --libs uuid`

AC_CHECK_LIB([pthread], [pthread_create], [], AC_MSG_ERROR([*** pthread library (libpthread) not found]))

##ext2/3##
AC_ARG_ENABLE(extfs,
	AS_HELP_STRING(--enable-extfs,enable ext2/3/4 file system), 
	enable_extfs=yes,
	enable_extfs=no
)
AM_CONDITIONAL(ENABLE_EXTFS, test "$enable_extfs" = yes)

if test "$enable_extfs" = "yes"; then
#check library of some filesystems
dnl Check for EXT2/3
AC_CHECKING([ for EXT2/3 Library and Header files ... ])
AC_CHECK_HEADERS([ext2fs/ext2fs.h], ,
  AC_MSG_ERROR([*** EXT2/3 header files (ext2fs/ext2fs.h) not found])
)
AC_CHECK_LIB([ext2fs], [ext2fs_initialize], true,
    AC_MSG_ERROR([*** EXT2/3 library (libext2fs) not found]))

AC_MSG_CHECKING(version of libextfs)
extfs_version=`gcc  $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lext2fs -DEXTFS`
extfs_version=`$srcdir/get_lib_version extfs`
extfs_version=`$srcdir/get_lib_version extfs | cut -d'.' -f1`
extfs_version_major=`$srcdir/get_lib_version extfs | cut -d'.' -f2`
extfs_version_minor=`$srcdir/get_lib_version extfs | cut -d'.' -f3`
AC_DEFINE_UNQUOTED([EXTFS_VERSION], "${extfs_version}.${extfs_version_major}", ["define e2fs version"])
if test -z "$extfs_version"; then
    extfs_version="?"
    extfs_version_major="?"
    extfs_version_minor="?"
    extfs_version="$extfs_version.$extfs_version_major.$extfs_version_minor; bad"
    AC_MSG_RESULT($extfs_version)
    AC_MSG_ERROR(Please check your libextfs!)
fi

if test $extfs_version -lt 1 ; then
    extfs_version="$extfs_version.$extfs_version_major.$extfs_version_minor; bad"
    AC_MSG_RESULT($extfs_version)
    AC_MSG_ERROR(Please upgrade your libextfs to 1.41 or newer version!)
elif test $extfs_version -ge 1 -a $extfs_version_major -lt 41  ; then
    extfs_version="$extfs_version.$extfs_version_major.$extfs_version_minor; bad"
    AC_MSG_RESULT($extfs_version)
    AC_MSG_ERROR(Please upgrade your libextfs to 1.42 or newer version!)
elif test $extfs_version -ge 1 -a $extfs_version_major -lt 42  ; then    
    extfs_version="$extfs_version.$extfs_version_major.$extfs_version_minor; suggest you upgrade the library!"
    AC_MSG_RESULT($extfs_version)
    AC_MSG_WARN(Suggest upgrade your libextfs to 1.42 or newer version!)
    AC_DEFINE_UNQUOTED([EXTFS_1_41], "1", ["define e2fs version"])
else
    extfs_version="$extfs_version.$extfs_version_major.$extfs_version_minor; ok!"
    AC_MSG_RESULT($extfs_version)
fi
supported_fs=$supported_fs" extfs"

fi
#end of check extfs

##XFS##
AC_ARG_ENABLE(xfs,
	AS_HELP_STRING(--enable-xfs,enable XFS file system), 
	enable_xfs=yes,
	enable_xfs=no
)
AM_CONDITIONAL(ENABLE_XFS, test "$enable_xfs" = yes)

if test "$enable_xfs" = "yes"; then
#check library of some filesystems
dnl Check runtime library which depended by XFS
AC_CHECK_LIB([rt], [aio_init], true, 
    AC_MSG_ERROR([*** runtime library (libc6) not found])
)

dnl Check for XFS 
AC_CHECKING([ for XFS Library and Header files ... ])
AC_CHECK_HEADERS([xfs/libxfs.h], ,
   AC_MSG_ERROR([*** XFS header (libxfs.h) not found])
)
supported_fs=$supported_fs" xfs"
fi
xfs_version="version unknown, suggest 3.1.1"
#end of check xfs

##reiserfs##
AC_ARG_ENABLE(reiserfs,
	AS_HELP_STRING(--enable-reiserfs,enable REISERFS 3.6/3.6 file system), 
	enable_reiserfs=yes,
	enable_reiserfs=no
)
AM_CONDITIONAL(ENABLE_REISERFS, test "$enable_reiserfs" = yes)

if test "$enable_reiserfs" = "yes"; then

dnl Check for REISERFS
AC_CHECKING([ for Reiserfs Library and Header files ... ])
AC_CHECK_HEADERS([reiserfs/reiserfs.h], ,
    AC_MSG_ERROR([*** reiserfs header files (reiserfs/reiserfs.h) not found])
)
AC_CHECK_LIB([reiserfs], [reiserfs_fs_open], true,
    AC_MSG_ERROR([*** Reiserfs library (libreiserfs) not found]))
AC_CHECK_LIB([dal], [file_dal_open], true,
    AC_MSG_ERROR([*** Reiserfs depend library (libdal) not found]))
AC_MSG_CHECKING(version of libreiserfs)
reiserfs_version=`gcc  $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lreiserfs -DREISERFS`
reiserfs_version=`$srcdir/get_lib_version reiserfs`
AC_DEFINE_UNQUOTED([REISERFS_VERSION], "${reiserfs_version}", ["define reiserfs version"])
reiserfs_version=`$srcdir/get_lib_version reiserfs | cut -d'.' -f1`
reiserfs_version_major=`$srcdir/get_lib_version reiserfs | cut -d'.' -f2`
reiserfs_version_minor=`$srcdir/get_lib_version reiserfs | cut -d'.' -f3`
reiserfs_version_minor_2=`$srcdir/get_lib_version reiserfs | cut -d'.' -f4`
if test -z "$reiserfs_version"; then
    reiserfs_version="?"
    reiserfs_version_major="?"
    reiserfs_version_minor="?"
    reiserfs_version_minor_2="?"
    reiserfs_version="$reiserfs_version.$reiserfs_version_major.$reiserfs_version_minor.$reiserfs_version_minor_2; bad"
    AC_MSG_RESULT($reiserfs_version)
    AC_MSG_ERROR(Please check your libreiserfs!)
fi

if test $reiserfs_version_major -lt 3 ; then
    reiserfs_version="$reiserfs_version.$reiserfs_version_major.$reiserfs_version_minor.$reiserfs_version_minor_2; bad"
    AC_MSG_RESULT($reiserfs_version)
    AC_MSG_ERROR(Please upgrade your libreiserfs to 0.3.0.5 or newer version!)
elif test $reiserfs_version_major -ge 3 -a $reiserfs_version_minor_2 -lt 5  ; then
    reiserfs_version="$reiserfs_version.$reiserfs_version_major.$reiserfs_version_minor.$reiserfs_version_minor_2; suggest you upgrade the library!"
    AC_MSG_RESULT($reiserfs_version)
    AC_MSG_ERROR(Please upgrade your libreiserfs to 0.3.0.5 or newer version!)
else
    reiserfs_version="$reiserfs_version.$reiserfs_version_major.$reiserfs_version_minor.$reiserfs_version_minor_2; ok!"
    AC_MSG_RESULT($reiserfs_version)
fi 
supported_fs=$supported_fs" reiserfs"

fi
#end of check reiserfs

##reiser4##
AC_ARG_ENABLE(reiser4,
	AS_HELP_STRING(--enable-reiser4,enable Reiser4 file system), 
	enable_reiser4=yes,
	enable_reiser4=no
)
AM_CONDITIONAL(ENABLE_REISER4, test "$enable_reiser4" = yes)

if test "$enable_reiser4" = "yes"; then

dnl Check for REISER4
dnl Check uuid library which depended by REISER4
AC_CHECKING([ for Reiser4 Library and Header files ... ])
AC_CHECK_HEADERS([reiser4/libreiser4.h], ,
  AC_MSG_ERROR([*** reiser4 header files (reiser4/libreiser4.h) not found])
)
#AC_CHECK_LIB([reiser4 -laal], [reiser4_init], ,
#    AC_MSG_ERROR([*** Reiser4 library (libreiser4) not found]))
AC_CHECK_LIB([aal], [aal_device_open], true,
    AC_MSG_ERROR([*** Reiser4 depend library (libaal) not found]))
AC_MSG_CHECKING(version of libreiser4)
reiser4_version=`gcc  $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lreiser4 -laal -DREISER4`
reiser4_version=`$srcdir/get_lib_version reiser4`
AC_DEFINE_UNQUOTED([REISER4_VERSION], "${reiser4_version}", ["define reiser4 version"])
reiser4_version=`$srcdir/get_lib_version reiser4 | cut -d'.' -f1`
reiser4_version_major=`$srcdir/get_lib_version reiser4 | cut -d'.' -f2`
reiser4_version_minor=`$srcdir/get_lib_version reiser4 | cut -d'.' -f3`
if test -z "$reiser4_version"; then
    reiser4_version="?"
    reiser4_version_major="?"
    reiser4_version_minor="?"
    reiser4_version="$reiser4_version.$reiser4_version_major.$reiser4_version_minor; bad"
    AC_MSG_RESULT($reiser4_version)
    AC_MSG_ERROR(Please check your libreiser4!)
fi

if test $reiser4_version -lt 1 ; then
    reiser4_version="$reiser4_version.$reiser4_version_major.$reiser4_version_minor; bad"
    AC_MSG_RESULT($reiser4_version)
    AC_MSG_ERROR(Please upgrade your libreiser4 to 1.0.6 or newer version!)
elif test $reiser4_version -ge 1 -a $reiser4_version_minor -lt 6  ; then
    reiser4_version="$reiser4_version.$reiser4_version_major.$reiser4_version_minor; suggest you upgrade the library!"
    AC_MSG_RESULT($reiser4_version)
    AC_MSG_ERROR(Please upgrade your libreiser4 to 1.0.6 or newer version!)
else
    reiser4_version="$reiser4_version.$reiser4_version_major.$reiser4_version_minor; ok!"
    AC_MSG_RESULT($reiser4_version)
fi   
supported_fs=$supported_fs" reiser4"
fi
#end of check reiser4

##hfs plus##
AC_ARG_ENABLE(hfsp,
	AS_HELP_STRING(--enable-hfsp,enable HFS plus file system), 
	enable_hfsp=yes,
	enable_hfsp=no
)
AM_CONDITIONAL(ENABLE_HFSP, test "$enable_hfsp" = yes)

if test "$enable_hfsp" = "yes"; then
supported_fs=$supported_fs" hfs-plus"
hfs_plus_version="build-in"
fi
#end of check hfsplus

##fat##
AC_ARG_ENABLE(fat,
	AS_HELP_STRING(--enable-fat,enable FAT file system), 
	enable_fat=yes,
	enable_fat=no
)
AM_CONDITIONAL(ENABLE_FAT, test "$enable_fat" = yes)

if test "$enable_fat" = "yes"; then
supported_fs=$supported_fs" fat"
fat_version="build-in"
fi
#end of check fat

##NTFS##
AC_ARG_ENABLE(ntfs,
	AS_HELP_STRING(--enable-ntfs,enable NTFS file system), 
	enable_ntfs=yes,
	enable_ntfs=no
)

if test "$enable_ntfs" = "yes"; then
    ntfs_3g_h=0
    ntfs_3g_l=0
    ntfsprogs_h=0
    ntfsprogs_l=0
    #check library of some filesystems
    dnl Check for NTFS-3g
    AC_CHECKING([ for NTFS-3g Library and Header files ... ])
    AC_CHECK_HEADERS([ntfs-3g/misc.h], ntfs_3g_h=1,
    AC_MSG_WARN([*** NTFS(libntfs-3g-dev) header not found]))
    AC_CHECK_LIB([ntfs-3g], [ntfs_mount], ntfs_3g_l=1,
    AC_MSG_WARN([*** ntfs depend library (libntfs-3g) not found]))

    dnl Check for NTFSPROGS
    AC_CHECKING([ for NTFS Library and Header files ... ])
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
    AC_DEFINE_UNQUOTED([NTFS_VERSION], "${ntfs_version}", ["define ntfs version"])
elif test "$ntfsprogs_h" = 1 -a "$ntfsprogs_l" = 1; then
    AC_MSG_CHECKING(version of libntfs)
    ntfs_version=`gcc  $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lntfs -DNTFS`
    ntfs_version=`$srcdir/get_lib_version ntfs`
    AC_DEFINE_UNQUOTED([NTFS_VERSION], "${ntfs_version}", ["define ntfs version"])
    ntfs_version=`$srcdir/get_lib_version ntfs | cut -d':' -f1`
    ntfs_version_major=`$srcdir/get_lib_version ntfs | cut -d':' -f2`
    ntfs_version_minor=`$srcdir/get_lib_version ntfs | cut -d':' -f3`
    if test -z "$ntfs_version"; then
        ntfs_version="?"
        ntfs_version_major="?"
        ntfs_version_minor="?"
        ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; bad"
        AC_MSG_RESULT($ntfs_version)
        AC_MSG_ERROR(Please check your libntfs!)
    fi

    if test "$ntfs_version" -lt 9 ; then
        ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; bad"
        AC_MSG_RESULT($ntfs_version)
        AC_MSG_ERROR(Please upgrade your libntfs to 9:0:0 or newer version!)
    elif test "$ntfs_version" -eq 9 ; then
        ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; suggest you upgrade the library!"
        AC_MSG_RESULT($ntfs_version)
    else
        ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; ok!"
        AC_MSG_RESULT($ntfs_version)
        supported_fs=$supported_fs" ntfs"
    fi
    define_ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor"
else
    if test "$enable_ntfs" = "yes"; then
	AC_MSG_ERROR(Can't find ntfs library!)
    fi
fi
#end of check ntfs


##UFS##
AC_ARG_ENABLE(ufs,
	AS_HELP_STRING(--enable-ufs,enable UFS(1/2) file system), 
	enable_ufs=yes,
	enable_ufs=no
)
AM_CONDITIONAL(ENABLE_UFS, test "$enable_ufs" = yes)

if test "$enable_ufs" = "yes"; then
#check library of some filesystems

dnl Check for UFS 
AC_CHECKING([ for UFS Library ... ])
AC_CHECK_LIB([ufs], [ufs_disk_fillout], true,
        AC_MSG_ERROR([*** ufs depend library (libufs2) not found]))
supported_fs=$supported_fs" ufs"
ufs_version="unknown, suggest 7.0-2"
fi
#end of check ufs

##VMFS##
AC_ARG_ENABLE(vmfs,
	AS_HELP_STRING(--enable-vmfs,enable vmfs file system), 
	enable_vmfs=yes,
	enable_vmfs=no
)
AM_CONDITIONAL(ENABLE_VMFS, test "$enable_vmfs" = yes)

if test "$enable_vmfs" = "yes"; then
#check library of some filesystems

dnl Check for VMFS 
##AC_CHECKING([ for VMFS Library and Header files ... ])
##AC_CHECK_HEADERS([uuid/uuid.h vmfs/vmfs.h], ,
##   AC_MSG_ERROR([*** VMFS(vmfs-tools) header not found]))
AC_CHECKING([for VMFS Library files ... ])
AC_CHECK_LIB([vmfs -luuid], [vmfs_host_init], true,
        AC_MSG_ERROR([*** vmfs depend library (libvmfs) not found]))
supported_fs=$supported_fs" vmfs"
vmfs_version=`gcc  $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lvmfs -DVMFS $uuidcfg`
vmfs_version=`$srcdir/get_lib_version vmfs`
fi
#end of check vmfs

##JFS##
AC_ARG_ENABLE(jfs,
	AS_HELP_STRING(--enable-jfs,enable jfs file system), 
	enable_jfs=yes,
	enable_jfs=no
)
AM_CONDITIONAL(ENABLE_JFS, test "$enable_jfs" = yes)

if test "$enable_jfs" = "yes"; then
#check library of some filesystems

dnl Check for JFS 
##AC_CHECKING([ for JFS Library and Header files ... ])
##AC_CHECK_HEADERS([uuid/uuid.h jfs/jfs_superblock.h], ,
##   AC_MSG_ERROR([*** JFS(libjfs) header not found]))
AC_CHECKING([for JFS Library files ... ])
AC_CHECK_LIB([jfs -luuid], [ujfs_get_superblk], true,
        AC_MSG_ERROR([*** jfs depend library (libjfs) not found]))
supported_fs=$supported_fs" jfs"
jfs_version="version unknown, suggest 1.1.12"
fi
#end of check jfs

##btrfs##
AC_ARG_ENABLE(btrfs,
	AS_HELP_STRING(--enable-btrfs,enable btrfs file system), 
	enable_btrfs=yes,
	enable_btrfs=no
)
AM_CONDITIONAL(ENABLE_BTRFS, test "$enable_btrfs" = yes)

if test "$enable_btrfs" = yes; then
supported_fs=$supported_fs" btrfs"
btrfs_version="0.19"
fi
#end of check btrfs


##libncursesw##
AC_ARG_ENABLE(ncursesw,
	AS_HELP_STRING(--enable-ncursesw,enable TEXT User Interface), 
	enable_ncursesw=yes,
)
AM_CONDITIONAL(ENABLE_NCURSESW, test "$enable_ncursesw" = yes)

if test "$enable_ncursesw" = "yes"; then

dnl Check for NCURSESW
AC_CHECKING([ for Ncursesw Library and Header files ... ])
AC_CHECK_HEADERS([ncurses.h], ,
    AC_MSG_ERROR([*** ncurses header files (ncurses.h) not found])
)
AC_CHECK_LIB([ncursesw], [initscr], ,
    AC_MSG_ERROR([*** Ncursesw library (libncursesw5) not found]))
fi


##static linking##
AC_ARG_ENABLE(static,
	AS_HELP_STRING(--enable-static, enable static linking), 
	enable_static=yes,
)
AM_CONDITIONAL(ENABLE_STATIC, test "$enable_static" = yes)

AC_CHECK_LIB([tinfo], [initscr], tinfo=1,
    AC_MSG_WARN([*** tinfo library (libtinfo) not found]))
AM_CONDITIONAL(ENABLE_TINFO, test "$tinfo" = 1)

##memory tracing##
AC_ARG_ENABLE(mtrace,
	AS_HELP_STRING(--enable-mtrace, enable memory tracing), 
	enable_memtrace=yes,
)
AM_CONDITIONAL(ENABLE_MEMTRACE, test "$enable_memtrace" = yes)


##extra test
AC_ARG_ENABLE(fs-test,
        AS_HELP_STRING(--enable-fs-test, enable file system clone/restore test),
        enable_fs_test=yes,
)
AM_CONDITIONAL(ENABLE_FS_TEST, test "$enable_fs_test" = yes)

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
echo "ext2/3/4...... $enable_extfs, $extfs_version"
echo "reiserfs...... $enable_reiserfs, $reiserfs_version"
echo "reiser4....... $enable_reiser4, $reiser4_version"
echo "xfs........... $enable_xfs, $xfs_version"
echo "ntfs.......... $enable_ntfs, $ntfs_version"
echo "fat12/16/32... $enable_fat, $fat_version"
echo "hfs plus...... $enable_hfsp, $hfs_plus_version"
echo "ufs .......... $enable_ufs, $ufs_version"
echo "vmfs ......... $enable_vmfs, $vmfs_version"
echo "jfs .......... $enable_jfs, $jfs_version"
echo "btrfs......... $enable_btrfs, $btrfs_version"
#echo $supported_fs
