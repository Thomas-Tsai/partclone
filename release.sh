#!/bin/bash
set -e

packbase="$HOME/download/pre-release/"
[ ! -d $presrc ] && mkdir $presrc
rm -rf $packbase/*
presrc="$HOME/download/pre-release/src"
ptlpath="$HOME/partclone"
[ ! -d $presrc ] && mkdir $presrc

GPGID="0x68cdf01d"
SRC=0
DPKG=0
TAGS=0
PO=0
LOG=0
SYNC=0
VER=0
VERSION=auto
gVERSION="HEAD"
TEST=0
_dch_options="-b"
is_release=1
USAGE(){
    
    cat << EOF
	$0 is script to auto release partclone. This cript will create partclone 
	tar ball file and auto build  debian package with my key.
	It's only for developer used.

	recognized flags are:
	-s, --src       build tar ball
	-d, --dpkg	build dpkg files
	-p, --po	run make po
	-t, --test	run make check
	-v, --ver	update version.h
	-l, --sync-log	update Changelog
	-f, --sync-fs	update source i.e btrfs
	-g, --git	give git commit version like b655 or 0.2.54
	-a, --tags	add version as git tag
	-n, --no_push   do NOT auto push to git repo
	-h, --help      show this help
EOF

}

check_option(){
    while [ $# -gt 0 ]; do
	case "$1" in
	-g|--git)
	    shift
	    if [ -z "$(echo $1 |grep ^-.)" ]; then
		gVERSION=$1
		shift
		pushd $ptlpath
		#git pull
		vgit=$(git log -1 $gVERSION)
		if [ -z "$vgit" ]; then
		    echo "can't find git version $VERSION"> 2& 
		    exit 1
		fi
		popd
	    fi
	    ;;
	-d|--dpkg)
	    DPKG=1
	    shift
	    ;;
	-s|--src)
	    SRC=1
	    shift
	    ;;
	-t|--test)
	    TEST=1
	    shift
	    ;;
	-a|--tags)
	    TAGS=1
	    shift
	    ;;
	-p|--po)
	    PO=1
	    shift
	    ;;
	-l|--sync-log)
	    LOG=1
	    shift
	    ;;
	-f|--sync-fs)
	    SYNC=1
	    shift
	    ;;
	-n|--no_push)
	    is_release=0
	    shift
	    ;;
	-v|--ver)
	    VER=1
	    shift
	    ;;
	-h|--help)
	    USAGE >& 2
	    exit 2
	    ;;
	-*)
	    echo "${0}: ${1}: invalid option" >&2
	    USAGE >& 2
	    exit 2
	    ;;
	*)
	    break
	    ;;
	esac
    done
}

# exit if not git
is_git(){
    
  GIT_REPOSITORY=`git remote -v | grep partclone`
  if [ "${GIT_REPOSITORY}" = "" ];
  then
    echo "not git repository?"
    exit 0
  fi

}

check_version(){
    pushd $ptlpath
    is_git
    #git pull
    git fetch --tags

    if [ "$gVERSION" != "HEAD" ]; then
	git checkout $gVERSION -b 'g_$gVERSION'
	ptlversion=$(grep ^PACKAGE_VERSION= configure | sed s/PACKAGE_VERSION//g | sed s/[\'=]//g)
	if [ "$gVERSION" == "$ptlversion" ]; then
	    VERSION=$ptlversion
	    _dch_options="-v $VERSION-2 -b"
	else
	VERSION=$ptlversion~git$gVERSION
	_dch_options="-v $VERSION-1 -b"
	fi
    fi
    
    if [ "$VERSION" == "auto" ];then
	VERSION=$(grep ^PACKAGE_VERSION= configure | sed s/PACKAGE_VERSION//g | sed s/[\'=]//g)
	gVERSION="HEAD"
	_dch_options=" -v $VERSION-1 -b"
    fi
    popd

    if [ -z "$VERSION" ]; then
	echo "can't find version $VERSION"> 2&
	exit 1
    fi

}

tarball (){
	pushd $ptlpath
	## release tar ball
	    autoreconf
	    update_version
	    update_po
	    sync_log
	    git add -u
	    git commit -m "release $VERSION"
	    git archive --format=tar --prefix=partclone-$VERSION/  $gVERSION | gzip > $presrc/partclone-$VERSION.tar.gz
	    git archive --format=tar --prefix=partclone-$VERSION/  $gVERSION | gzip > $presrc/partclone_$VERSION.orig.tar.gz

	    [ is_releae == 1 ] && git push
	popd
}

dpkg_package() {
    pushd $packbase
	if [ -f $presrc/partclone_$VERSION.orig.tar.gz  ]; then
	    cp $presrc/partclone_$VERSION.orig.tar.gz $packbase/ && tar -zxvf partclone_$VERSION.orig.tar.gz
	else
	    echo "copy partclone_$VERSION.orig.tar.gz to $presrc/partclone_$VERSION.orig.tar.gz first"
	    echo "i.e: wget http://partclone.nchc.org.tw/download/source/partclone_$VERSION.orig.tar.gz -P $presrc/"
	    exit
	fi
	pushd partclone-$VERSION
	    cp -r README.Packages/debian debian
	    debuild -k$GPGID
	popd
    popd
}

push_tags(){
    pushd $ptlpath
	git tag -f -a $VERSION -m "release $VERSION"
	git push --tags
    popd
}

sync_log(){
    pushd $ptlpath
    is_git
    # get source code from Jim Meyering at 
    # http://git.mymadcat.com/index.php/p/tracker/source/tree/master/gitlog-to-changelog
    perl gitlog-to-changelog --since 1999-01-01 > ChangeLog
    popd
    cp -r README.Packages/debian debian
	    dch $_dch_options
	    dch -r
    cp -r debian README.Packages/
}

update_po(){
    pushd $ptlpath
    make -C po/ update-po
    popd
}

sync_fs(){
    BTRFS_SOURCE="bitops.h ctree.h extent_io.c inode-item.c math.h radix-tree.c root-tree.c ulist.c volumes.c btrfsck.h dir-item.c extent_io.h inode-map.c print-tree.c radix-tree.h send.h ulist.h volumes.h btrfs-list.c disk-io.c extent-tree.c ioctl.h print-tree.h raid6.c send-stream.c utils.c btrfs-list.h disk-io.h file-item.c kerncompat.h qgroup.c rbtree.c send-stream.h utils.h crc32c.c free-space-cache.c list.h qgroup.h rbtree.h send-utils.c utils-lib.c rbtree-utils.c crc32c.h extent-cache.c free-space-cache.h list_sort.c qgroup-verify.c repair.c send-utils.h uuid-tree.c ctree.c extent-cache.h hash.h list_sort.h qgroup-verify.h repair.h transaction.h version.h rbtree-utils.h rbtree_augmented.h"

    echo "manual run : cp $BTRFS_SOURCE $ptlpath/src/btrfs/"

}

update_version(){
    pushd $ptlpath
    file="$ptlpath/src/version.h"
    is_git
    GIT_REVISION=`git log -1 | grep ^commit | sed 's/commit //'`
    #GIT_REVISION_UPSTREAM=`git log master -1 | grep commit | sed 's/commit //'`
    git_ver="none"

    #if [ "${GIT_REVISION_UPSTREAM}" != "" ]; then
    #    git_ver=$GIT_REVISION_UPSTREAM
    #el
    if [ "${GIT_REVISION}" != "" ]; then
	git_ver=$GIT_REVISION
    else
	git_ver="none"
    fi

    if [ "$git_ver" != "none" ];
    then

    echo "create ${file}"
    cat > ${file} << EOF
/* DO NOT EDIT THIS FILE - IT IS REGENERATED AT EVERY COMPILE -
 * IT GIVES BETTER TRACKING OF DEVELOPMENT VERSIONS
 * WHETHER THEY ARE BUILT BY OTHERS OR DURING DEVELOPMENT OR FOR THE
 * OFFICIAL PARTCLONE RELEASES.
 */
#define git_version  "$git_ver"

EOF
   fi
}

# main
check_option "$@"
check_version
[ $SRC == 1 ]	&& tarball
[ $DPKG == 1 ]	&& dpkg_package
[ $LOG == 1 ]	&& sync_log
[ $TAGS == 1 ]	&& push_tags
[ $PO == 1 ]	&& update_po
[ $SYNC == 1 ]	&& sync_fs
[ $VER == 1 ]	&& update_version
