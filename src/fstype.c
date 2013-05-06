#include <stdio.h>
#include <vmfs/vmfs.h>
//#include <vmfs/vmfs_fs.h>

vmfs_fs_t *fs;
vmfs_volume_t *vol;
vmfs_dir_t *root_dir;

/// open device
static int pvmfs_fs_open(char* device){
    vmfs_flags_t flags;
    char *mdev[] = {device, NULL};

    vmfs_host_init();
    flags.packed = 0;
    flags.allow_missing_extents = 1;

#ifdef VMFS5_ZLA_BASE
    if (!(fs = vmfs_fs_open(mdev, flags))) {
	fprintf(stderr, "type: Unable to open volume (vmfs5).\n");
	return 1;
    }
    vol = vmfs_vol_open(device, flags);
#else
    vmfs_lvm_t *lvm;
    if (!(lvm = vmfs_lvm_create(flags))) {
	fprintf(stderr, "Unable to create LVM structure\n");
	return 1;
    }
    vol = vmfs_vol_open(device, flags);
    if (vmfs_lvm_add_extent(lvm, vol) == -1) {
	fprintf(stderr, "Unable to open device/file \"%s\".\n", device);
	return 1;
    }

    if (!(fs = vmfs_fs_create(lvm))) {
	fprintf(stderr, "Unable to open filesystem\n");
	return 1;
    }

    if (vmfs_fs_open(fs) == -1) {

	fprintf(stderr, "type: Unable to open volume.\n");
	return 1;
    }
#endif

    if (!(root_dir = vmfs_dir_open_from_blkid(fs,VMFS_BLK_FD_BUILD(0,0,0)))) {
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
	fprintf(stdout, "TYPE=\"vmfs%i\"\n", vol->vol_info.version);
    }else{
	fprintf(stderr, "error exit\n");
	return 1;
    }
    pvmfs_vmfs_close();

    return 0;
}
