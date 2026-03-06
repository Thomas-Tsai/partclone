// SPDX-License-Identifier: GPL-2.0


#include "libxfs_priv.h"

/*
 * Simple memory interface
 */
struct kmem_cache *
kmem_cache_create(const char *name, unsigned int size, unsigned int align,
		unsigned int slab_flags, void (*ctor)(void *))
{
	struct kmem_cache	*ptr = malloc(sizeof(struct kmem_cache));

	if (ptr == NULL) {
		fprintf(stderr, _("%s: cache init failed (%s, %d bytes): %s\n"),
			progname, name, (int)sizeof(struct kmem_cache),
			strerror(errno));
		exit(1);
	}
	ptr->cache_unitsize = size;
	ptr->cache_name = name;
	ptr->allocated = 0;
	ptr->align = align;
	ptr->ctor = ctor;

	return ptr;
}

int
kmem_cache_destroy(struct kmem_cache *cache)
{
	int	leaked = 0;

	if (getenv("LIBXFS_LEAK_CHECK") && cache->allocated) {
		leaked = 1;
		fprintf(stderr, "cache %s freed with %d items allocated\n",
				cache->cache_name, cache->allocated);
	}
	free(cache);
	return leaked;
}

void *
kmem_cache_alloc(struct kmem_cache *cache, gfp_t flags)
{
	void	*ptr = malloc(cache->cache_unitsize);

	if (ptr == NULL) {
		fprintf(stderr, _("%s: cache alloc failed (%s, %d bytes): %s\n"),
			progname, cache->cache_name, cache->cache_unitsize,
			strerror(errno));
		exit(1);
	}
	cache->allocated++;
	return ptr;
}

void *
kmem_cache_zalloc(struct kmem_cache *cache, gfp_t flags)
{
	void	*ptr = kmem_cache_alloc(cache, flags);

	memset(ptr, 0, cache->cache_unitsize);
	return ptr;
}

void *
kvmalloc(size_t size, gfp_t flags)
{
	void	*ptr;

	if (flags & __GFP_ZERO)
		ptr = calloc(1, size);
	else
		ptr = malloc(size);

	if (ptr == NULL) {
		fprintf(stderr, _("%s: malloc failed (%d bytes): %s\n"),
			progname, (int)size, strerror(errno));
		exit(1);
	}
	return ptr;
}

void *
krealloc(void *ptr, size_t new_size, int flags)
{
	/*
	 * If @new_size is zero, Linux krealloc will free the memory and return
	 * NULL, so force that behavior here.  The return value of realloc with
	 * a zero size is implementation dependent, so we cannot use that.
	 */
	if (!new_size) {
		free(ptr);
		return NULL;
	}

	ptr = realloc(ptr, new_size);
	if (ptr == NULL) {
		fprintf(stderr, _("%s: realloc failed (%d bytes): %s\n"),
			progname, (int)new_size, strerror(errno));
		exit(1);
	}
	return ptr;
}

char *kasprintf(gfp_t gfp, const char *fmt, ...)
{
	va_list ap;
	char *p;

	va_start(ap, fmt);
	if (vasprintf(&p, fmt, ap) < 0)
		p = NULL;
	va_end(ap);

	return p;
}
