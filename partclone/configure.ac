AC_INIT([Partclone], [0.0.9], [thomas@nchc.org.tw])
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


supported_fs="ext2/3"
##ext2/3##
dnl Check for EXT2/3
AC_CHECKING([ for EXT2/3 Library and Header files ... ])
AC_CHECK_HEADERS([ext2fs/ext2fs.h], ,
  AC_MSG_ERROR([*** EXT2/3 header files (ext2fs/ext2fs.h) not found])
)
AC_CHECK_LIB([ext2fs], [ext2fs_initialize], ,
    AC_MSG_ERROR([*** EXT2/3 library (libext2fs) not found]))

##enable-all##
AC_ARG_ENABLE(test,
	        AS_HELP_STRING(--enable-all,enable all supported file system),
	        enable_all=yes,
)
AM_CONDITIONAL(ENABLE_ALL, test "$enable_all" = yes)
if test "$enable_all" = "yes"; then
enable_xfs="yes"
enable_reiserfs="yes"
enable_reiser4="yes"
enable_hfsp="yes"
enable_fat="yes"
enable_ntfs="yes"
fi

##XFS##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-xfs,enable XFS file system), 
	enable_xfs=yes,
)
AM_CONDITIONAL(ENABLE_XFS, test "$enable_xfs" = yes)

if test "$enable_xfs" = "yes"; then
#check library of some filesystems
dnl Check runtime library which depended by XFS
AC_CHECK_LIB([rt], [aio_init], , 
    AC_MSG_ERROR([*** runtime library (libc6) not found])
)

dnl Check for XFS 
AC_CHECKING([ for XFS Library and Header files ... ])
AC_CHECK_HEADERS([xfs/libxfs.h], ,
   AC_MSG_ERROR([*** XFS header (libxfs.h) not found])
)
supported_fs=$supported_fs" xfs"
fi

##reiserfs##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-reiserfs,enable REISERFS 3.6/3.6 file system), 
	enable_reiserfs=yes,
)
AM_CONDITIONAL(ENABLE_REISERFS, test "$enable_reiserfs" = yes)

if test "$enable_reiserfs" = "yes"; then

dnl Check for REISERFS
AC_CHECKING([ for Reiserfs Library and Header files ... ])
AC_CHECK_HEADERS([reiserfs/reiserfs.h], ,
    AC_MSG_ERROR([*** reiserfs header files (reiserfs/reiserfs.h) not found])
)
AC_CHECK_LIB([reiserfs], [reiserfs_fs_open], ,
    AC_MSG_ERROR([*** Reiserfs library (libreiserfs) not found]))
AC_CHECK_LIB([dal], [file_dal_open], ,
    AC_MSG_ERROR([*** Reiserfs depend library (libdal) not found]))
supported_fs=$supported_fs" reiserfs"

fi

##reiser4##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-reiser4,enable Reiser4 file system), 
	enable_reiser4=yes,
)
AM_CONDITIONAL(ENABLE_REISER4, test "$enable_reiser4" = yes)

if test "$enable_reiser4" = "yes"; then

dnl Check for REISER4
dnl Check uuid library which depended by REISER4
AC_CHECK_LIB([uuid], [uuid_unparse], , 
    AC_MSG_ERROR([*** uuid library (libuuid) not found])
)

AC_CHECKING([ for Reiser4 Library and Header files ... ])
AC_CHECK_HEADERS([reiser4/libreiser4.h], ,
  AC_MSG_ERROR([*** reiser4 header files (reiser4/libreiser4.h) not found])
)
#AC_CHECK_LIB([reiser4 -laal], [reiser4_init], ,
#    AC_MSG_ERROR([*** Reiser4 library (libreiser4) not found]))
AC_CHECK_LIB([aal], [aal_device_open], ,
    AC_MSG_ERROR([*** Reiser4 depend library (libaal) not found]))
#end of check fs
supported_fs=$supported_fs" reiser4"
fi

##hfs plus##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-hfsp,enable HFS plus file system), 
	enable_hfsp=yes,
)
AM_CONDITIONAL(ENABLE_HFSP, test "$enable_hfsp" = yes)

if test "$enable_hfsp" = yes; then
supported_fs=$supported_fs" hfs-plus"
fi

##fat plus##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-fat,enable FAT file system), 
	enable_fat=yes,
)
AM_CONDITIONAL(ENABLE_FAT, test "$enable_fat" = yes)

if test "$enable_fat" = yes; then
supported_fs=$supported_fs" fat"
fi

##NTFS##
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-ntfs,enable NTFS file system), 
	enable_ntfs=yes,
)
AM_CONDITIONAL(ENABLE_NTFS, test "$enable_ntfs" = yes)

if test "$enable_ntfs" = "yes"; then
#check library of some filesystems

dnl Check for NTFS 
AC_CHECKING([ for NTFS Library and Header files ... ])
AC_CHECK_HEADERS([ntfs/version.h], ,
   AC_MSG_ERROR([*** NTFS(ntfsprog) header not found])
AC_CHECK_LIB([ntfs], [ntfs_mount], ,
        AC_MSG_ERROR([*** ntfs depend library (libntfs) not found]))
)
AC_MSG_CHECKING(version of libntfs)
ntfs_version=`gcc $srcdir/src/deplib_version.c -o $srcdir/get_lib_version -lntfs -DNTFS`
ntfs_version=`$srcdir/get_lib_version ntfs | cut -d':' -f1`
ntfs_version_major=`$srcdir/get_lib_version ntfs | cut -d':' -f2`
ntfs_version_minor=`$srcdir/get_lib_version ntfs | cut -d':' -f3`
if test -z "$ntfs_version"; then
        ntfs_version="?"
	ntfs_version_major="?"
	ntfs_version_minor="?"
	AC_MSG_RESULT($ntfs_version)
        AC_MSG_ERROR(Please check your libntfs to 9:0:0 or newer version!)

fi

if test $ntfs_version -lt 9 ; then
ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; bad"
AC_MSG_RESULT($ntfs_version)
AC_MSG_ERROR(Please upgrade your libntfs to 9:0:0 or newer version!)
elif test $ntfs_version -eq 9 ; then
ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; suggest you upgrade the library!"
AC_MSG_RESULT($ntfs_version)
else
ntfs_version="$ntfs_version:$ntfs_version_major:$ntfs_version_minor; ok!"
AC_MSG_RESULT($ntfs_version)
fi

supported_fs=$supported_fs" ntfs"
fi


##libncursesw##
AC_ARG_ENABLE(test,
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
AC_ARG_ENABLE(test,
	AS_HELP_STRING(--enable-static,enable static linking), 
	enable_static=yes,
)
AM_CONDITIONAL(ENABLE_STATIC, test "$enable_static" = yes)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	po/Makefile.in
	docs/Makefile
	docs/partclone.extfs.8
	docs/partclone.hfsp.8
	docs/partclone.fat.8
	docs/partclone.ntfs.8
	docs/partclone.reiserfs.8
	docs/partclone.reiser4.8
	docs/partclone.xfs.8
	])
AC_OUTPUT

echo ""
echo "Support File System:"
echo $supported_fs
