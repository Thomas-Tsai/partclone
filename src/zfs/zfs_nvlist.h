/**
 * zfs_nvlist.h - minimal XDR nvlist parser (clean-room)
 *
 * Wire format per the XDR packing used by ZFS vdev labels and
 * DMU_OT_PACKED_NVLIST objects (big-endian, 4-byte aligned):
 *
 *   header:  u8 encoding (1=XDR), u8 host_endian, u8 reserved[2]
 *   nvlist:  s32 version, s32 nvflag, then nvpairs
 *   nvpair:  s32 encode_size, s32 decode_size (0,0 = end of list)
 *            string name (s32 len + bytes, 4-byte padded)
 *            s32 data_type, s32 nelem, then value by type
 *
 * This file is part of Partclone (GPL-2.0-or-later).
 */

#ifndef ZFS_NVLIST_H
#define ZFS_NVLIST_H

#include <stdint.h>
#include <stddef.h>

/* nvpair data types */
enum {
	ZNV_UNKNOWN	= 0,
	ZNV_BOOLEAN	= 1,	/* valueless */
	ZNV_BYTE	= 2,
	ZNV_INT16	= 3,
	ZNV_UINT16	= 4,
	ZNV_INT32	= 5,
	ZNV_UINT32	= 6,
	ZNV_INT64	= 7,
	ZNV_UINT64	= 8,
	ZNV_STRING	= 9,
	ZNV_BYTE_ARRAY	= 10,
	ZNV_INT16_ARRAY	= 11,
	ZNV_UINT16_ARRAY = 12,
	ZNV_INT32_ARRAY	= 13,
	ZNV_UINT32_ARRAY = 14,
	ZNV_INT64_ARRAY	= 15,
	ZNV_UINT64_ARRAY = 16,
	ZNV_STRING_ARRAY = 17,
	ZNV_HRTIME	= 18,
	ZNV_NVLIST	= 19,
	ZNV_NVLIST_ARRAY = 20,
	ZNV_BOOLEAN_VALUE = 21,
	ZNV_INT8	= 22,
	ZNV_UINT8	= 23,
	ZNV_BOOLEAN_ARRAY = 24,
	ZNV_INT8_ARRAY	= 25,
	ZNV_UINT8_ARRAY	= 26,
};

typedef struct znv_pair {
	char			*name;
	int			type;
	uint32_t		nelem;
	/* single-value scalars */
	uint64_t		u64;
	int64_t			s64;
	int			boolv;
	/* allocated values */
	char			*str;
	uint64_t		*u64a;
	uint8_t			*bytes;
	struct znv_list		*child;		/* ZNV_NVLIST */
	struct znv_list		**childa;	/* ZNV_NVLIST_ARRAY */
	struct znv_pair		*next;
} znv_pair_t;

typedef struct znv_list {
	znv_pair_t	*pairs;
} znv_list_t;

/* Parse a packed nvlist buffer. Returns 0 on success, -1 on error. */
int		znv_parse(const void *buf, size_t len, znv_list_t **out);
void		znv_free(znv_list_t *list);

znv_pair_t	*znv_find(const znv_list_t *list, const char *name);
int		znv_get_u64(const znv_list_t *list, const char *name,
		    uint64_t *out);
int		znv_get_string(const znv_list_t *list, const char *name,
		    const char **out);
int		znv_get_nvlist(const znv_list_t *list, const char *name,
		    znv_list_t **out);

#endif /* ZFS_NVLIST_H */
