// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2023-2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef	__LIBFROG_GETPARENTS_H_
#define	__LIBFROG_GETPARENTS_H_

struct path_list;

struct parent_rec {
	/* File handle to parent directory */
	struct xfs_handle	p_handle;

	/* Null-terminated directory entry name in the parent */
	char			*p_name;

	/* Flags for this record; see PARENTREC_* below */
	uint32_t		p_flags;
};

/* This is the root directory. */
#define PARENTREC_FILE_IS_ROOT	(1U << 0)

typedef int (*walk_parent_fn)(const struct parent_rec *rec, void *arg);

int fd_walk_parents(int fd, size_t ioctl_bufsize, walk_parent_fn fn, void *arg);
int handle_walk_parents(const void *hanp, size_t hanlen, size_t ioctl_bufsize,
		walk_parent_fn fn, void *arg);

typedef int (*walk_path_fn)(const char *mntpt, const struct path_list *path,
		void *arg);

int fd_walk_paths(int fd, size_t ioctl_bufsize, walk_path_fn fn, void *arg);
int handle_walk_paths(const void *hanp, size_t hanlen, size_t ioctl_bufsize,
		walk_path_fn fn, void *arg);

int fd_to_path(int fd, size_t ioctl_bufsize, char *path, size_t pathlen);
int handle_to_path(const void *hanp, size_t hlen, size_t ioctl_bufsize,
		char *path, size_t pathlen);

#endif /* __LIBFROG_GETPARENTS_H_ */
