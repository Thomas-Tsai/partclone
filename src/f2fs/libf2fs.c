/**
 * libf2fs.c
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 *             http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define _LARGEFILE64_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <mntent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
#include <linux/fs.h>

#include "f2fs_fs.h"

struct f2fs_configuration config;

void ASCIIToUNICODE(u_int16_t *out_buf, u_int8_t *in_buf)
{
	u_int8_t *pchTempPtr = in_buf;
	u_int16_t *pwTempPtr = out_buf;

	while (*pchTempPtr != '\0') {
		*pwTempPtr = (u_int16_t)*pchTempPtr;
		pchTempPtr++;
		pwTempPtr++;
	}
	*pwTempPtr = '\0';
	return;
}

int log_base_2(u_int32_t num)
{
	int ret = 0;
	if (num <= 0 || (num & (num - 1)) != 0)
		return -1;

	while (num >>= 1)
		ret++;
	return ret;
}

/*
 * f2fs bit operations
 */
static const int bits_in_byte[256] = {
	0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
	4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8,
};

int get_bits_in_byte(unsigned char n)
{
	return bits_in_byte[n];
}

int set_bit(unsigned int nr,void * addr)
{
	int             mask, retval;
	unsigned char   *ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << ((nr & 0x07));
	retval = mask & *ADDR;
	*ADDR |= mask;
	return retval;
}

int clear_bit(unsigned int nr, void * addr)
{
	int             mask, retval;
	unsigned char   *ADDR = (unsigned char *) addr;

	ADDR += nr >> 3;
	mask = 1 << ((nr & 0x07));
	retval = mask & *ADDR;
	*ADDR &= ~mask;
	return retval;
}

int test_bit(unsigned int nr, const void * addr)
{
	const __u32 *p = (const __u32 *)addr;

	nr = nr ^ 0;

	return ((1 << (nr & 31)) & (p[nr >> 5])) != 0;
}

int f2fs_test_bit(unsigned int nr, const char *p)
{
	int mask;
	char *addr = (char *)p;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	return (mask & *addr) != 0;
}

int f2fs_set_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr |= mask;
	return ret;
}

int f2fs_clear_bit(unsigned int nr, char *addr)
{
	int mask;
	int ret;

	addr += (nr >> 3);
	mask = 1 << (7 - (nr & 0x07));
	ret = mask & *addr;
	*addr &= ~mask;
	return ret;
}

static inline unsigned long __ffs(unsigned long word)
{
	int num = 0;

#if BITS_PER_LONG == 64
	if ((word & 0xffffffff) == 0) {
		num += 32;
		word >>= 32;
	}
#endif
	if ((word & 0xffff) == 0) {
		num += 16;
		word >>= 16;
	}
	if ((word & 0xff) == 0) {
		num += 8;
		word >>= 8;
	}
	if ((word & 0xf) == 0) {
		num += 4;
		word >>= 4;
	}
	if ((word & 0x3) == 0) {
		num += 2;
		word >>= 2;
	}
	if ((word & 0x1) == 0)
		num += 1;
	return num;
}

unsigned long find_next_bit(const unsigned long *addr, unsigned long size,
                unsigned long offset)
{
        const unsigned long *p = addr + BIT_WORD(offset);
        unsigned long result = offset & ~(BITS_PER_LONG-1);
        unsigned long tmp;

        if (offset >= size)
                return size;
        size -= result;
        offset %= BITS_PER_LONG;
        if (offset) {
                tmp = *(p++);
                tmp &= (~0UL << offset);
                if (size < BITS_PER_LONG)
                        goto found_first;
                if (tmp)
                        goto found_middle;
                size -= BITS_PER_LONG;
                result += BITS_PER_LONG;
        }
        while (size & ~(BITS_PER_LONG-1)) {
                if ((tmp = *(p++)))
                        goto found_middle;
                result += BITS_PER_LONG;
                size -= BITS_PER_LONG;
        }
        if (!size)
                return result;
        tmp = *p;

found_first:
        tmp &= (~0UL >> (BITS_PER_LONG - size));
        if (tmp == 0UL)		/* Are any bits set? */
                return result + size;   /* Nope. */
found_middle:
        return result + __ffs(tmp);
}

/*
 * Hashing code adapted from ext3
 */
#define DELTA 0x9E3779B9

