/**
* The part of partclone
*
* Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
*
* The utility to print out the Image information.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*/

#include <config.h>
#include <features.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <errno.h>

#include "partclone.h"
off_t baseseek=0;
cmd_opt opt;
image_options    img_opt;
int dfr;                  /// file descriptor for source and target
unsigned long   *bitmap;  /// the point for bitmap data
file_system_info fs_info;
char *image_file;

void info_usage(void)
{
    fprintf(stderr, "partclone v%s http://partclone.org\n"
		    "Usage: partclone.fuseimg [FILE] [mount point]\n"
		    "\n"
		    , VERSION);
    exit(1);
}
unsigned long pathtoblock(const char *path)
{
    char bb[256];
    unsigned long block = 0;
    int sl = strlen(path);
    strncpy(bb, path+1, sl);
    bb[sl]='\0';
    //block = atoi(bb);
    block = strtoul(bb, NULL, 16);
    block = block / fs_info.block_size;

    //printf("path = %s bb = %s block = %i\n", path, bb, block);
    return block;

}

size_t get_file_size(unsigned long block)
{
    unsigned long copied = 0;
    unsigned long block_id =0;
    unsigned long nx_current = 0;
    for (block_id = block; block_id <= fs_info.totalblock; block_id++) {
	if (block_id < fs_info.totalblock) {
	    nx_current = pc_test_bit(block_id, bitmap, fs_info.totalblock);
	    if (nx_current)
		copied++;
	} else
	    nx_current = -1;
	if (!nx_current) {
	    return (size_t)(copied*fs_info.block_size);
	}
    }
    return 0;
}


size_t read_block_data(unsigned long block, char *buf, size_t size, off_t offset)
{
    unsigned long long used = 0;
    unsigned int i;
    unsigned long int seek_crc_size = 0;
    off_t bseek = 0;
    size_t x = 0;

    for(i = 0; i < block; ++i) {
	if (pc_test_bit(i, bitmap, fs_info.totalblock))
	    ++used;
    }

    seek_crc_size = (used / img_opt.blocks_per_checksum) * img_opt.checksum_size;
    bseek = (off_t)(fs_info.block_size*used+seek_crc_size+baseseek);

    //printf("RRRRR read block %lu\n", block);
    //printf("bseek, == %zd, ", bseek);
    //printf("blockXsiz == %zd, ", (fs_info.block_size*used));
    //printf("crcXsize == %zd, ", seek_crc_size);
    //printf("base == %zd, ", baseseek);
    //printf("\n");

    offset+=bseek;
    //printf("pread buf %p, size %zu, offset %zd\n", buf, size, offset);
    x  = pread(dfr, buf, size, offset);
    return x;

}
size_t read_blocks_data(unsigned long block, char *buf, size_t size, off_t offset)
{
    size_t readed_size = 0;
    size_t total_readed_size = 0;
    unsigned long block_count = 0;
    size_t last_size = 0;
    unsigned long current_block = 0;
    unsigned long skip_block = 0;
    unsigned long x_block = 0;
    size_t skip_size = 0;

    current_block = block;
    skip_block = offset/fs_info.block_size;
    skip_size = offset%fs_info.block_size;

    // first block
    if ((skip_size > 0) || (skip_block >= 1)){
	current_block += skip_block;
	//printf("read first block %lu\n", current_block);
	if (size <= fs_info.block_size){
	    readed_size = read_block_data(current_block, buf, size, skip_size);
	}else{
	    readed_size = read_block_data(current_block, buf, fs_info.block_size-skip_size, skip_size);
	}
	current_block++;
	total_readed_size+=readed_size;
    }

    if (total_readed_size == size)
	return total_readed_size;

    block_count = (size-total_readed_size) / fs_info.block_size;

    // continue read each block
    //printf("block %lu blockcount %lu\n", block, block_count);
    for (x_block = 0; x_block < block_count ; x_block++){
	//printf("read block %lu\n", current_block);
	readed_size = read_block_data(current_block, buf+total_readed_size, fs_info.block_size, 0);
	total_readed_size+=readed_size;
	current_block++;
    }

    if (total_readed_size == size)
	return total_readed_size;

    last_size = size - total_readed_size;
    // last size in last block
    if (last_size > 0){
	//printf("read last block %lu\n", current_block);
	readed_size = read_block_data(current_block, buf+total_readed_size, last_size, 0);
	current_block++;
	total_readed_size+=readed_size;
    }
    
    return total_readed_size;

}

