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
#include "bitmap.h"

#define IMAGE_MAGIC "partclone-image"
#define IMAGE_MAGIC_SIZE 15
#define BIT_MAGIC "BiTmAgIc"
#define BIT_MAGIC_SIZE 8

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
#define exfat_MAGIC "EXFAT"
#define ntfs_MAGIC "NTFS"
#define ufs_MAGIC "UFS"
#define vmfs_MAGIC "VMFS"
#define jfs_MAGIC "JFS"
#define btrfs_MAGIC "BTRFS"
#define minix_MAGIC "MINIX"
#define f2fs_MAGIC "F2FS"
#define nilfs_MAGIC "NILFS"
#define raw_MAGIC "raw"

#define IMAGE_VERSION_SIZE 4
#define IMAGE_VERSION_0001 "0001"
#define IMAGE_VERSION_0002 "0002"
#define IMAGE_VERSION_CURRENT IMAGE_VERSION_0002
#define PARTCLONE_VERSION_SIZE (FS_MAGIC_SIZE-1)
#define DEFAULT_BUFFER_SIZE 1048576
#define PART_SECTOR_SIZE 512
#define CRC32_SIZE 4
#define NOTE_SIZE 128

// Reference: ntfsclone.c
#define KBYTE (1000)
#define MBYTE (1000 * 1000)
#define GBYTE (1000 * 1000 * 1000)
#define print_size(a, b) (((a) + (b - 1)) / (b))

// define read and write
#define read_all(f, b, s, o) io_all((f), (b), (s), 0, (o))
#define write_all(f, b, s, o) io_all((f), (b), (s), 1, (o))

// progress flag
#define BITMAP 1
#define IO 2
#define NO_BLOCK_DETAIL 3

const char* get_exec_name();

#ifdef crc32
#undef crc32
#endif

char *EXECNAME;
unsigned long long rescue_write_size;

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
    int ddd;
    int domain;
    int chkimg;
    int info;
    int debug;
    char* source;
    char* target;
    char* logfile;
    char note[NOTE_SIZE];
    int overwrite;
    int rescue;
    int check;
    int ncurses;
    int force;
    int ignore_fschk;
    int ignore_crc;
    int quiet;
    int no_block_detail;
    int restore_raw_file;
    int skip_write_error;
    int buffer_size;
    unsigned long offset;
    unsigned long fresh;
    unsigned long long offset_domain;

    int checksum_mode;
    int reseed_checksum;
    unsigned long blocks_per_checksum;
};
typedef struct cmd_opt cmd_opt;

/* Disable fields alignment for struct stored in the image */
#pragma pack(push, 1)

#define ENDIAN_MAGIC 0xC0DE

typedef struct
{
    char magic[IMAGE_MAGIC_SIZE];
    char fs[FS_MAGIC_SIZE];
    char version[IMAGE_VERSION_SIZE];
    char padding[2];

} image_head_v1;

typedef struct
{
    char magic[IMAGE_MAGIC_SIZE+1];

    /// Partclone's version who created the image, ex: "2.61"
    char ptc_version[PARTCLONE_VERSION_SIZE];

    /// Image's version
    char version[IMAGE_VERSION_SIZE];

    /// 0xC0DE = little-endian, 0xDEC0 = big-endian
    uint16_t endianess;

} image_head_v2;

typedef struct
{
    int  block_size;
    unsigned long long device_size;
    unsigned long long totalblock;
    unsigned long long usedblocks;

} file_system_info_v1;

typedef struct
{
	/// File system type
	char fs[FS_MAGIC_SIZE+1];

	/// Size of the source device, in bytes
	unsigned long long device_size;

	/// Number of blocks in the file system
	unsigned long long totalblock;

	/// Number of blocks in use as reported by the file system
	unsigned long long usedblocks;

	/// Number of blocks in use in the bitmap
	unsigned long long used_bitmap;

	/// Number of bytes in each block
	unsigned int  block_size;

} file_system_info_v2;

typedef struct
{
	char buff[4096];

} image_options_v1;

typedef enum
{
	BM_NONE = 0x00,
	BM_BIT  = 0x01,
	BM_BYTE = 0x08,

} bitmap_mode_t;

