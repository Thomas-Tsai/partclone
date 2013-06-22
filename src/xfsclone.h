/**
 * xfsclone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read xfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <xfs/libxfs.h>

/*
 * An on-disk allocation group header is composed of 4 structures,
 * each of which is 1 disk sector long where the sector size is at
 * least 512 bytes long (BBSIZE).
 *
 * There's one ag_header per ag and the superblock in the first ag
 * is the contains the real data for the entire filesystem (although
 * most of the relevant data won't change anyway even on a growfs).
 *
 * The filesystem superblock specifies the number of AG's and
 * the AG size.  That splits the filesystem up into N pieces,
 * each of which is an AG and has an ag_header at the beginning.
 */
typedef struct ag_header  {
    xfs_dsb_t       *xfs_sb;        /* superblock for filesystem or AG */
    xfs_agf_t       *xfs_agf;       /* free space info */
    xfs_agi_t       *xfs_agi;       /* free inode info */
    xfs_agfl_t      *xfs_agfl;      /* AG freelist */
    char            *residue;
    int             residue_length;
} ag_header_t;
