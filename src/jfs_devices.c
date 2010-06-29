/*
 *   Copyright (c) International Business Machines Corp., 2000-2003
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#include <config.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_SYS_MOUNT_H
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/mount.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#if defined(__DragonFly__)
#include <machine/param.h>
#include <sys/diskslice.h>
#endif

#ifdef HAVE_SYS_DISKLABEL_H
#include <sys/disklabel.h>
#endif

#include <jfs/jfs_types.h>
#include <jfs/jfs_filsys.h>
#include "jfs_devices.h"


/// libfs/debug.h
#ifndef  __H_DEBUG__

#define __H_DEBUG__

#include <stdio.h>

#ifdef TRACE
#define DBG_TRACE(a) {printf a; fflush(stdout);}
#else
#define DBG_TRACE(a) ;
#endif

#ifdef TRACE_IO
#define DBG_IO(a) {printf a; fflush(stderr);}
#else
#define DBG_IO(a) ;
#endif

#ifdef TRACE_ERROR
#define DBG_ERROR(a) {printf a; fflush(stdout);}
#else
#define DBG_ERROR(a) ;
#endif

#endif


#if defined(__linux__) && defined(_IO) && !defined(BLKGETSIZE64)
#define BLKGETSIZE64 _IOR(0x12, 114, size_t)
#endif
#if defined(__linux__) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE _IO(0x12,96)	/* return device size (sectors) */
#endif

/*
 * NAME: ujfs_device_is_valid
 *
 * FUNCTION: Check device validity by examining stat modes.
 *
 * PRE CONDITIONS: 'device' must refer to an open device handle.
 *
 * PARAMETERS:
 *      device_handle  - open device handle to check
 *      st             - stat information if device handle not provided
 *
 * RETURNS: 0 if successful; anything else indicates failures
 */
int ujfs_device_is_valid(FILE *device_handle, struct stat *st)
{
	struct stat stat_data;
	int rc = 0;

	if (device_handle != NULL) {
		rc = fstat(fileno(device_handle), &stat_data);
		if (rc)
			return -1;
		st = &stat_data;
	}
	else if (st == NULL) {
		return -1;
	}

	/* Do we have a block special device or regular file? */
#if defined(__DragonFly__)
	if (!S_ISCHR(st->st_mode) && !S_ISREG(st->st_mode))
#else /* __linux__ etc. */
	if (!S_ISBLK(st->st_mode) && !S_ISREG(st->st_mode))
#endif
		return -1;

	return (rc);
}

/*
 * NAME: ujfs_get_dev_size
 *
 * FUNCTION: Uses the device driver interface to determine the raw capacity of
 *      the specified device.
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      device  - device
 *      size    - filled in with size of device; not modified if failure occurs
 *
 * NOTES:
 *
 * DATA STRUCTURES:
 *
 * RETURNS: 0 if successful; anything else indicates failures
 */
