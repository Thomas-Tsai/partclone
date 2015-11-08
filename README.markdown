Partclone is a project similar to the well-known backup utility "Partition Image" a.k.a partimage. Partclone provides utilities to back up and restore used-blocks of a partition and it is designed for higher compatibility of the file system by using existing library, e.g. e2fslibs is used to read and write the ext2 partition.

Partclone now supports ext2, ext3, ext4, hfs+, reiserfs, reiser4, btrfs, vmfs3, vmfs5, xfs, jfs, ufs, ntfs, fat(12/16/32), exfat...

We made some utilies:

* partclone.ext2, partclone.ext3, partclone.extfs
* partclone.ext4
* partclone.reiserfs
* partclone.reiser4
* partclone.xfs
* partclone.exfat
* partclone.fat (fat 12, fat 16, fat 32)
* partclone.ntfs
* partclone.hfsp
* partclone.vmfs(v3 and v5)
* partclone.ufs
* partclone.jfs
* partclone.btrfs
* partclone.minix
* partclone.f2fs
* partclone.nilfs
* partclone.info 
* partclone.restore
* partclone.chkimg
* partclone.dd
...

Basic Usage:

 - clone partition to image

    `partclone.ext4 -d -c -s /dev/sda1 -o sda1.img`

 - restore image to partition

    `partclone.ext4 -d -r -s sda1.img -o /dev/sda1`

 - partiiton to partition clone

    `partclone.ext4 -d -b -s /dev/sda1 -o /dev/sdb1`

 - display image information

    `partclone.info -s sda1.img`

 - check image

    `partclone.chkimg -s sda1.img`

For more info about partclone, check our website http://partclone.org or github-wiki.
