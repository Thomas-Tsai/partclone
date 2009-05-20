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

/// update bitmap table
static void addToHist(int dwAgNo, int dwAgBlockNo, int qwLen, char* bitmap);

/// scan bno
static void scanfunc_bno(xfs_btree_sblock_t* ablock,  int level, xfs_agf_t* agf, char* bitmap);

/// scan btree
static void scan_sbtree(xfs_agf_t* agf, xfs_agblock_t root, int nlevels, char* bitmap);

/// open device
static void fs_open(char* device);

/// close device
static void fs_close();

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui);

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr);