typedef struct
{
	/// Number of bytes used by this struct
	uint32_t feature_size;

	/// version of the image
	uint16_t image_version;

	/// partclone's compilation architecture: 32 bits or 64 bits
	uint16_t cpu_bits;

	/// checksum algorithm used (see checksum_mode_enum)
	uint16_t checksum_mode;

	/// Size of one checksum, in bytes. 0 when NONE, 4 with CRC32, etc.
	uint16_t checksum_size;

	/// How many consecutive blocks are checksumed together.
	uint32_t blocks_per_checksum;

	/// Reseed the checksum after each write (1 = yes; 0 = no)
	uint8_t reseed_checksum;

	/// Kind of bitmap stored in the image (see bitmap_mode_enum)
	uint8_t bitmap_mode;

} image_options_v2;

/// image format 0001 description
typedef struct
{
	image_head_v1       head;
	file_system_info_v1 fs_info;
	image_options_v1    options;

} image_desc_v1;

typedef struct
{
	image_head_v2       head;
	file_system_info_v2 fs_info;
	image_options_v2    options;
	uint32_t            crc;

} image_desc_v2;

#pragma pack(pop)

// Use these typedefs when a function handles the current version and use the
// "versioned" typedefs when a function handles a specific version.
typedef image_head_v2       image_head;
typedef file_system_info_v2 file_system_info;
typedef image_options_v2    image_options;

extern image_options img_opt;

extern void usage(void);
extern void print_version(void);
extern void parse_options(int argc, char **argv, cmd_opt* opt);

/**
 * Ncurses Text User Interface
 * open_ncurses		- open text window
 * close_ncurses	- close text window
 */
extern int open_ncurses();
extern void close_ncurses();

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
extern int io_all(int *fd, char *buffer, unsigned long long count, int do_write, cmd_opt *opt);
extern void sync_data(int fd, cmd_opt* opt);
extern void rescue_sector(int *fd, unsigned long long pos, char *buff, cmd_opt *opt);

extern unsigned long long cnv_blocks_to_bytes(unsigned long long block_offset, unsigned int block_count, unsigned int block_size, const image_options* img_opt);
extern unsigned long long get_bitmap_size_on_disk(const file_system_info* fs_info, const image_options* img_opt, cmd_opt* opt);
extern unsigned long get_checksum_count(unsigned long long block_count, const image_options *img_opt);
extern void update_used_blocks_count(file_system_info* fs_info, unsigned long* bitmap);

extern void init_fs_info(file_system_info* fs_info);
extern void init_image_options(image_options* img_opt);
extern void load_image_desc(int* ret, cmd_opt* opt, image_head_v2* img_head, file_system_info* fs_info, image_options* img_opt);
extern void load_image_bitmap(int* ret, cmd_opt opt, file_system_info fs_info, image_options img_opt, unsigned long* bitmap);
extern void write_image_desc(int* ret, file_system_info fs_info, image_options img_opt, cmd_opt* opt);
extern void write_image_bitmap(int* ret, file_system_info fs_info, image_options img_opt, unsigned long* bitmap, cmd_opt* opt);

extern const char *get_bitmap_mode_str(bitmap_mode_t bitmap_mode);

/**
 * The next two functions are not defined in partclone.c. They must be defined by each
 * file system specialisation. They are the only ones who need access to the file system's library.
 */
extern void read_super_blocks(char* device, file_system_info* fs_info);
extern void read_bitmap(char* device, file_system_info fs_info, unsigned long* bitmap, int pui);

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
extern void check_mem_size(file_system_info fs_info, image_options img_opt, cmd_opt opt);

/// print partclone info
extern void print_partclone_info(cmd_opt opt);
/// print file system info
extern void print_file_system_info(file_system_info fs_info, cmd_opt opt);
/// print image info
extern void print_image_info(image_head_v2 img_head, image_options img_opt, cmd_opt opt);
/// print option
extern void print_opt(cmd_opt opt);
/// print finish mesg
extern void print_finish_info(cmd_opt opt);

/// get partition size
extern unsigned long long get_partition_size(int* ret);
