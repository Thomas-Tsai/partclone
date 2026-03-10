/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2024 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <djwong@kernel.org>
 */
#ifndef __LIBFROG_FAKELIBATTR_H__
#define __LIBFROG_FAKELIBATTR_H__

/* This file emulates APIs from the deprecated libattr. */

struct attrlist_cursor;

static inline struct xfs_attrlist_ent *
libfrog_attr_entry(
	struct xfs_attrlist	*list,
	unsigned int		index)
{
	char			*buffer = (char *)list;

	return (struct xfs_attrlist_ent *)&buffer[list->al_offset[index]];
}

static inline int
libfrog_attr_list_by_handle(
	void				*hanp,
	size_t				hlen,
	void				*buf,
	size_t				bufsize,
	int				flags,
	struct xfs_attrlist_cursor	*cursor)
{
	return attr_list_by_handle(hanp, hlen, buf, bufsize, flags,
			(struct attrlist_cursor *)cursor);
}

#endif /* __LIBFROG_FAKELIBATTR_H__ */
