// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2021-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#include "libxfs_priv.h"
#include "libxfs.h"
#include "libxfs/xfile.h"
#include <linux/memfd.h>
#include <sys/mman.h>
#ifndef HAVE_MEMFD_CREATE
#include <sys/syscall.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Swappable Temporary Memory
 * ==========================
 *
 * Offline checking sometimes needs to be able to stage a large amount of data
 * in memory.  This information might not fit in the available memory and it
 * doesn't all need to be accessible at all times.  In other words, we want an
 * indexed data buffer to store data that can be paged out.
 *
 * memfd files meet those requirements.  Therefore, the xfile mechanism uses
 * one to store our staging data.  The xfile must be freed with xfile_destroy.
 *
 * xfiles assume that the caller will handle all required concurrency
 * management; file locks are not taken.
 */

/*
 * Starting with Linux 6.3, there's a new MFD_NOEXEC_SEAL flag that disables
 * the longstanding memfd behavior that files are created with the executable
 * bit set, and seals the file against it being turned back on.
 */
#ifndef MFD_NOEXEC_SEAL
# define MFD_NOEXEC_SEAL	(0x0008U)
#endif

/*
 * The memfd_create system call was added to kernel 3.17 (2014), but
 * its corresponding glibc wrapper was only added in glibc 2.27
 * (2018).  In case a libc is not providing the wrapper, we provide
 * one here.
 */
#ifndef HAVE_MEMFD_CREATE
static int memfd_create(const char *name, unsigned int flags)
{
	return syscall(SYS_memfd_create, name, flags);
}
#endif

/*
 * Open a memory-backed fd to back an xfile.  We require close-on-exec here,
 * because these memfd files function as windowed RAM and hence should never
 * be shared with other processes.
 */
static int
xfile_create_fd(
	const char		*description)
{
	int			fd = -1;
	int			ret;

	/*
	 * memfd_create was added to kernel 3.17 (2014).  MFD_NOEXEC_SEAL
	 * causes -EINVAL on old kernels, so fall back to omitting it so that
	 * new xfs_repair can run on an older recovery cd kernel.
	 */
	fd = memfd_create(description, MFD_CLOEXEC | MFD_NOEXEC_SEAL);
	if (fd >= 0)
		goto got_fd;
	fd = memfd_create(description, MFD_CLOEXEC);
	if (fd >= 0)
		goto got_fd;

	/*
	 * O_TMPFILE exists as of kernel 3.11 (2013), which means that if we
	 * find it, we're pretty safe in assuming O_CLOEXEC exists too.
	 */
	fd = open("/dev/shm", O_TMPFILE | O_CLOEXEC | O_RDWR, 0600);
	if (fd >= 0)
		goto got_fd;

	fd = open("/tmp", O_TMPFILE | O_CLOEXEC | O_RDWR, 0600);
	if (fd >= 0)
		goto got_fd;

	/*
	 * mkostemp exists as of glibc 2.7 (2007) and O_CLOEXEC exists as of
	 * kernel 2.6.23 (2007).
	 */
	fd = mkostemp("libxfsXXXXXX", O_CLOEXEC);
	if (fd >= 0)
		goto got_fd;

	if (!errno)
		errno = EOPNOTSUPP;
	return -1;
got_fd:
	/*
	 * Turn off mode bits we don't want -- group members and others should
	 * not have access to the xfile, nor it be executable.  memfds are
	 * created with mode 0777, but we'll be careful just in case the other
	 * implementations fail to set 0600.
	 */
	ret = fchmod(fd, 0600);
	if (ret)
		perror("disabling xfile executable bit");

	return fd;
}

static LIST_HEAD(fcb_list);
static pthread_mutex_t fcb_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Create a new memfd. */
static inline int
xfile_fcb_create(
	const char		*description,
	struct xfile_fcb	**fcbp)
{
	struct xfile_fcb	*fcb;
	int			fd;

	fd = xfile_create_fd(description);
	if (fd < 0)
		return -errno;

	fcb = malloc(sizeof(struct xfile_fcb));
	if (!fcb) {
		close(fd);
		return -ENOMEM;
	}

	list_head_init(&fcb->fcb_list);
	fcb->fd = fd;
	fcb->refcount = 1;

	*fcbp = fcb;
	return 0;
}

/* Release an xfile control block */
static void
xfile_fcb_irele(
	struct xfile_fcb	*fcb,
	loff_t			pos,
	uint64_t		len)
{
	/*
	 * If this memfd is linked only to itself, it's private, so we can
	 * close it without taking any locks.
	 */
	if (list_empty(&fcb->fcb_list)) {
		close(fcb->fd);
		free(fcb);
		return;
	}

	pthread_mutex_lock(&fcb_mutex);
	if (--fcb->refcount == 0) {
		/* If we're the last user of this memfd file, kill it fast. */
		list_del(&fcb->fcb_list);
		close(fcb->fd);
		free(fcb);
	} else if (len > 0) {
		struct stat	statbuf;
		int		ret;

		/*
		 * If we were using the end of a partitioned file, free the
		 * address space.  IOWs, bonus points if you delete these in
		 * reverse-order of creation.
		 */
		ret = fstat(fcb->fd, &statbuf);
		if (!ret && statbuf.st_size == pos + len) {
			ret = ftruncate(fcb->fd, pos);
		}
	}
	pthread_mutex_unlock(&fcb_mutex);
}

/*
 * Find an memfd that can accomodate the given amount of address space.
 */
static int
xfile_fcb_find(
	const char		*description,
	uint64_t		maxbytes,
	loff_t			*posp,
	struct xfile_fcb	**fcbp)
{
	struct xfile_fcb	*fcb;
	int			ret;
	int			error = 0;

