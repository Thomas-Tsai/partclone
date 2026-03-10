/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef __LIBFROG_FSPROPS_H__
#define __LIBFROG_FSPROPS_H__

/* Edit and view filesystem property sets. */

struct fsprops_handle {
	void	*hanp;
	size_t	hlen;
};

struct xfs_fd;
struct fs_path;

int fsprops_open_handle(struct xfs_fd *xfd, struct fs_path *fspath,
		struct fsprops_handle *fph);
void fsprops_free_handle(struct fsprops_handle *fph);

typedef int (*fsprops_name_walk_fn)(struct fsprops_handle *fph,
		const char *name, size_t valuelen, void *priv);

int fsprops_walk_names(struct fsprops_handle *fph, fsprops_name_walk_fn walk_fn,
		void *priv);
int fsprops_get(struct fsprops_handle *fph, const char *name, void *attrbuf,
		size_t *attrlen);
int fsprops_set(struct fsprops_handle *fph, const char *name, void *attrbuf,
		size_t attrlen);
int fsprops_remove(struct fsprops_handle *fph, const char *name);

#endif /* __LIBFROG_FSPROPS_H__ */
