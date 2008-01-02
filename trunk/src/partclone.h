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

// Reference: ntfsclone.c
#define MBYTE (1000 * 1000)
#define print_size(a, b) (((a) + (b - 1)) / (b))

// define read and write
#define read_all(f, b, s) io_all((f), (b), (s), 0)
#define write_all(f, b, s) io_all((f), (b), (s), 1)


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
//	int dd;
	int debug;
        char* source;
        char* target;
};
typedef struct cmd_opt cmd_opt;
extern void usage(void);
extern void parse_options(int argc, char **argv, cmd_opt* opt);

/**
 * debug message
 * open_log	- to open file /var/log/partclone.log
 * log_mesg	- write log to the file
 *		- write log and exit 
 *		- write to stderr...
 */
extern void open_log();
extern void log_mesg(int lerrno, int lexit, int only_debug, int debug, const char *fmt, ...);
extern void close_log();
extern int io_all(int *fd, char *buffer, int count, int do_write);

/**
 * for restore used functions
 * restore_image_hdr	- get image_head from image file 
 * get_image_bitmap	- read bitmap data from image file
 */
struct image_head 
{
    char magic[IMAGE_MAGIC_SIZE];
    int block_size;
    unsigned long long device_size;
    unsigned long long totalblock;
    unsigned long long usedblocks;
};
typedef struct image_head image_head;

extern void restore_image_hdr(int* ret, cmd_opt* opt, image_head* image_hdr);
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
extern void check_size(int* ret, unsigned long long size);

/// print image_head
extern void print_image_hdr_info(image_head image_hdr, cmd_opt opt);

