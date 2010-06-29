/*
 *   Copyright (c) International Business Machines Corp., 2000-2002
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
#ifndef H_DEVICES
#define H_DEVICES

#define GET	0
#define PUT	1
#define VRFY	2

/* Macros used for determining open mode */
#define READONLY	0
#define RDWR_EXCL	1

/* Error codes */
#define NO_ERROR		0
#define ERROR_INVALID_FUNCTION	1
#define ERROR_FILE_NOT_FOUND	2
#define ERROR_INVALID_HANDLE	6
#define ERROR_NOT_ENOUGH_MEMORY 8
#define ERROR_INVALID_ACCESS	12
#define ERROR_SEEK		25
#define ERROR_WRITE_FAULT	29
#define ERROR_READ_FAULT	30
#define ERROR_GEN_FAILURE	31
#define ERROR_INVALID_PARAMETER	87
#define ERROR_DISK_FULL		112

struct stat;

int ujfs_get_dev_size(FILE *, int64_t * size);
int ujfs_rw_diskblocks(FILE *, int64_t, int32_t, void *, int32_t);
int ujfs_flush_dev(FILE *);
int ujfs_device_is_valid(FILE *, struct stat *);
#endif				/* H_DEVICES */
