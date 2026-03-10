/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2019 Oracle, Inc.
 * All Rights Reserved.
 */
#ifndef __LIBFROG_SCRUB_H__
#define __LIBFROG_SCRUB_H__

/* Group the scrub types by principal filesystem object. */
enum xfrog_scrub_group {
	XFROG_SCRUB_GROUP_NONE,		/* not metadata */
	XFROG_SCRUB_GROUP_AGHEADER,	/* per-AG header */
	XFROG_SCRUB_GROUP_PERAG,	/* per-AG metadata */
	XFROG_SCRUB_GROUP_FS,		/* per-FS metadata */
	XFROG_SCRUB_GROUP_INODE,	/* per-inode metadata */
	XFROG_SCRUB_GROUP_ISCAN,	/* metadata requiring full inode scan */
	XFROG_SCRUB_GROUP_SUMMARY,	/* summary metadata */
	XFROG_SCRUB_GROUP_METAPATH,	/* metadata directory path */
	XFROG_SCRUB_GROUP_RTGROUP,	/* per-rtgroup metadata */
};

/* Catalog of scrub types and names, indexed by XFS_SCRUB_TYPE_* */
struct xfrog_scrub_descr {
	const char		*name;
	const char		*descr;
	enum xfrog_scrub_group	group;
};

extern const struct xfrog_scrub_descr xfrog_scrubbers[XFS_SCRUB_TYPE_NR];
extern const struct xfrog_scrub_descr xfrog_metapaths[XFS_SCRUB_METAPATH_NR];

int xfrog_scrub_metadata(struct xfs_fd *xfd, struct xfs_scrub_metadata *meta);

/*
 * Allow enough space to call all scrub types with a barrier between each.
 * This is overkill for every caller in xfsprogs.
 */
#define XFROG_SCRUBV_MAX_VECTORS	(XFS_SCRUB_TYPE_NR * 2)

struct xfrog_scrubv {
	struct xfs_scrub_vec_head	head;
	struct xfs_scrub_vec		vectors[XFROG_SCRUBV_MAX_VECTORS];
};

/* Initialize a scrubv structure; callers must have zeroed @scrubv. */
static inline void
xfrog_scrubv_init(struct xfrog_scrubv *scrubv)
{
	scrubv->head.svh_vectors = (uintptr_t)scrubv->vectors;
}

/* Return the next free vector from the scrubv structure. */
static inline struct xfs_scrub_vec *
xfrog_scrubv_next_vector(struct xfrog_scrubv *scrubv)
{
	if (scrubv->head.svh_nr >= XFROG_SCRUBV_MAX_VECTORS)
		return NULL;

	return &scrubv->vectors[scrubv->head.svh_nr++];
}

#define foreach_xfrog_scrubv_vec(scrubv, i, vec) \
	for ((i) = 0, (vec) = (scrubv)->vectors; \
	     (i) < (scrubv)->head.svh_nr; \
	     (i)++, (vec)++)

int xfrog_scrubv_metadata(struct xfs_fd *xfd, struct xfrog_scrubv *scrubv);

#endif	/* __LIBFROG_SCRUB_H__ */