static void TEA_transform(unsigned int buf[4], unsigned int const in[])
{
	__u32 sum = 0;
	__u32 b0 = buf[0], b1 = buf[1];
	__u32 a = in[0], b = in[1], c = in[2], d = in[3];
	int     n = 16;

	do {
		sum += DELTA;
		b0 += ((b1 << 4)+a) ^ (b1+sum) ^ ((b1 >> 5)+b);
		b1 += ((b0 << 4)+c) ^ (b0+sum) ^ ((b0 >> 5)+d);
	} while (--n);

	buf[0] += b0;
	buf[1] += b1;

}

static void str2hashbuf(const char *msg, int len, unsigned int *buf, int num)
{
	unsigned pad, val;
	int i;

	pad = (__u32)len | ((__u32)len << 8);
	pad |= pad << 16;

	val = pad;
	if (len > num * 4)
		len = num * 4;
	for (i = 0; i < len; i++) {
		if ((i % 4) == 0)
			val = pad;
		val = msg[i] + (val << 8);
		if ((i % 4) == 3) {
			*buf++ = val;
			val = pad;
			num--;
		}
	}
	if (--num >= 0)
		*buf++ = val;
	while (--num >= 0)
		*buf++ = pad;

}

/**
 * Return hash value of directory entry
 * @param name          dentry name
 * @param len           name lenth
 * @return              return on success hash value, errno on failure
 */
f2fs_hash_t f2fs_dentry_hash(const char *name, int len)
{
	__u32 hash;
	f2fs_hash_t	f2fs_hash;
	const char	*p;
	__u32 in[8], buf[4];

	/* special hash codes for special dentries */
	if (name[0] == '.') {
		if (name[1] == '\0') {
			f2fs_hash = F2FS_DOT_HASH;
			goto exit;
		}
		if (name[1] == '.' && name[2] == '\0') {
			f2fs_hash = F2FS_DDOT_HASH;
			goto exit;
		}
	}

	/* Initialize the default seed for the hash checksum functions */
	buf[0] = 0x67452301;
	buf[1] = 0xefcdab89;
	buf[2] = 0x98badcfe;
	buf[3] = 0x10325476;

	p = name;
	while (len > 0) {
		str2hashbuf(p, len, in, 4);
		TEA_transform(buf, in);
		len -= 16;
		p += 16;
	}
	hash = buf[0];

	f2fs_hash = hash;
exit:
	f2fs_hash &= ~F2FS_HASH_COL_BIT;

	return f2fs_hash;
}

unsigned int addrs_per_inode(struct f2fs_inode *i)
{
	if (i->i_inline & F2FS_INLINE_XATTR)
		return DEF_ADDRS_PER_INODE - F2FS_INLINE_XATTR_ADDRS;
	return DEF_ADDRS_PER_INODE;
}

/*
 * CRC32
 */
#define CRCPOLY_LE 0xedb88320

u_int32_t f2fs_cal_crc32(u_int32_t crc, void *buf, int len)
{
	int i;
	unsigned char *p = (unsigned char *)buf;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++)
			crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
	}
	return crc;
}

int f2fs_crc_valid(u_int32_t blk_crc, void *buf, int len)
{
	u_int32_t cal_crc = 0;

	cal_crc = f2fs_cal_crc32(F2FS_SUPER_MAGIC, buf, len);

	if (cal_crc != blk_crc)	{
		DBG(0,"CRC validation failed: cal_crc = %u \
			blk_crc = %u buff_size = 0x%x",
			cal_crc, blk_crc, len);
		return -1;
	}
	return 0;
}

/*
 * device information
 */
void f2fs_init_configuration(struct f2fs_configuration *c)
{
	c->sector_size = DEFAULT_SECTOR_SIZE;
	c->sectors_per_blk = DEFAULT_SECTORS_PER_BLOCK;
	c->blks_per_seg = DEFAULT_BLOCKS_PER_SEGMENT;

	/* calculated by overprovision ratio */
	c->reserved_segments = 48;
	c->overprovision = 5;
	c->segs_per_sec = 1;
	c->secs_per_zone = 1;
	c->heap = 1;
	c->vol_label = "";
	c->device_name = NULL;
	c->trim = 1;
}

