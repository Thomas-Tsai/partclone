/*
 * Copyright (C) 2007 Oracle.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License v2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 021110-1307, USA.
 */

#ifndef __BTRFS_UTILS_H__
#define __BTRFS_UTILS_H__

#include "kerncompat.h"
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include "kernel-lib/list.h"
#include "kernel-shared/volumes.h"

struct list_head;

enum exclusive_operation {
	BTRFS_EXCLOP_NONE,
	BTRFS_EXCLOP_BALANCE,
	BTRFS_EXCLOP_BALANCE_PAUSED,
	BTRFS_EXCLOP_DEV_ADD,
	BTRFS_EXCLOP_DEV_REMOVE,
	BTRFS_EXCLOP_DEV_REPLACE,
	BTRFS_EXCLOP_RESIZE,
	BTRFS_EXCLOP_SWAP_ACTIVATE,
	BTRFS_EXCLOP_UNKNOWN = -1,
};

static inline u32 btrfs_type_to_imode(u8 type)
{
	static u32 imode_by_btrfs_type[] = {
		[BTRFS_FT_REG_FILE]	= S_IFREG,
		[BTRFS_FT_DIR]		= S_IFDIR,
		[BTRFS_FT_CHRDEV]	= S_IFCHR,
		[BTRFS_FT_BLKDEV]	= S_IFBLK,
		[BTRFS_FT_FIFO]		= S_IFIFO,
		[BTRFS_FT_SOCK]		= S_IFSOCK,
		[BTRFS_FT_SYMLINK]	= S_IFLNK,
	};

	return imode_by_btrfs_type[(type)];
}

/* 2 for "0x", 2 for each byte, plus nul */
#define BTRFS_CSUM_STRING_LEN		(2 + 2 * BTRFS_CSUM_SIZE + 1)
void btrfs_format_csum(u16 csum_type, const u8 *data, char *output);
int get_fs_info(const char *path, struct btrfs_ioctl_fs_info_args *fi_args,
		struct btrfs_ioctl_dev_info_args **di_ret);
int get_fsid(const char *path, u8 *fsid, int silent);
int get_fsid_fd(int fd, u8 *fsid);
int get_fs_exclop(int fd);
int check_running_fs_exclop(int fd, enum exclusive_operation start, bool enqueue);
const char *get_fs_exclop_name(int op);

int check_arg_type(const char *input);
int ask_user(const char *question);
int lookup_path_rootid(int fd, u64 *rootid);
int find_mount_fsroot(const char *subvol, const char *subvolid, char **mount);
int find_mount_root(const char *path, char **mount_root);
int get_df(int fd, struct btrfs_ioctl_space_args **sargs_ret);

const char *subvol_strip_mountpoint(const char *mnt, const char *full_path);
int find_next_key(struct btrfs_path *path, struct btrfs_key *key);
const char* btrfs_group_type_str(u64 flag);
const char* btrfs_group_profile_str(u64 flag);

int count_digits(u64 num);
u64 div_factor(u64 num, int factor);

unsigned long total_memory(void);

void print_device_info(struct btrfs_device *device, char *prefix);
void print_all_devices(struct list_head *devices);

#define BTRFS_BCONF_UNSET	-1
#define BTRFS_BCONF_QUIET	 0
/*
 * Global program state, configurable by command line and available to
 * functions without extra context passing.
 */
struct btrfs_config {
	unsigned int output_format;

	/*
	 * Values:
	 *   BTRFS_BCONF_QUIET
	 *   BTRFS_BCONF_UNSET
	 *   > 0: verbose level
	 */
	int verbose;
	/* Command line request to skip any modification actions. */
	int dry_run;
	struct list_head params;
};
extern struct btrfs_config bconf;

struct config_param {
	struct list_head list;
	const char *key;
	const char *value;
};

void btrfs_config_init(void);
void bconf_be_verbose(void);
void bconf_be_quiet(void);
void bconf_add_param(const char *key, const char *value);
void bconf_save_param(const char *str);
void bconf_set_dry_run(void);
bool bconf_is_dry_run(void);
const char *bconf_param_value(const char *key);

/* Pseudo random number generator wrappers */
int rand_int(void);
u8 rand_u8(void);
u16 rand_u16(void);
u32 rand_u32(void);
u64 rand_u64(void);
unsigned int rand_range(unsigned int upper);
void init_rand_seed(u64 seed);

bool get_env_bool(const char *env_name);

char *btrfs_test_for_multiple_profiles(int fd);
int btrfs_warn_multiple_profiles(int fd);
void btrfs_warn_experimental(const char *str);

/* An error code to error string mapping for the kernel error codes */
static inline char *btrfs_err_str(enum btrfs_err_code err_code)
{
	switch (err_code) {
	case BTRFS_ERROR_DEV_RAID1_MIN_NOT_MET:
		return "unable to go below two devices on raid1";
	case BTRFS_ERROR_DEV_RAID1C3_MIN_NOT_MET:
		return "unable to go below three devices on raid1c3";
	case BTRFS_ERROR_DEV_RAID1C4_MIN_NOT_MET:
		return "unable to go below four devices on raid1c4";
	case BTRFS_ERROR_DEV_RAID10_MIN_NOT_MET:
		return "unable to go below four/two devices on raid10";
	case BTRFS_ERROR_DEV_RAID5_MIN_NOT_MET:
		return "unable to go below two devices on raid5";
	case BTRFS_ERROR_DEV_RAID6_MIN_NOT_MET:
		return "unable to go below three devices on raid6";
	case BTRFS_ERROR_DEV_TGT_REPLACE:
		return "unable to remove the dev_replace target dev";
	case BTRFS_ERROR_DEV_MISSING_NOT_FOUND:
		return "no missing devices found to remove";
	case BTRFS_ERROR_DEV_ONLY_WRITABLE:
		return "unable to remove the only writeable device";
	case BTRFS_ERROR_DEV_EXCL_RUN_IN_PROGRESS:
		return "add/delete/balance/replace/resize operation "
			"in progress";
	default:
		return NULL;
	}
}

#endif