int ujfs_get_dev_size(FILE *device, int64_t *size)
{

	off_t Starting_Position;	/* position within file/device upon
					 * entry to this function. */
	off_t Current_Position = 16777215;	/* position we are attempting
						 * to read from. */
	off_t Last_Valid_Position = 0;	/* Last position we could successfully
					 * read from. */
	off_t First_Invalid_Position;	/* first invalid position we attempted
					 * to read from/seek to. */
	int Seek_Result;	/* value returned by lseek. */
	size_t Read_Result = 0;	/* value returned by read. */
	int rc;
	struct stat stat_data;
	int devfd = fileno(device);

	rc = fstat(devfd, &stat_data);
	if (!rc && S_ISREG(stat_data.st_mode)) {
		/* This is a regular file.  */
		*size = (int64_t) ((stat_data.st_size / 1024) * 1024);
		return NO_ERROR;
	}
#ifdef BLKGETSIZE64
	{
		uint64_t sz;
		if (ioctl(devfd, BLKGETSIZE64, &sz) >= 0) {
			*size = sz;
			return 0;
		}
	}
#endif
#ifdef BLKGETSIZE
	{
		unsigned long num_sectors = 0;

		if (ioctl(devfd, BLKGETSIZE, &num_sectors) >= 0) {
			/* for now, keep size as multiple of 1024, *
			 * not 512, so eliminate any odd sector.   */
			*size = PBSIZE * (int64_t) ((num_sectors / 2) * 2);
			return NO_ERROR;
		}
	}
#endif
#if defined(__DragonFly__)
	{
		struct diskslices dss;
		struct disklabel dl;
		struct diskslice *sliceinfo;
		int slice;
		dev_t dev = stat_data.st_rdev;

		rc = ioctl(devfd, DIOCGSLICEINFO, &dss);
		if (rc < 0)
			return -1;

		slice = dkslice(dev);
		sliceinfo = &dss.dss_slices[slice];

		DBG_TRACE(("ujfs_get_device_size: slice = %d\n", slice));

		if (sliceinfo) {
			if (slice == WHOLE_DISK_SLICE || slice == 0) {
				*size = (int64_t) sliceinfo->ds_size * dss.dss_secsize;
				DBG_TRACE(("ujfs_get_device_size: slice represents disk\n"));
			} else {
				if (sliceinfo->ds_label) {
					DBG_TRACE(("ujfs_get_device_size: slice has disklabel\n"));
					rc = ioctl(devfd, DIOCGDINFO, &dl);
					if (!rc) {
						*size = (int64_t) dl.d_secperunit * dss.dss_secsize;
					} else {
						return (-1);
					}
				}
			}
		} else {
			return (-1);
		}

		DBG_TRACE(("ujfs_get_device_size: size in bytes = %ld\n", *size));
		DBG_TRACE(("ujfs_get_device_size: size in megabytes = %ld\n",
			*size / (1024 * 1024)));

		return 0;
	}
#endif
#if defined(HAVE_SYS_DISKLABEL_H) && !defined(__DragonFly__)
	{
		struct disklabel dl;
		struct partition * part;
		dev_t dev = stat_data.st_rdev;

		rc = ioctl(devfd, DIOCGDINFO, &dl);
		if (rc < 0)
			return -1;
		part = dl.d_partitions + DISKPART(dev);

		*size = (dl.d_secsize * part->p_size);

		DBG_TRACE(("ujfs_get_device_size: size in bytes = %ld\n", 
			*size));
		DBG_TRACE(("ujfs_get_device_size: size in megabytes = %ld\n",
			*size / (1024 * 1024)));

		return NO_ERROR;
	}
#endif


	/*
	 * If the ioctl above fails or is undefined, use a binary search to
	 * find the last byte in the partition.  This works because an lseek to
	 * a position within the partition does not return an error while an
	 * lseek to a position beyond the end of the partition does.  Note that
	 * some SCSI drivers may log an 'access beyond end of device' error
	 * message.
	 */

	/* Save the starting position so that we can restore it when we are
	 * done! */
	Starting_Position = ftello(device);
	if (Starting_Position < 0)
		return ERROR_SEEK;

	/*
	 * Find a position beyond the end of the partition.  We will start by
	 * attempting to seek to and read the 16777216th byte in the partition.
	 * We start here because a JFS partition must be at least this big.  If
	 * it is not, then we can not format it as JFS.
	 */
	do {
		/* Seek to the location we wish to test. */
		Seek_Result = fseeko(device, Current_Position, SEEK_SET);
		if (Seek_Result == 0) {
			/* Can we read from this location? */
			Read_Result = fgetc(device);
			if (Read_Result != EOF) {
				/* The current test position is valid.  Save it
				 * for future reference. */
				Last_Valid_Position = Current_Position;

				/* Lets calculate the next location to test. */
				Current_Position = ((Current_Position + 1) * 2)
						   - 1;

			}
		}
	} while ((Seek_Result == 0) && (Read_Result == 1));

	/*
	 * We have exited the while loop, which means that Current Position is
	 * beyond the end of the partition or is unreadable due to a hardware
	 * problem (bad block).  Since the odds of hitting a bad block are very
	 * low, we will ignore that condition for now.  If time becomes
	 * available, then this issue can be revisited.
	 */

	/* Is the drive greater than 16MB? */
	if (Last_Valid_Position == 0) {
		/*
		 * Determine if drive is readable at all.  If it is, the drive
		 * is too small.  If not, it could be a newly created partion,
		 * so we need to issue a different error message
		 */
		*size = 0;	/* Indicates not readable at all */
		Seek_Result = fseeko(device, Last_Valid_Position, SEEK_SET);
		if (Seek_Result == 0) {
			/* Can we read from this location? */
			Read_Result = fgetc(device);
			if (Read_Result != EOF)
				/* non-zero indicates readable, but too small */
				*size = 1;
		}
		goto restore;
	}
	/*
	 * The drive is larger than 16MB.  Now we must find out exactly how
	 * large.
	 *
	 * We now have a point within the partition and one beyond it.  The end
	 * of the partition must lie between the two.  We will use a binary
	 * search to find it.
	 */

	/* Setup for the binary search. */
	First_Invalid_Position = Current_Position;
	Current_Position = Last_Valid_Position +
			   ((Current_Position - Last_Valid_Position) / 2);

	/*
	 * Iterate until the difference between the last valid position and the
	 * first invalid position is 2 or less.
	 */
	while ((First_Invalid_Position - Last_Valid_Position) > 2) {
		/* Seek to the location we wish to test. */
		Seek_Result = fseeko(device, Current_Position, SEEK_SET);
		if (Seek_Result == 0) {
			/* Can we read from this location? */
			Read_Result = fgetc(device);
			if (Read_Result != EOF) {
				/* The current test position is valid.
				 * Save it for future reference. */
				Last_Valid_Position = Current_Position;

				/*
				 * Lets calculate the next location to test. It
				 * should be half way between the current test
				 * position and the first invalid position that
				 * we know of.
				 */
				Current_Position = Current_Position +
						   ((First_Invalid_Position -
						     Last_Valid_Position) / 2);

			}
		} else
			Read_Result = 0;

		if (Read_Result != 1) {
			/* Out test position is beyond the end of the partition.
			 * It becomes our first known invalid position. */
			First_Invalid_Position = Current_Position;

			/* Our new test position should be half way between our
			 * last known valid position and our current test
			 * position. */
			Current_Position =
			    Last_Valid_Position +
			    ((Current_Position - Last_Valid_Position) / 2);
		}
	}

	/*
	 * The size of the drive should be Last_Valid_Position + 1 as
	 * Last_Valid_Position is an offset from the beginning of the partition.
	 */
	*size = Last_Valid_Position + 1;


restore:
	/* Restore the original position. */
	if (fseeko(device, Starting_Position, SEEK_SET) != 0)
		return ERROR_SEEK;

	return NO_ERROR;
}