static int is_mounted(const char *mpt, const char *device)
{
	FILE *file = NULL;
	struct mntent *mnt = NULL;

	file = setmntent(mpt, "r");
	if (file == NULL)
		return 0;

	while ((mnt = getmntent(file)) != NULL) {
		if (!strcmp(device, mnt->mnt_fsname))
			break;
	}
	endmntent(file);
	return mnt ? 1 : 0;
}

int f2fs_dev_is_umounted(struct f2fs_configuration *c)
{
	struct stat st_buf;
	int ret = 0;

	ret = is_mounted(MOUNTED, c->device_name);
	if (ret) {
		MSG(0, "\tError: Not available on mounted device!\n");
		return -1;
	}

	/*
	 * if failed due to /etc/mtab file not present
	 * try with /proc/mounts.
	 */
	ret = is_mounted("/proc/mounts", c->device_name);
	if (ret) {
		MSG(0, "\tError: Not available on mounted device!\n");
		return -1;
	}

	/*
	 * If f2fs is umounted with -l, the process can still use
	 * the file system. In this case, we should not format.
	 */
	if (stat(c->device_name, &st_buf) == 0 && S_ISBLK(st_buf.st_mode)) {
		int fd = open(c->device_name, O_RDONLY | O_EXCL);

		if (fd >= 0) {
			close(fd);
		} else if (errno == EBUSY) {
			MSG(0, "\tError: In use by the system!\n");
			return -1;
		}
	}
	return 0;
}

int f2fs_get_device_info(struct f2fs_configuration *c)
{
	int32_t fd = 0;
	int32_t sector_size;
	struct stat stat_buf;
	struct hd_geometry geom;

	fd = open(c->device_name, O_RDWR);
	if (fd < 0) {
		MSG(0, "\tError: Failed to open the device!\n");
		return -1;
	}
	c->fd = fd;

	if (fstat(fd, &stat_buf) < 0 ) {
		MSG(0, "\tError: Failed to get the device stat!\n");
		return -1;
	}

	if (S_ISREG(stat_buf.st_mode)) {
		c->total_sectors = stat_buf.st_size / c->sector_size;
	} else if (S_ISBLK(stat_buf.st_mode)) {
		if (ioctl(fd, BLKSSZGET, &sector_size) < 0) {
			MSG(0, "\tError: Using the default sector size\n");
		} else {
			if (c->sector_size < sector_size) {
				MSG(0, "\tError: Cannot set the sector size to:"
					" %d as the device does not support"
					"\nSetting the sector size to : %d\n",
					c->sector_size, sector_size);
				c->sector_size = sector_size;
				c->sectors_per_blk = PAGE_SIZE / sector_size;
			}
		}

		if (ioctl(fd, BLKGETSIZE, &c->total_sectors) < 0) {
			MSG(0, "\tError: Cannot get the device size\n");
			return -1;
		}

		if (ioctl(fd, HDIO_GETGEO, &geom) < 0)
			c->start_sector = 0;
		else
			c->start_sector = geom.start;
	} else {
		MSG(0, "\tError: Volume type is not supported!!!\n");
		return -1;
	}

	//MSG(0, "Info: sector size = %u\n", c->sector_size);
	//MSG(0, "Info: total sectors = %"PRIu64" (in 512bytes)\n",
	//				c->total_sectors);
	if (c->total_sectors <
			(F2FS_MIN_VOLUME_SIZE / DEFAULT_SECTOR_SIZE)) {
		MSG(0, "Error: Min volume size supported is %d\n",
				F2FS_MIN_VOLUME_SIZE);
		return -1;
	}

	return 0;
}

/*
 * IO interfaces
 */
int dev_read(void *buf, __u64 offset, size_t len)
{
	if (lseek64(config.fd, (off64_t)offset, SEEK_SET) < 0)
		return -1;
	if (read(config.fd, buf, len) < 0)
		return -1;
	return 0;
}

int dev_write(void *buf, __u64 offset, size_t len)
{
	if (lseek64(config.fd, (off64_t)offset, SEEK_SET) < 0)
		return -1;
	if (write(config.fd, buf, len) < 0)
		return -1;
	return 0;
}

int dev_read_block(void *buf, __u64 blk_addr)
{
	return dev_read(buf, blk_addr * F2FS_BLKSIZE, F2FS_BLKSIZE);
}

int dev_read_blocks(void *buf, __u64 addr, __u32 nr_blks)
{
	return dev_read(buf, addr * F2FS_BLKSIZE, nr_blks * F2FS_BLKSIZE);
}
