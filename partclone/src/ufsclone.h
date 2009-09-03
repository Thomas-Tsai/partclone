/**
 * reiser4clone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read reiser4 super block and bitmap
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

///  readbitmap - read bitmap
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui);

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr);
