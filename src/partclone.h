/**
 * partclone.h - Part of Partclone project.
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * function and structure used by main.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include <errno.h>

#define IMAGE_MAGIC "partclone-image"
#define IMAGE_MAGIC_SIZE 15

#define FS_MAGIC_SIZE 15
#define reiserfs_MAGIC "REISERFS"
#define reiser4_MAGIC "REISER4"
#define xfs_MAGIC "XFS"
#define extfs_MAGIC "EXTFS"
#define ext2_MAGIC "EXT2"
#define ext3_MAGIC "EXT3"
#define ext4_MAGIC "EXT4"
#define hfsplus_MAGIC "HFS Plus"
#define fat_MAGIC "FAT"
#define ntfs_MAGIC "NTFS"
#define ufs_MAGIC "UFS"
#define vmfs_MAGIC "VMFS"
#define raw_MAGIC "raw"

#define IMAGE_VERSION "0001"
#define VERSION_SIZE 4
#define SECTOR_SIZE 512
#define CRC_SIZE 4

// Reference: ntfsclone.c
#define KBYTE (1000)
#define MBYTE (1000 * 1000)
#define GBYTE (1000 * 1000 * 1000)
#define print_size(a, b) (((a) + (b - 1)) / (b))

// define read and write
#define read_all(f, b, s, o) io_all((f), (b), (s), 0, (o))
#define write_all(f, b, s, o) io_all((f), (b), (s), 1, (o))


char *EXECNAME;

/**
 * option
 * structure smd_opt
 * usage	    - print message "how to use this"
 * parse_options    - get parameter from agrc, argv
 */
struct cmd_opt
{
    int clone;
    int restore;
    int dd;
    int chkimg;
    int info;
    int debug;
    char* source;
    char* target;
    char* logfile;
    int overwrite;
    int rescue;
    int check;
    int ncurses;
    int dialog;
    int force;
    int ignore_fschk;
    int ignore_crc;
    unsigned long fresh;
};
typedef struct cmd_opt cmd_opt;
extern void usage(void);
extern void parse_options(int argc, char **argv, cmd_opt* opt);

/** 
 * Ncurses Text User Interface
 * open_ncurses		- open text window
 * close_ncurses	- close text window
 */
extern int open_ncurses();
extern void close_ncurses();
struct dialog_mesg
{
    int	    percent;
    char    data[1024];
};
typedef struct dialog_mesg p_dialog_mesg;
/**
 * debug message
 * open_log	- to open file /var/log/partclone.log
 * log_mesg	- write log to the file
 *		- write log and exit 
 *		- write to stderr...
 */
extern void open_log(char* source);
extern void log_mesg(int lerrno, int lexit, int only_debug, int debug, const char *fmt, ...);
extern void close_log();
extern int io_all(int *fd, char *buffer, int count, int do_write, cmd_opt *opt);
extern void sync_data(int fd, cmd_opt* opt);
extern void rescue_sector(int *fd, unsigned long long pos, char *buff, cmd_opt *opt);
/**
 * for restore used functions
 * restore_image_hdr	- get image_head from image file 
 * get_image_bitmap	- read bitmap data from image file
 */
struct image_head 
{
    char magic[IMAGE_MAGIC_SIZE];
    char fs[FS_MAGIC_SIZE];
    char version[VERSION_SIZE];
    int block_size;
    unsigned long long device_size;
    unsigned long long totalblock;
    unsigned long long usedblocks;
    char buff[4096];
};
typedef struct image_head image_head;

extern void restore_image_hdr(int* ret, cmd_opt* opt, image_head* image_hdr);
extern void restore_image_hdr_sp(int* ret, cmd_opt* opt, image_head* image_hdr, char* first_sec);
extern void get_image_hdr(int* ret, cmd_opt opt, image_head image_hdr, char* bitmap);

/**
 * for open and close
 * open_source	- open device or image or stdin
 * open_target	- open device or image or stdout
 *
 *	the data string 
 *	clone:	    read from device to image/stdout
 *	restore:    read from image/stdin to device
 *	dd:	    read from device to device !! not complete
 *
 */
extern int open_source(char* source, cmd_opt* opt);
extern int open_target(char* target, cmd_opt* opt);

/// check partition size
extern int check_size(int* ret, unsigned long long size);

/// check free space
extern void check_free_space(int* ret, unsigned long long size);

/// check free memory size
extern int check_mem_size(image_head image_hdr, cmd_opt opt, unsigned long long *mem_size);

/// generate crc32 code
extern unsigned long crc32(unsigned long crc, char *buf, int size);

/// print partclone info
extern void print_partclone_info(cmd_opt opt);
/// print image_head
extern void print_image_hdr_info(image_head image_hdr, cmd_opt opt);
/// print option
extern void print_opt(cmd_opt opt);
/// print finish mesg
extern void print_finish_info(cmd_opt opt);

/// initial dd hdr
extern void initial_dd_hdr(int ret, image_head* image_hdr);

/// initial bitmap
extern void dd_bitmap(image_head image_hdr, char* bitmap);
