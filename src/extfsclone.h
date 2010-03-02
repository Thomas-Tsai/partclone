/**
 * extfsclone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read ext2/3 super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/// open device
static void fs_open(char* device);

/// close device
static void fs_close();

/// get block size from super block
static int block_size();

/// get device size
static unsigned long long device_size(char* device);

/// get total block from super block
static unsigned long long block_count();

/// get used blocks ( total - free ) from super block
static unsigned long long get_used_blocks();

/// readbitmap - cread and heck bitmap, reference dumpe2fs
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui);

/// get extfs type
static int test_extfs_type(char* device);

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr);
