/**
 * zfs_nvlist.c - minimal XDR nvlist parser (clean-room)
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#include <stdlib.h>
#include <string.h>

#include "zfs_disk.h"
#include "zfs_nvlist.h"

typedef struct {
	const uint8_t	*buf;
	size_t		len;
	size_t		pos;
} znv_reader_t;

static int r_bytes(znv_reader_t *r, void *dst, size_t n)
{
	if (r->pos + n > r->len)
		return (-1);
	memcpy(dst, r->buf + r->pos, n);
	r->pos += n;
	return (0);
}

static int r_u32(znv_reader_t *r, uint32_t *out)
{
	uint8_t tmp[4];
	if (r_bytes(r, tmp, 4) != 0)
		return (-1);
	*out = zbe32(tmp);
	return (0);
}

static int r_u64(znv_reader_t *r, uint64_t *out)
{
	uint8_t tmp[8];
	if (r_bytes(r, tmp, 8) != 0)
		return (-1);
	*out = zbe64(tmp);
	return (0);
}

/* XDR string: s32 length + bytes, padded to a 4-byte boundary */
static int r_string(znv_reader_t *r, char **out)
{
	uint32_t slen;
	size_t padded;
	char *s;

	if (r_u32(r, &slen) != 0)
		return (-1);
	if (slen > 64 * 1024)			/* sanity cap */
		return (-1);
	padded = (slen + 3) & ~3ULL;
	if (r->pos + padded > r->len)
		return (-1);
	s = malloc(slen + 1);
	if (s == NULL)
		return (-1);
	memcpy(s, r->buf + r->pos, slen);
	s[slen] = '\0';
	r->pos += padded;
	*out = s;
	return (0);
}

static int parse_pairs(znv_reader_t *r, znv_list_t *list, int depth);

static int parse_value(znv_reader_t *r, znv_pair_t *p, int depth)
{
	uint32_t u32;
	uint32_t i;

	switch (p->type) {
	case ZNV_BOOLEAN:
		p->u64 = 0;
		return (0);
	case ZNV_BOOLEAN_VALUE:
		if (r_u32(r, &u32) != 0)
			return (-1);
		p->boolv = (int)u32;
		return (0);
	default:
		break;
	}

	if (p->type == ZNV_UINT64 || p->type == ZNV_INT64 ||
	    p->type == ZNV_HRTIME) {
		if (r_u64(r, &p->u64) != 0)
			return (-1);
		return (0);
	}

	if (p->type == ZNV_UINT32 || p->type == ZNV_INT32 ||
	    p->type == ZNV_UINT16 || p->type == ZNV_INT16 ||
	    p->type == ZNV_UINT8 || p->type == ZNV_INT8 ||
	    p->type == ZNV_BYTE) {
		if (r_u32(r, &u32) != 0)
			return (-1);
		p->u64 = u32;
		return (0);
	}

	if (p->type == ZNV_STRING)
		return (r_string(r, &p->str));

	if (p->type == ZNV_UINT64_ARRAY || p->type == ZNV_INT64_ARRAY) {
		if (p->nelem > 1ULL << 22)	/* sanity cap */
			return (-1);
		p->u64a = calloc(p->nelem ? p->nelem : 1, sizeof (uint64_t));
		if (p->u64a == NULL)
			return (-1);
		for (i = 0; i < p->nelem; i++) {
			if (r_u64(r, &p->u64a[i]) != 0)
				goto fail;
		}
		return (0);
	}

	if (p->type == ZNV_UINT32_ARRAY || p->type == ZNV_INT32_ARRAY ||
	    p->type == ZNV_UINT16_ARRAY || p->type == ZNV_INT16_ARRAY ||
	    p->type == ZNV_UINT8_ARRAY || p->type == ZNV_INT8_ARRAY ||
	    p->type == ZNV_BYTE_ARRAY || p->type == ZNV_BOOLEAN_ARRAY) {
		/* only byte-level access needed: store raw payload */
		size_t elem = (p->type == ZNV_UINT32_ARRAY ||
		    p->type == ZNV_INT32_ARRAY || p->type == ZNV_BOOLEAN_ARRAY)
		    ? 4 : 1;
		size_t n = (size_t)p->nelem * elem;
		size_t padded = (n + 3) & ~3ULL;
		if (p->nelem > 1ULL << 22)
			return (-1);
		p->bytes = malloc(n ? n : 1);
		if (p->bytes == NULL)
			return (-1);
		if (r_bytes(r, p->bytes, n) != 0)
			goto fail;
		if (padded > n) {
			uint8_t dummy[3];
			if (r_bytes(r, dummy, padded - n) != 0)
				goto fail;
		}
		return (0);
	}

	if (p->type == ZNV_NVLIST) {
		if (p->nelem != 1)
			return (-1);
		p->child = calloc(1, sizeof (znv_list_t));
		if (p->child == NULL)
			return (-1);
		return (parse_pairs(r, p->child, depth + 1));
	}

	if (p->type == ZNV_NVLIST_ARRAY) {
		if (p->nelem > 64)
			return (-1);
		p->childa = calloc(p->nelem ? p->nelem : 1,
		    sizeof (znv_list_t *));
		if (p->childa == NULL)
			return (-1);
		for (i = 0; i < p->nelem; i++) {
			p->childa[i] = calloc(1, sizeof (znv_list_t));
			if (p->childa[i] == NULL)
				goto fail;
			if (parse_pairs(r, p->childa[i], depth + 1) != 0)
				goto fail;
		}
		return (0);
	}

	return (-1);

fail:
	return (-1);
}