/*
 * NAME: ujfs_rw_diskblocks
 *
 * FUNCTION: Read/Write specific number of bytes for an opened device.
 *
 * PRE CONDITIONS:
 *
 * POST CONDITIONS:
 *
 * PARAMETERS:
 *      dev_ptr         - file handle of an opened device to read/write
 *      disk_offset     - byte offset from beginning of device for start of disk
 *                        block read/write
 *      disk_count      - number of bytes to read/write
 *      data_buffer     - On read this will be filled in with data read from
 *                        disk; on write this contains data to be written
 *      mode            - GET: read; PUT: write; VRFY: verify
 *
 * NOTES: A disk address is formed by {#cylinder, #head, #sector}
 *
 *      Also the DSK_READTRACK and DSK_WRITETRACK is a track based
 *      function. If it needs to read/write crossing track boundary,
 *      additional calls are used.
 *
 * DATA STRUCTURES:
 *
 * RETURNS:
 */
int ujfs_rw_diskblocks(FILE *dev_ptr,
		       int64_t disk_offset,
		       int32_t disk_count,
		       void *data_buffer,
		       int32_t mode)
{
	int Seek_Result;
	size_t Bytes_Transferred;
	int error = NO_ERROR;

	Seek_Result = fseeko(dev_ptr, disk_offset, SEEK_SET);
	if (Seek_Result != 0) {
		error = ERROR_SEEK;
		goto finished;
	}

	if (disk_count == 0) {
		fprintf(stderr, "ujfs_rw_diskblocks: disk_count is 0\n");
		error = ERROR_INVALID_PARAMETER;
		goto finished;
	}

	switch (mode) {
	case GET:
		Bytes_Transferred = fread(data_buffer, 1, disk_count, dev_ptr);
		break;
	case PUT:
		Bytes_Transferred = fwrite(data_buffer, 1, disk_count, dev_ptr);
		break;
	default:
		DBG_ERROR(("Internal error: %s(%d): bad mode: %d\n", __FILE__,
			   __LINE__, mode))
			error = ERROR_INVALID_HANDLE;
		    goto finished;
		break;		/* Keep the compiler happy. */
	}

	if (Bytes_Transferred != disk_count) {
		if (Bytes_Transferred == -1)
			perror("ujfs_rw_diskblocks");
		else
			fprintf(stderr,
				"ujfs_rw_diskblocks: %s %zd of %d bytes at offset %lld\n",
				(mode == GET) ? "read" : "wrote",
				Bytes_Transferred, disk_count, disk_offset);

		if (mode == GET)
			error = ERROR_READ_FAULT;
		else
			error = ERROR_WRITE_FAULT;
		goto finished;
	}

finished:
	if (error)
		rewind(dev_ptr);
	return (error);
}

int ujfs_flush_dev(FILE *fp)
{
	if (fsync(fileno(fp)) == -1)
		return errno;
#ifdef BLKFLSBUF
	return ioctl(fileno(fp), BLKFLSBUF, 0);
#else
	return (0);
#endif
}
