#include <stdio.h>
#include <string.h>

#ifdef EXTFS
    #include <ext2fs/ext2fs.h>
#elif REISERFS
    #include <reiserfs/reiserfs.h>
#elif REISER4
    #include <reiser4/libreiser4.h>
#elif XFS
#elif HFSPLUS
#elif FAT
#elif NTFS
    #include <ntfs/version.h>
#elif VMFS
    #include <vmfs/vmfs.h>
#elif BTRFS
    #include "btrfs/version.h"
#endif

int main(int argc, char **argv){
    char *libfs;
    int err=0;

    if(argv[1]){
        libfs = argv[1];
    } else {
        libfs="xxx";
    }

    //printf("%s\n", libfs);
    if (strcmp(libfs, "ntfs") == 0){
	err = libntfs_version();
    } else if (strcmp(libfs, "extfs") == 0) {
	err = libextfs_version();
    } else if (strcmp(libfs, "reiserfs") == 0) {
	err = libreiserfs_version();
    } else if (strcmp(libfs, "vmfs") == 0) {
	err = libvmfs_version();
    } else if (strcmp(libfs, "reiser4") == 0) {
	err = libpreiser4_version();
    } else if (strcmp(libfs, "btrfs") == 0) {
	err = libbtrfs_version();
    }

    if(err == 1){
	printf("0\n");
    }
    return 0;

}

int libvmfs_version(){
#ifdef VMFS
    char *version;
    version = "0.2.0";
#ifdef VMFS5_ZLA_BASE
    version = "0.2.5";
#endif
    printf("%s\n", version);
#endif
}

int libntfs_version(){
#ifdef NTFS
    printf("%s\n", ntfs_libntfs_version());
    return 0;
#endif
    return 1;
}

int libextfs_version(){
#ifdef EXTFS

    const char      *lib_ver;
    ext2fs_get_library_version(&lib_ver, 0);
    printf("%s\n", lib_ver);
    return 0;
#endif
    return 1;
}

int libpreiser4_version(){
#ifdef REISER4
    const char      *lib_ver;
    lib_ver = (const char *)libreiser4_version();
    printf("%s\n", lib_ver);
    return 0;
#endif
    return 1;
}

int libreiserfs_version(){
#ifdef REISERFS
    const char      *lib_ver;
    lib_ver = (const char *)libreiserfs_get_version();
    printf("%s\n", lib_ver);
    return 0;
#endif
    return 1;
}

int libbtrfs_version(){
#ifdef BTRFS
    printf("%s\n", BTRFS_BUILD_VERSION);
    return 0;
#endif
    return 1;
}
