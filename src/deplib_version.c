#include <stdio.h>

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
    } else if (strcmp(libfs, "reiser4") == 0) {
	err = libpreiser4_version();
    }

    if(err == 1){
	printf("0\n");
    }
    return 0;

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