void info_options ()
{
    memset(&opt, 0, sizeof(cmd_opt));
    opt.debug = 0;
    opt.quiet = 0;
    opt.info  = 1;
    opt.logfile = "/var/log/partclone.log";
    opt.target  = 0;
    opt.clone   = 0;
    opt.restore = 0;
    opt.info = 1;
    opt.source = image_file;
}
static void *main_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{

    (void) conn;
    //cfg->kernel_cache = 1;
    return NULL;
}

static int getattr_block(const char *path, struct stat *stbuf)
{

    unsigned long block = 0;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
	stbuf->st_mode = S_IFDIR | 0755;
	stbuf->st_nlink = 2;
	return 0;
    }

    stbuf->st_mode = S_IFREG | 0644;
    stbuf->st_nlink = 1;
    block = pathtoblock(path);
    stbuf->st_size = get_file_size(block);
    return 0;

    //return -ENOENT;
}

static int readdir_block(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{

    (void) offset;
    (void) fi;
    char buffer[33];
    int n = 0;
    unsigned long long test_block = 0;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (test_block = 0; test_block < fs_info.totalblock; ++test_block){
	if (pc_test_bit(test_block, bitmap, fs_info.totalblock)){
	    n = sprintf (buffer, "%032llx", test_block*fs_info.block_size);
	    if (n >0){ 
		filler(buf, buffer, NULL, 0);
		test_block = test_block + (get_file_size(test_block)/fs_info.block_size);
	    }
	}
    }

    return 0;
}

static int open_block(const char *path, struct fuse_file_info *fi)
{
    //image_head_v2    img_head;
    //memset(&opt, 0, sizeof(cmd_opt));
    //info_options();
    ////open_log(opt.logfile);

    //dfr = open(opt.source, O_RDONLY);
    //load_image_desc(&dfr, &opt, &img_head, &fs_info, &img_opt);
    //bitmap = pc_alloc_bitmap(fs_info.totalblock);
    //load_image_bitmap(&dfr, opt, fs_info, img_opt, bitmap);
    //baseseek = lseek(dfr, 0, SEEK_CUR);
    fi->fh = dfr;
    return 0;
}

static int read_block(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{

    unsigned long block = 0;
    size_t r_size = 0;
    size_t len = 0;

    block = pathtoblock(path);
    len = get_file_size(block);

    if (path) {
	if (offset >= len) {
	    printf("offset bigger than len\n");
	    return 0;
	}

	if (offset + size > len) {
	    //printf("ReadIng Offset(%zd) size(%zu) > len(%zu)\n", offset, size, len);
	    r_size = read_blocks_data(block, buf, len-offset, offset);
	    return len - offset;
	}

	//printf("Read Offset(%zd) size(%zu) len(%zu)\n", offset, size, len);
	r_size = read_blocks_data(block, buf, size, offset);
	//printf("Read Size %zu\n", r_size);
	return r_size;

    }

    return -ENOENT;
}

static struct fuse_operations ptl_fuse_operations =
{
    .init = main_init,
    .getattr = getattr_block,
    .open = open_block,
    .read = read_block,
    .readdir = readdir_block,
};

int main(int argc, char *argv[])
{
    image_file = realpath(argv[argc-2], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    image_head_v2    img_head;

    memset(&opt, 0, sizeof(cmd_opt));
    info_options();
    open_log(opt.logfile);

    /**
    * open Image file
    */
    dfr = open(opt.source, O_RDONLY);
//    if (dfr == -1){
//	log_mesg(0, 1, 1, opt.debug, "fuseinfo: Can't open file(%s)\n", opt.source);
//    }

    /// get image information from image file
    load_image_desc(&dfr, &opt, &img_head, &fs_info, &img_opt);

    /// alloc a memory to restore bitmap
    bitmap = pc_alloc_bitmap(fs_info.totalblock);
//    if (bitmap == NULL){
//	log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);
//    }
//    log_mesg(0, 0, 0, opt.debug, "initial main bitmap pointer %p\n", bitmap);
//    log_mesg(0, 0, 0, opt.debug, "Initial image hdr: read bitmap table\n");

    /// read and check bitmap from image file
    load_image_bitmap(&dfr, opt, fs_info, img_opt, bitmap);

//    log_mesg(0, 0, 0, opt.debug, "check main bitmap pointer %p\n", bitmap);
//    log_mesg(0, 0, 0, opt.debug, "print image information\n");
//    log_mesg(0, 0, 1, opt.debug, "\n");

//    print_partclone_info(opt);
//    print_file_system_info(fs_info, opt);
    baseseek = lseek(dfr, 0, SEEK_CUR);

    return fuse_main(argc, argv, &ptl_fuse_operations, NULL);
}
