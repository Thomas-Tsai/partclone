// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Oracle.  All Rights Reserved.
 * Author: Darrick J. Wong <darrick.wong@oracle.com>
 */
#ifndef __LIBFROG_PTVAR_H__
#define __LIBFROG_PTVAR_H__

struct ptvar;

typedef void (*ptvar_init_fn)(void *data);
int ptvar_alloc(size_t nr, size_t size, ptvar_init_fn init_fn,
		struct ptvar **pptv);
void ptvar_free(struct ptvar *ptv);
void *ptvar_get(struct ptvar *ptv, int *ret);

/*
 * Visit each per-thread value.  Return 0 to continue iteration or nonzero to
 * stop iterating and return to the caller.
 */
typedef int (*ptvar_iter_fn)(struct ptvar *ptv, void *data, void *foreach_arg);
int ptvar_foreach(struct ptvar *ptv, ptvar_iter_fn fn, void *foreach_arg);

#endif /* __LIBFROG_PTVAR_H__ */