	/* No maximum range means that the caller gets a private memfd. */
	if (maxbytes == 0) {
		*posp = 0;
		return xfile_fcb_create(description, fcbp);
	}

	/* round up to page granularity so we can do mmap */
	maxbytes = roundup_64(maxbytes, PAGE_SIZE);

	pthread_mutex_lock(&fcb_mutex);

	/*
	 * If we only need a certain number of byte range, look for one with
	 * available file range.
	 */
	list_for_each_entry(fcb, &fcb_list, fcb_list) {
		struct stat	statbuf;
		loff_t		pos;

		ret = fstat(fcb->fd, &statbuf);
		if (ret)
			continue;
		pos = roundup_64(statbuf.st_size, PAGE_SIZE);

		/*
		 * Truncate up to ensure that the memfd can actually handle
		 * writes to the end of the range.
		 */
		ret = ftruncate(fcb->fd, pos + maxbytes);
		if (ret)
			continue;

		fcb->refcount++;
		*posp = pos;
		*fcbp = fcb;
		goto out_unlock;
	}

	/* Otherwise, open a new memfd and add it to our list. */
	error = xfile_fcb_create(description, &fcb);
	if (error)
		goto out_unlock;

	ret = ftruncate(fcb->fd, maxbytes);
	if (ret) {
		error = -errno;
		xfile_fcb_irele(fcb, 0, maxbytes);
		goto out_unlock;
	}

	list_add_tail(&fcb->fcb_list, &fcb_list);
	*posp = 0;
	*fcbp = fcb;

out_unlock:
	pthread_mutex_unlock(&fcb_mutex);
	return error;
}

/*
 * Create an xfile of the given size.  The description will be used in the
 * trace output.
 */
int
xfile_create(
	const char		*description,
	unsigned long long	maxbytes,
	struct xfile		**xfilep)
{
	struct xfile		*xf;
	int			error;

	xf = kmalloc(sizeof(struct xfile), 0);
	if (!xf)
		return -ENOMEM;

	error = xfile_fcb_find(description, maxbytes, &xf->partition_pos,
			&xf->fcb);
	if (error) {
		kfree(xf);
		return error;
	}

	xf->maxbytes = maxbytes;
	*xfilep = xf;
	return 0;
}

/* Close the file and release all resources. */
void
xfile_destroy(
	struct xfile		*xf)
{
	xfile_fcb_irele(xf->fcb, xf->partition_pos, xf->maxbytes);
	kfree(xf);
}

static inline loff_t
xfile_maxbytes(
	struct xfile		*xf)
{
	if (xf->maxbytes > 0)
		return xf->maxbytes;

	if (sizeof(loff_t) == 8)
		return LLONG_MAX;
	return LONG_MAX;
}

/*
 * Load an object.  Since we're treating this file as "memory", any error or
 * short IO is treated as a failure to allocate memory.
 */
ssize_t
xfile_load(
	struct xfile		*xf,
	void			*buf,
	size_t			count,
	loff_t			pos)
{
	ssize_t			ret;

	if (count > INT_MAX)
		return -ENOMEM;
	if (xfile_maxbytes(xf) - pos < count)
		return -ENOMEM;

	ret = pread(xf->fcb->fd, buf, count, pos + xf->partition_pos);
	if (ret < 0)
		return -errno;
	if (ret != count)
		return -ENOMEM;
	return 0;
}

/*
 * Store an object.  Since we're treating this file as "memory", any error or
 * short IO is treated as a failure to allocate memory.
 */
ssize_t
xfile_store(
	struct xfile		*xf,
	const void		*buf,
	size_t			count,
	loff_t			pos)
{
	ssize_t			ret;

	if (count > INT_MAX)
		return -E2BIG;
	if (xfile_maxbytes(xf) - pos < count)
		return -EFBIG;

	ret = pwrite(xf->fcb->fd, buf, count, pos + xf->partition_pos);
	if (ret < 0)
		return -errno;
	if (ret != count)
		return -ENOMEM;
	return 0;
}

/* Compute the number of bytes used by a partitioned xfile. */
static unsigned long long
xfile_partition_bytes(
	struct xfile		*xf)
{
	loff_t			data_pos = xf->partition_pos;
	loff_t			stop_pos = data_pos + xf->maxbytes;
	loff_t			hole_pos;
	unsigned long long	bytes = 0;

	data_pos = lseek(xf->fcb->fd, data_pos, SEEK_DATA);
	while (data_pos >= 0 && data_pos < stop_pos) {
		hole_pos = lseek(xf->fcb->fd, data_pos, SEEK_HOLE);
		if (hole_pos < 0) {
			/* save error, break */
			data_pos = hole_pos;
			break;
		}
		if (hole_pos >= stop_pos) {
			bytes += stop_pos - data_pos;
			return bytes;
		}
		bytes += hole_pos - data_pos;

		data_pos = lseek(xf->fcb->fd, hole_pos, SEEK_DATA);
	}
	if (data_pos < 0 && errno != ENXIO)
		return xf->maxbytes;

	return bytes;
}

/* Compute the number of bytes used by a xfile. */
unsigned long long
xfile_bytes(
	struct xfile		*xf)
{
	struct stat		statbuf;
	int			error;

	if (xf->maxbytes > 0)
		return xfile_partition_bytes(xf);

	error = fstat(xf->fcb->fd, &statbuf);
	if (error)
		return -errno;

	return (unsigned long long)statbuf.st_blocks << 9;
}

/* Discard pages backing a range of the xfile. */
void
xfile_discard(
	struct xfile		*xf,
	loff_t			pos,
	unsigned long long	count)
{
	fallocate(xf->fcb->fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			pos, count);
}
