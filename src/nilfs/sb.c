/*
 * sb.c - NILFS super block access library
 *
 * Copyright (C) 2005-2012 Nippon Telegraph and Telephone Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Written by Ryusuke Konishi <konishi.ryusuke@lab.ntt.co.jp>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif	/* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif	/* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif	/* HAVE_UNISTD_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif	/* HAVE_SYS_TYPES_H */

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif	/* HAVE_FCNTL_H */

#if HAVE_LIMITS_H
#include <limits.h>
#endif	/* HAVE_LIMITS_H */

#if HAVE_LINUX_TYPES_H
#include <linux/types.h>
#endif	/* HAVE_LINUX_TYPES_H */

#include <linux/fs.h>

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif	/* HAVE_SYS_IOCTL_H */

#include <errno.h>
#include <assert.h>
#include "nilfs.h"

#define NILFS_MAX_SB_SIZE	1024

extern __u32 crc32_le(__u32 seed, unsigned char const *data, size_t length);

static __u32 nilfs_sb_check_sum(struct nilfs_super_block *sbp)
{
	__u32 seed, crc;
	__le32 sum;

	seed = le32_to_cpu(sbp->s_crc_seed);
	sum = sbp->s_sum;
	sbp->s_sum = 0;
	crc = crc32_le(seed, (unsigned char *)sbp, le16_to_cpu(sbp->s_bytes));
	sbp->s_sum = sum;
	return crc;
}

static int nilfs_sb_is_valid(struct nilfs_super_block *sbp, int check_crc)
{
	__u32 crc;

	if (le16_to_cpu(sbp->s_magic) != NILFS_SUPER_MAGIC)
		return 0;
	if (le16_to_cpu(sbp->s_bytes) > NILFS_MAX_SB_SIZE)
		return 0;
	if (!check_crc)
		return 1;

	crc = nilfs_sb_check_sum(sbp);

	return crc == le32_to_cpu(sbp->s_sum);
}

static int nilfs_sb2_offset_is_too_small(struct nilfs_super_block *sbp,
					 __u64 sb2_offset)
{
	return sb2_offset < ((le64_to_cpu(sbp->s_nsegments) *
			      le32_to_cpu(sbp->s_blocks_per_segment)) <<
			     (le32_to_cpu(sbp->s_log_block_size) +
			      NILFS_SB_BLOCK_SIZE_SHIFT));
}

static int __nilfs_sb_read(int devfd, struct nilfs_super_block **sbp,
			   __u64 *offsets)
{
	__u64 devsize, sb2_offset;
	int invalid_fs = 0;

	sbp[0] = malloc(NILFS_MAX_SB_SIZE);
	sbp[1] = malloc(NILFS_MAX_SB_SIZE);
	if (sbp[0] == NULL || sbp[1] == NULL)
		goto failed;

	if (ioctl(devfd, BLKGETSIZE64, &devsize) != 0)
		goto failed;

	if (lseek(devfd, NILFS_SB_OFFSET_BYTES, SEEK_SET) >= 0 &&
	    read(devfd, sbp[0], NILFS_MAX_SB_SIZE) >= 0) {
		if (nilfs_sb_is_valid(sbp[0], 0))
			goto sb1_ok;
		invalid_fs = 1;
	}

	free(sbp[0]);
	sbp[0] = NULL;
sb1_ok:

	sb2_offset = NILFS_SB2_OFFSET_BYTES(devsize);
	if (offsets) {
		offsets[0] = NILFS_SB_OFFSET_BYTES;
		offsets[1] = sb2_offset;
	}

	if (lseek(devfd, sb2_offset, SEEK_SET) >= 0 &&
	    read(devfd, sbp[1], NILFS_MAX_SB_SIZE) >= 0) {
		if (nilfs_sb_is_valid(sbp[1], 0) &&
		    !nilfs_sb2_offset_is_too_small(sbp[1], sb2_offset))
			goto sb2_ok;
		invalid_fs = 1;
	}

	free(sbp[1]);
	sbp[1] = NULL;
sb2_ok:

	if (!sbp[0] && !sbp[1]) {
		if (invalid_fs)
			errno = EINVAL;
		goto failed;
	}

	return 0;

 failed:
	free(sbp[0]);  /* free(NULL) is just ignored */
	free(sbp[1]);
	return -1;
}

struct nilfs_super_block *nilfs_sb_read(int devfd)
{
	struct nilfs_super_block *sbp[2];

	if (__nilfs_sb_read(devfd, sbp, NULL))
		return NULL;

	if (!sbp[0]) {
		sbp[0] = sbp[1];
		sbp[1] = NULL;
	}

	free(sbp[1]);

	return sbp[0];
}

int nilfs_sb_write(int devfd, struct nilfs_super_block *sbp, int mask)
{
	__u64 offsets[2];
	struct nilfs_super_block *sbps[2];
	ssize_t count;
	int i, ret;
	__u32 crc;

	assert(devfd >= 0);

	if (sbp == NULL)
		return -1;

	if (__nilfs_sb_read(devfd, sbps, offsets))
		return -1;

	ret = 0;
	for (i = 0; i < 2; i++) {
		if (!sbps[i])
			continue;

		if (mask & NILFS_SB_LABEL)
			memcpy(sbps[i]->s_volume_name,
			       sbp->s_volume_name, sizeof(sbp->s_volume_name));

		if (mask & NILFS_SB_COMMIT_INTERVAL)
			sbps[i]->s_c_interval = sbp->s_c_interval;

		if (mask & NILFS_SB_BLOCK_MAX)
			sbps[i]->s_c_block_max = sbp->s_c_block_max;

		if (mask & NILFS_SB_UUID)
			memcpy(sbps[i]->s_uuid, sbp->s_uuid,
			       sizeof(sbp->s_uuid));

		if (mask & NILFS_SB_FEATURES) {
			sbps[i]->s_feature_compat = sbp->s_feature_compat;
			sbps[i]->s_feature_compat_ro = sbp->s_feature_compat_ro;
			sbps[i]->s_feature_incompat = sbp->s_feature_incompat;
		}

		crc = nilfs_sb_check_sum(sbps[i]);
		sbps[i]->s_sum = cpu_to_le32(crc);
		if (lseek(devfd, offsets[i], SEEK_SET) > 0) {
			count = write(devfd, sbps[i], NILFS_MAX_SB_SIZE);
			if (count < NILFS_MAX_SB_SIZE) {
				ret = -1;
				break;
			}
		}
	}

	free(sbps[0]);
	free(sbps[1]);

	return ret;
}