static int parse_pairs(znv_reader_t *r, znv_list_t *list, int depth)
{
	uint32_t version, nvflag;
	znv_pair_t **tail = &list->pairs;

	if (depth > 8)
		return (-1);
	if (r_u32(r, &version) != 0 || r_u32(r, &nvflag) != 0)
		return (-1);

	for (;;) {
		uint32_t enc, dec, type, nelem;
		znv_pair_t *p;

		if (r_u32(r, &enc) != 0 || r_u32(r, &dec) != 0)
			return (-1);
		if (enc == 0 && dec == 0)
			return (0);		/* end of list */
		if (enc == 0 || dec == 0)
			return (-1);

		p = calloc(1, sizeof (*p));
		if (p == NULL)
			return (-1);
		if (r_string(r, &p->name) != 0)
			goto pfail;
		if (r_u32(r, &type) != 0 || r_u32(r, &nelem) != 0)
			goto pfail;
		p->type = (int)type;
		p->nelem = nelem;
		if (parse_value(r, p, depth) != 0)
			goto pfail;
		*tail = p;
		tail = &p->next;
		continue;

pfail:
		if (p->str != NULL)
			free(p->str);
		if (p->name != NULL)
			free(p->name);
		free(p->u64a);
		free(p->bytes);
		free(p);
		return (-1);
	}
}

int
znv_parse(const void *buf, size_t len, znv_list_t **out)
{
	znv_reader_t r = { .buf = buf, .len = len, .pos = 0 };
	uint8_t hdr[4];
	znv_list_t *list;

	*out = NULL;
	if (len < 12)
		return (-1);
	if (r_bytes(&r, hdr, 4) != 0)
		return (-1);
	if (hdr[0] != 1)			/* NV_ENCODE_XDR */
		return (-1);

	list = calloc(1, sizeof (*list));
	if (list == NULL)
		return (-1);
	if (parse_pairs(&r, list, 0) != 0) {
		znv_free(list);
		return (-1);
	}
	*out = list;
	return (0);
}

static void znv_pair_free(znv_pair_t *p)
{
	if (p == NULL)
		return;
	free(p->name);
	free(p->str);
	free(p->u64a);
	free(p->bytes);
	if (p->child != NULL)
		znv_free(p->child);
	if (p->childa != NULL) {
		for (uint32_t i = 0; i < p->nelem; i++)
			if (p->childa[i] != NULL)
				znv_free(p->childa[i]);
		free(p->childa);
	}
	znv_pair_free(p->next);
	free(p);
}

void
znv_free(znv_list_t *list)
{
	if (list == NULL)
		return;
	znv_pair_free(list->pairs);
	free(list);
}

znv_pair_t *
znv_find(const znv_list_t *list, const char *name)
{
	znv_pair_t *p;

	if (list == NULL)
		return (NULL);
	for (p = list->pairs; p != NULL; p = p->next)
		if (strcmp(p->name, name) == 0)
			return (p);
	return (NULL);
}

int
znv_get_u64(const znv_list_t *list, const char *name, uint64_t *out)
{
	znv_pair_t *p = znv_find(list, name);

	if (p == NULL)
		return (-1);
	switch (p->type) {
	case ZNV_UINT64:
	case ZNV_INT64:
	case ZNV_HRTIME:
	case ZNV_UINT32:
	case ZNV_INT32:
	case ZNV_UINT16:
	case ZNV_INT16:
	case ZNV_UINT8:
	case ZNV_INT8:
	case ZNV_BYTE:
		*out = p->u64;
		return (0);
	default:
		return (-1);
	}
}

int
znv_get_string(const znv_list_t *list, const char *name, const char **out)
{
	znv_pair_t *p = znv_find(list, name);

	if (p == NULL || p->type != ZNV_STRING)
		return (-1);
	*out = p->str;
	return (0);
}

int
znv_get_nvlist(const znv_list_t *list, const char *name, znv_list_t **out)
{
	znv_pair_t *p = znv_find(list, name);

	if (p == NULL || p->type != ZNV_NVLIST || p->child == NULL)
		return (-1);
	*out = p->child;
	return (0);
}
