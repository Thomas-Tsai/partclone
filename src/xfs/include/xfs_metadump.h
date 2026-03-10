// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2007 Silicon Graphics, Inc.
 * All Rights Reserved.
 */

#ifndef _XFS_METADUMP_H_
#define _XFS_METADUMP_H_

#define	XFS_MD_MAGIC_V1		0x5846534d	/* 'XFSM' */
#define	XFS_MD_MAGIC_V2		0x584D4432	/* 'XMD2' */

/* Metadump v1 */
typedef struct xfs_metablock {
	__be32		mb_magic;
	__be16		mb_count;
	uint8_t		mb_blocklog;
	uint8_t		mb_info;
	/* followed by an array of xfs_daddr_t */
} xfs_metablock_t;

/* These flags are informational only, not backwards compatible */
#define XFS_METADUMP_INFO_FLAGS	(1 << 0) /* This image has informative flags */
#define XFS_METADUMP_OBFUSCATED	(1 << 1)
#define XFS_METADUMP_FULLBLOCKS	(1 << 2)
#define XFS_METADUMP_DIRTYLOG	(1 << 3)

/*
 * Metadump v2
 *
 * The following diagram depicts the ondisk layout of the metadump v2 format.
 *
 * |------------------------------|
 * | struct xfs_metadump_header   |
 * |------------------------------|
 * | struct xfs_meta_extent 0     |
 * | Extent 0's data              |
 * | struct xfs_meta_extent 1     |
 * | Extent 1's data              |
 * | ...                          |
 * | struct xfs_meta_extent (n-1) |
 * | Extent (n-1)'s data          |
 * |------------------------------|
 *
 * The "struct xfs_metadump_header" is followed by alternating series of "struct
 * xfs_meta_extent" and the extent itself.
 */
struct xfs_metadump_header {
	__be32		xmh_magic;
	__be32		xmh_version;
	__be32		xmh_compat_flags;
	__be32		xmh_incompat_flags;
	__be64		xmh_reserved;
} __packed;

/*
 * User-supplied directory entry and extended attribute names have been
 * obscured, and extended attribute values are zeroed to protect privacy.
 */
#define XFS_MD2_COMPAT_OBFUSCATED	(1 << 0)

/* Full blocks have been dumped. */
#define XFS_MD2_COMPAT_FULLBLOCKS	(1 << 1)

/* Log was dirty. */
#define XFS_MD2_COMPAT_DIRTYLOG		(1 << 2)

/* Dump contains external log contents. */
#define XFS_MD2_COMPAT_EXTERNALLOG	(1 << 3)

/* Dump contains realtime device contents. */
#define XFS_MD2_INCOMPAT_RTDEVICE	(1U << 0)

#define XFS_MD2_INCOMPAT_ALL		(XFS_MD2_INCOMPAT_RTDEVICE)

struct xfs_meta_extent {
	/*
	 * Lowest 54 bits are used to store 512 byte addresses.
	 * Next 2 bits is used for indicating the device.
	 * 00 - Data device
	 * 01 - External log
	 * 10 - Realtime device
	 */
	__be64 xme_addr;
	/* In units of 512 byte blocks */
	__be32 xme_len;
} __packed;

#define XME_ADDR_DEVICE_SHIFT	54

#define XME_ADDR_DADDR_MASK	((1ULL << XME_ADDR_DEVICE_SHIFT) - 1)

/* Extent was copied from the data device */
#define XME_ADDR_DATA_DEVICE	(0ULL << XME_ADDR_DEVICE_SHIFT)
/* Extent was copied from the log device */
#define XME_ADDR_LOG_DEVICE	(1ULL << XME_ADDR_DEVICE_SHIFT)
/* Extent was copied from the rt device */
#define XME_ADDR_RT_DEVICE	(2ULL << XME_ADDR_DEVICE_SHIFT)

#define XME_ADDR_DEVICE_MASK	(3ULL << XME_ADDR_DEVICE_SHIFT)

#endif /* _XFS_METADUMP_H_ */
