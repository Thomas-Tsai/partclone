#include <stdio.h>
#include <vmfs/vmfs.h>

vmfs_fs_t *fs;
vmfs_dir_t *root_dir;

/// open device
static int pvmfs_fs_open(char* device){
    vmfs_lvm_t *lvm;
    vmfs_flags_t flags;

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

    //log_mesg(3, 0, 0, fs_opt.debug, "%s: device %s\n", device);
    if (!(lvm = vmfs_lvm_create(flags))) {
	fprintf(stderr, "Unable to create LVM structure\n");
	return 1;
    }

    if (vmfs_lvm_add_extent(lvm, vmfs_vol_open(device, flags)) == -1) {
	fprintf(stderr, "Unable to open device/file \"%s\".\n", device);
	return 1;
    }

    if (!(fs = vmfs_fs_create(lvm))) {
	fprintf(stderr, "Unable to open filesystem\n");
	return 1;
    }

    if (vmfs_fs_open(fs) == -1) {
	fprintf(stderr, "Unable to open volume.\n");
	return 1;
    }

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0)))) {
	fprintf(stderr, "Unable to open root directory\n");
	return 1;
    }

    return 0;

}

/// close device
static void pvmfs_vmfs_close(){
    vmfs_dir_close(root_dir);
    vmfs_fs_close(fs);
}

int main (int argc, char **argv){
    
    char* source;			/// source data
    int ret;

    if( !argv[1]) {
	fprintf(stderr, "Please give the device name.\n");
	return 1;
    }

    source=argv[1];
    ret = pvmfs_fs_open(source);
    if(ret == 0){
	fprintf(stdout, "TYPE=\"vmfs\"\n");
    }else{
        fprintf(stderr, "error exit\n");
	return 1;
    }

    return 0;
}
