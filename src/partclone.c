/**
 * partclone.c - Part of Partclone project.
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * more functions used by main.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <config.h>
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
#include <features.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <locale.h>
#include <mntent.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>
#include "gettext.h"
#include <linux/fs.h>
#include <sys/types.h>
#include <dirent.h>
#define _(STRING) gettext(STRING)
//#define PACKAGE "partclone"
#include "version.h"
#include "partclone.h"
#include "checksum.h"

#if defined(linux) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE      _IO(0x12,96)  /* Get device size in 512-byte blocks. */
#endif
#if defined(linux) && defined(_IOR) && !defined(BLKGETSIZE64)
#define BLKGETSIZE64    _IOR(0x12,114,size_t)   /* Get device size in bytes. */
#endif


FILE* msg = NULL;
#ifdef HAVE_LIBNCURSESW
#include <ncurses.h>
WINDOW *log_win;
WINDOW *p_win;
WINDOW *box_win;
WINDOW *bar_win;
WINDOW *tbar_win;
WINDOW *ptclscr_win;
SCREEN *ptclscr;
int log_y_line = 0;
#endif

/**
 * return the cpu architecture for which partclone is compiled
 *
 * When partclone is compiled is 32 bits, this function returns 32 even if it is run on a 64 bits OS.
 */
int get_cpu_bits()
{
#if __x86_32__ || __amd32__ || __i386__
	return 32;
#elif __x86_64__ || __amd64__
	return 64;
// Use the pointer's size if the compiler did not define one of the above.
#elif __SIZEOF_POINTER__ == 4
	return 32;
#elif __SIZEOF_POINTER__ == 8
	return 64;
#else
#pragma GCC error "Unrecognised CPU architecture. Please update this file."
#endif
}

/**
 * A version 0001 image does not contains an image_options. Its values are always the same.
 *
 * @note
 * We must use a special version of CRC32 algorithm, crc32_0001(), because the
 * old crc32() implementation contains a bug that prevented it from computing
 * the crc32 correctly.
 *
 * Also, an old bug affects some images generated from an old 64 bits version
 * of partclone. In these images, the crc32 is recorded on 8 bytes instead of 4.
 */
void set_image_options_v1(image_options* img_opt)
{
	// reset options
	memset(img_opt, 0, sizeof(image_options));

	img_opt->feature_size = sizeof(image_options_v1);
	img_opt->image_version = 0x0001;
	img_opt->checksum_mode = CSM_CRC32_0001;
	img_opt->checksum_size = CRC32_SIZE;
	img_opt->blocks_per_checksum = 1;
	img_opt->reseed_checksum = 0;
	img_opt->bitmap_mode = BM_BYTE;
}

/**
 * Set the default options for image version 0002.
 */
void set_image_options_v2(image_options* img_opt)
{
	img_opt->feature_size = sizeof(image_options_v2);
	img_opt->image_version = 0x0002;
	img_opt->checksum_mode = CSM_CRC32;
	img_opt->checksum_size = CRC32_SIZE;
	img_opt->blocks_per_checksum = 0;
	img_opt->reseed_checksum = 1;
	img_opt->bitmap_mode = BM_BIT;
}

void init_image_head_v1(image_head_v1* image_hdr, char* fs)
{
	memset(image_hdr, 0, sizeof(image_head_v1));

	strncpy(image_hdr->magic,   IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
	strncpy(image_hdr->version, IMAGE_VERSION_0001, IMAGE_VERSION_SIZE);
	strncpy(image_hdr->fs, fs, FS_MAGIC_SIZE);
}

void init_image_head_v2(image_head_v2* image_hdr) {

	memset(image_hdr, 0, sizeof(image_head_v2));

	strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
	strncpy(image_hdr->version, IMAGE_VERSION_0002, IMAGE_VERSION_SIZE);
	strncpy(image_hdr->ptc_version, VERSION, PARTCLONE_VERSION_SIZE);

	image_hdr->endianess = ENDIAN_MAGIC;
}

void init_fs_info(file_system_info* fs_info)
{
	memset(fs_info, 0, sizeof(file_system_info));
}

void init_image_options(image_options* img_opt)
{
	memset(img_opt, 0, sizeof(image_options));

	img_opt->cpu_bits = get_cpu_bits();

	set_image_options_v2(img_opt);
}

void print_readable_size_str(unsigned long long size_byte, char *new_size_str) {

	float new_size = 1.0;
	memset(new_size_str, 0, 11);
	uint64_t tbyte=1000000000000.0;
	uint64_t gbyte=1000000000;
	uint64_t mbyte=1000000;
	uint64_t kbyte=1000;

	if (size_byte == 0)
		snprintf(new_size_str, 11, "%llu", size_byte);

	if (size_byte >= tbyte) {
		new_size = (float)size_byte / (float)tbyte;
		snprintf(new_size_str, 11, "%5.1f TB", new_size);
	} else if (size_byte >= gbyte) {
		new_size = (float)size_byte / (float)gbyte;
		snprintf(new_size_str, 11, "%5.1f GB", new_size);
	} else if (size_byte >= mbyte) {
		new_size = (float)size_byte / (float)mbyte;
		snprintf(new_size_str, 11, "%5.1f MB", new_size);
	} else if (size_byte >= kbyte) {
		new_size = (float)size_byte / (float)kbyte;
		snprintf(new_size_str, 11, "%5.1f KB", new_size);
	} else {
		snprintf(new_size_str, 11, "%3i Byte", (int)size_byte);
	}
}

/**
 * options
 * usage                    - print message "how to use this"
 * print_version
 * parse_options            - get parameters from agrc, argv
 */
void usage(void) {
	fprintf(stderr, "%s v%s http://partclone.org\nUsage: %s [OPTIONS]\n"
#ifdef CHKIMG
		"    Check partclone image.\n"
#else
#ifdef RESTORE
		"    Restore partclone image to a device or standard output.\n"
#else
		"    Efficiently clone to an image, device or standard output.\n"
#endif
#endif
		"\n"
#ifndef CHKIMG
		"    -o,  --output FILE      Output FILE\n"
		"    -O   --overwrite FILE   Output FILE, overwriting if exists\n"
		"    -W   --restore_raw_file create special raw file for loop device\n"
#endif
		"    -s,  --source FILE      Source FILE\n"
		"    -L,  --logfile FILE     Log FILE\n"
#ifndef CHKIMG
#ifndef RESTORE
#ifndef DD
		"    -c,  --clone            Save to the special image format\n"
		"    -x,  --compresscmd CMD  Start CMD as an output pipe to compress the cloned image\n"
		"    -r,  --restore          Restore from the special image format\n"
		"    -b,  --dev-to-dev       Local device to device copy mode\n"
#endif
		"    -D,  --domain           Create ddrescue domain log from source device\n"
		"         --offset_domain=X  Add offset X (bytes) to domain log values\n"
		"    -R,  --rescue           Continue clone while disk read errors\n"
		"    -aX  --checksum-mode=X  Checksum formula to use to add error detection\n"
		"                            where X:\n"
		"                            0: No checksum (no slowdown, smallest image)\n"
		"                            1: CRC32 (Fast to compute, basic detection)\n"
		"    -kX  --blocks-per-checksum=X\n"
		"                            Write one checksum for every X blocks\n"
		"    -K,  --no-reseed        Do not reseed the checksum at each write (TEST)\n"
#endif
		"    -w,  --skip_write_error Continue restore while write errors\n"
#endif
		"    -dX, --debug=X          Set the debug level to X = [0|1|2]\n"
		"    -C,  --no_check         Don't check device size and free space\n"
#ifdef HAVE_LIBNCURSESW
		"    -N,  --ncurses          Using Ncurses User Interface\n"
#endif
#ifndef CHKIMG
		"    -I,  --ignore_fschk     Ignore filesystem check\n"
#endif
		"    -i,  --ignore_crc       Ignore checksum error\n"
		"    -F,  --force            Force progress\n"
		"    -f,  --UI-fresh         Fresh times of progress\n"
		"    -B,  --no_block_detail  Show progress message without block detail\n"
		"    -z,  --buffer_size SIZE Read/write buffer size (default: %d)\n"
#ifndef CHKIMG
		"    -q,  --quiet            Disable progress message\n"
		"    -E,  --offset=X         Add offset X (bytes) to OUTPUT\n"
		"    -T,  --btfiles          Restore block as file for ClonezillaBT\n"
		"    -t,  --btfiles_torrent  Restore block as file for ClonezillaBT but only generate torrent\n"
#endif
		"    -n,  --note NOTE        Display Message Note (128 words)\n"
		"    -v,  --version          Display partclone version\n"
		"    -h,  --help             Display this help\n"
		, get_exec_name(), VERSION, get_exec_name(), DEFAULT_BUFFER_SIZE);
	exit(0);
}

void print_version(void){
	printf("Partclone : v%s (%s) \n", VERSION, git_version);
	exit(0);
}

int convert_to_checksum_mode(unsigned long mode) {

	switch(mode) {

	case 0:
		return CSM_NONE;
		break;

	case 1:
		return CSM_CRC32;
		break;

	// note: we do not allow the user to use CSM_CRC32_0001. That mode exist only
	// to support image created in format 0001.

	default:
		fprintf(stderr, "Unknown checksum mode '%lu'.\n", mode);
		usage();
		return 0;
		break;
	}
}

enum {
	OPT_OFFSET_DOMAIN = 1000
};

const char *exec_name = "unset_name";

const char* get_exec_name() {

	return exec_name;
}

static void save_program_name(const char* argv0) {

	const char* last_slash = strrchr(argv0, '/');

	if (last_slash != 0) {

		exec_name = last_slash + 1;
	}
}

void parse_options(int argc, char **argv, cmd_opt* opt) {

#if CHKIMG
	static const char *sopt = "-hvd::L:s:f:CFiBz:Nn:";
#elif RESTORE
	static const char *sopt = "-hvd::L:o:O:s:f:CFINiqWBz:E:n:Tt";
#elif DD
	static const char *sopt = "-hvd::L:o:O:s:f:CFINiqWBz:E:n:Tt";
#else
	static const char *sopt = "-hvd::L:cx:brDo:O:s:f:RCFINiqWBz:E:a:k:Kn:Tt";
#endif

	static const struct option lopt[] = {
// common options
		{ "help",		no_argument,		NULL,   'h' },
		{ "print_version",	no_argument,		NULL,   'v' },
		{ "note",		required_argument,	NULL,   'n' },
		{ "source",		required_argument,	NULL,   's' },
		{ "debug",		optional_argument,	NULL,   'd' },
		{ "logfile",		required_argument,	NULL,   'L' },
		{ "UI-fresh",		required_argument,	NULL,   'f' },
		{ "no_check",		no_argument,		NULL,   'C' },
		{ "ignore_crc",		no_argument,		NULL,   'i' },
		{ "force",		no_argument,		NULL,   'F' },
		{ "no_block_detail",	no_argument,		NULL,   'B' },
		{ "buffer_size",	required_argument,	NULL,   'z' },
// not RESTORE and not CHKIMG
#ifndef CHKIMG
#ifndef RESTORE
#ifndef DD
		{ "clone",		no_argument,		NULL,   'c' },
		{ "compresscmd",	required_argument,	NULL,	'x' },
		{ "restore",		no_argument,		NULL,   'r' },
		{ "dev-to-dev",		no_argument,		NULL,   'b' },
#endif
		{ "domain",		no_argument,		NULL,   'D' },
		{ "offset_domain",	required_argument,	NULL,   OPT_OFFSET_DOMAIN },
		{ "rescue",		no_argument,		NULL,   'R' },
		{ "checksum-mode",       required_argument, NULL, 'a' },
		{ "blocks-per-checksum", required_argument, NULL, 'k' },
		{ "no-reseed",           no_argument,       NULL, 'K' },
#endif
#endif
// not CHKIMG
#ifndef CHKIMG
		{ "output",		required_argument,	NULL,   'o' },
		{ "overwrite",		required_argument,	NULL,   'O' },
		{ "restore_raw_file",	no_argument,		NULL,   'W' },
		{ "skip_write_error",	no_argument,		NULL,   'w' },
		{ "ignore_fschk",	no_argument,		NULL,   'I' },
		{ "quiet",		no_argument,		NULL,   'q' },
		{ "offset",		required_argument,	NULL,   'E' },
		{ "btfiles",		no_argument,		NULL,   'T' },
		{ "btfiles_torrent",	no_argument,		NULL,   't' },
#endif
#ifdef HAVE_LIBNCURSESW
		{ "ncurses",		no_argument,		NULL,   'N' },
#endif
		{ NULL,			0,			NULL,    0  }
	};

    save_program_name(argv[0]);

	int c;
	int mode = 0;
	memset(opt, 0, sizeof(cmd_opt));

	opt->debug = 0;
	opt->offset = 0;
	opt->rescue = 0;
	opt->skip_write_error = 0;
	opt->check = 1;
	opt->ignore_crc = 0;
	opt->quiet = 0;
	opt->no_block_detail = 0;
	opt->fresh = 2;
	opt->logfile = "/var/log/partclone.log";
	opt->buffer_size = DEFAULT_BUFFER_SIZE;
	opt->checksum_mode = CSM_CRC32;
	opt->reseed_checksum = 1;
	opt->blocks_per_checksum = 0;
	opt->blockfile = 0;


#ifdef DD
#ifndef RESTORE
#ifndef CHKIMG
    opt->ddd++;
    opt->checksum_mode = CSM_NONE;
    mode=1;
#endif
#endif
#endif
#ifdef RESTORE
	opt->restore++;
	mode=1;
#endif
#ifdef CHKIMG
	opt->restore++;
	opt->chkimg++;
	mode=1;
#endif

	while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
		switch (c) {
			case 'h':
			case '?':
				usage();
				break;
			case 'v':
				print_version();
				break;
			case 'n':
				strncpy(opt->note, optarg, NOTE_SIZE);
				break;
			case 's':
				opt->source = optarg;
				break;
			case 'd':
				opt->debug = optarg ? atol(optarg) : 1;
				break;
			case 'L':
				opt->logfile = optarg;
				break;
			case 'f':
                assert(optarg != NULL);
				opt->fresh = atol(optarg);
				break;
			case 'C':
				opt->check = 0;
				break;
			case 'i':
				opt->ignore_crc = 1;
				break;
			case 'F':
				opt->force++;
				break;
			case 'B':
				opt->no_block_detail = 1;
				break;
			case 'z':
                assert(optarg != NULL);
				opt->buffer_size = atol(optarg);
				break;
#ifndef CHKIMG
#ifndef RESTORE
#ifndef DD
			case 'c':
				opt->clone++;
				mode=1;
				break;
			case 'x':
				opt->compresscmd = optarg;
				break;
			case 'r':
				opt->restore++;
				mode=1;
				break;
			case 'b':
				opt->dd++;
				mode=1;
				break;
#endif
			case 'D':
				opt->domain++;
				mode=1;
				break;
			case OPT_OFFSET_DOMAIN:
                assert(optarg != NULL);
				opt->offset_domain = (off_t)strtoull(optarg, NULL, 0);
				break;
			case 'R':
				opt->rescue++;
				break;
			case 'a':
                assert(optarg != NULL);
				opt->checksum_mode = convert_to_checksum_mode(atol(optarg));
				break;
			case 'k':
                assert(optarg != NULL);
				opt->blocks_per_checksum = atol(optarg);
				break;
			case 'K':
				opt->reseed_checksum = 0;
				break;
#endif
#endif
#ifndef CHKIMG
			case 'O':
				opt->overwrite++;
			case 'o':
				opt->target = optarg;
				break;
			case 'W':
				opt->restore_raw_file = 1;
				break;
			case 'w':
				opt->skip_write_error = 1;
				break;
			case 'I':
				opt->ignore_fschk++;
				break;
			case 'q':
				opt->quiet = 1;
				break;
			case 'T':
				opt->blockfile = 1;
				break;
			case 't':
				opt->blockfile = 1;
				opt->torrent_only = 1;
				break;
			case 'E':
                assert(optarg != NULL);
				opt->offset = (off_t)atol(optarg);
				break;
#endif
#ifdef HAVE_LIBNCURSESW
			case 'N':
				opt->ncurses = 1;
				break;
#endif
			default:
				fprintf(stderr, "Unknown option '%s'.\n", argv[optind-1]);
				usage();
		}
	}

	if (mode != 1) {
		usage();
	}

	if (!opt->debug) {
		opt->debug = 0;
	}

	// fix conflict option for dev-to-dev
	if (opt->dd != 0 || opt->domain != 0) {
	    opt->checksum_mode = CSM_NONE;
	    opt->reseed_checksum = 1;
	    opt->blocks_per_checksum = 0;
	}


	if ((!opt->target) && (!opt->source)) {
		fprintf(stderr, "There is no image name. Use --help get more info.\n");
		exit(0);
	}

	if (opt->buffer_size < 512) {
		fprintf(stderr, "Too small or bad buffer size. Use --help get more info.\n");
		exit(0);
	}

	if (opt->offset < 0) {
		fprintf(stderr, "Too small or bad offset. Use --help get more info.\n");
		exit(0);
	}

	if (opt->offset_domain < 0) {
		fprintf(stderr, "Too small or bad offset of domain file. Use --help get more info.\n");
		exit(0);
	}

	if (!opt->target)
		opt->target = "-";

	if (!opt->source)
		opt->source = "-";

	if (opt->clone || opt->domain) {
		if ((!strcmp(opt->source, "-")) || (!opt->source)) {
			fprintf(stderr, "Partclone can't %s from stdin.\nFor help, type: %s -h\n",
				opt->clone ? "clone" : "make domain log",
				get_exec_name());
			exit(0);
		}
	}

	if (opt->checksum_mode == CSM_NONE) {

		if (opt->blocks_per_checksum > 0) {
			fprintf(stderr, "No checksum mode specified with blocks_per_checksum\n"
				"Use --help to get more info.\n");
			exit(0);
		}

		if (!opt->reseed_checksum) {
			fprintf(stderr, "No checksum mode specified with reseed_checksum\n"
				"Use --help to get more info.\n");
			exit(0);
		}

	}

#ifndef CHKIMG
	if (opt->restore) {
		if ((!strcmp(opt->target, "-")) || (!opt->target)) {
			fprintf(stderr, "Partclone can't restore to stdout.\nFor help,type: %s -h\n", get_exec_name());
			exit(0);
		}
	}
#endif
}

/**
 * Convert a number of blocks to the required space in bytes to store these blocks with their checksum.
 * It assumes block_count fill the read buffer.
 *
 * @param block_offset
 *        number of blocks already processed
 *
 * @param block_count
 *        number of blocks to be read
 *
 * @param block_size
 *        size of a block in bytes
 *
 * @param img_opt
 *        image's options
 *
 * @note
 * If the caller read a partial chunk, she have to take care of the latest checksum.
 *
 * This function handles these specials cases:
 *
 * - When blocks_per_cs is greater than the buffer's capacity, then the buffer does not
 *   always contains a checksum. When the buffer have a checksum, it is not always at the
 *   same offset, ie:
@verbatim
	# with blocks_per_cs = 4 and buffer_cap = 3
	read 1: <block><block><block>
	read 2: <block><cs><block><block>
	read 3: <block><block><cs><block>
	read 4: <block><block><block><cs>
@endverbatim
 *
 * - When the buffer's capacity does not contains full sets of blocks_per_cs, we will not
 *   always get the same number of checksum per read, ie:
@verbatim
	# with blocks_per_cs = 2 and buffer_cap = 3
	read 1: <block><block><cs><block>
	read 2: <block><cs><block><block><cs>
@endverbatim
 *
 */
unsigned long long cnv_blocks_to_bytes(unsigned long long block_offset, unsigned int block_count, unsigned int block_size, const image_options* img_opt) {

	unsigned long long bytes_count = block_count * block_size;

	if (img_opt->blocks_per_checksum) {

		/// adjust read_size to read the right number of checksum

		unsigned long total_cs  = get_checksum_count(block_offset + block_count, img_opt);
		unsigned long copied_cs = get_checksum_count(block_offset, img_opt);
		unsigned long newer_cs  = total_cs - copied_cs;

		bytes_count += newer_cs * img_opt->checksum_size;
	}

	return bytes_count;
}

/**
 * Ncurses Text User Interface
 * open_ncurses	    - open text window
 * close_ncurses    - close text window
 */
int open_ncurses() {
#ifdef HAVE_LIBNCURSESW
	int debug = 1;

	FILE *in = fopen( "/dev/stderr", "r" );
	FILE *out = fopen( "/dev/stderr", "w" );
	int terminal_x = 0;
	int terminal_y = 0;

	ptclscr = newterm(NULL, out, in);
	refresh();
	if (!ptclscr)
		log_mesg(0, 1, 1, debug, "partclone ncurses initial error\n");
	if (!set_term(ptclscr))
		log_mesg(0, 1, 1, debug, "partclone ncurses set term error\n");
	ptclscr_win = newwin(LINES, COLS, 0, 0);

	// check terminal width and height
	getmaxyx(ptclscr_win, terminal_y, terminal_x);

	// set window position
	int log_line = 12;
	int log_row = 60;
	int log_y_pos = (terminal_y-24)/2+2;
	int log_x_pos = (terminal_x-log_row)/2;
	int gap = 0;
	int p_line = 8;
	int p_row = log_row;
	int p_y_pos = log_y_pos+log_line+gap;
	int p_x_pos = log_x_pos;

	int size_ok = 1;

	if (terminal_y < (log_line+gap+p_line+3))
		size_ok = 0;
	if (terminal_x < (log_row+2))
		size_ok = 0;

	if (size_ok == 0) {
		log_mesg(0, 0, 0, debug, "Terminal width(%i) or height(%i) too small\n", terminal_x, terminal_y);
		return 0;
	}

	/// check color pair
	if (!has_colors()) {
		log_mesg(0, 0, 0, debug, "Terminal color error\n");
		return 0;
	}

	if (start_color() != OK){
		log_mesg(0, 0, 0, debug, "Terminal can't start color mode\n");
		return 0;
	}

	/// define color
	init_pair(1, COLOR_WHITE, COLOR_BLUE); ///stdscr
	init_pair(2, COLOR_RED, COLOR_WHITE); ///sub window
	init_pair(3, COLOR_BLUE, COLOR_WHITE); ///sub window

	/// write background color
	bkgd(COLOR_PAIR(1));
	wbkgd(ptclscr_win,COLOR_PAIR(2));
	touchwin(ptclscr_win);
	refresh();

	/// init main box
	attrset(COLOR_PAIR(2));
	box_win = subwin(ptclscr_win, (log_line+gap+p_line+2), log_row+2, log_y_pos-1, log_x_pos-1);
	box(box_win, ACS_VLINE, ACS_HLINE);
	wrefresh(box_win);
	wbkgd(box_win, COLOR_PAIR(2));
	mvprintw((log_y_pos-1), ((terminal_x-9)/2), " Partclone ");
	attroff(COLOR_PAIR(2));

	attrset(COLOR_PAIR(3));

	/// init log window
	log_win = subwin(ptclscr_win, log_line, log_row, log_y_pos, log_x_pos);
	wbkgd(log_win, COLOR_PAIR(3));
	touchwin(log_win);
	refresh();

	// init progress window
	p_win = subwin(ptclscr_win, p_line, p_row, p_y_pos, p_x_pos);
	wbkgd(p_win, COLOR_PAIR(3));
	touchwin(p_win);
	refresh();

	// init progress window
	bar_win = subwin(ptclscr_win, 1, p_row-10, p_y_pos+4, p_x_pos);
	wbkgd(bar_win, COLOR_PAIR(1));
	touchwin(bar_win);
	refresh();

	// init total block progress window
	tbar_win = subwin(ptclscr_win, 1, p_row-10, p_y_pos+7, p_x_pos);
	wbkgd(tbar_win, COLOR_PAIR(1));
	touchwin(tbar_win);
	refresh();

	scrollok(log_win, TRUE);

	if (touchwin(ptclscr_win) == ERR)
		return 0;

	refresh();

#endif
	return 1;
}

void close_ncurses() {
#ifdef HAVE_LIBNCURSESW
	sleep(3);
	attroff(COLOR_PAIR(3));
	delwin(log_win);
	delwin(p_win);
	delwin(bar_win);
	delwin(box_win);
	touchwin(ptclscr_win);
	endwin();
	delscreen (ptclscr);
#endif
}


/**
 * debug message
 * open_log	- to open file default is /var/log/partclone.log
 * log_mesg	- write log to the file
 *		- write log and exit 
 *		- write to stderr...
 * close_log	- to close file /var/log/partclone.log
 */
void open_log(char* source) {
	msg = fopen(source,"w");
	if (msg == NULL) {
		fprintf(stderr, "open logfile %s error\n", source);
		exit(0);
	}
}

void log_mesg(int log_level, int log_exit, int log_stderr, int debug, const char *fmt, ...) {

	va_list args;
	extern cmd_opt opt;
	char tmp_str[512];

	if (log_level > debug && (!log_exit || opt.force))
		return;

	va_start(args, fmt);
	vsnprintf(tmp_str, sizeof(tmp_str), fmt, args);
	va_end(args);

	if (opt.ncurses) {
#ifdef HAVE_LIBNCURSESW
		setlocale(LC_ALL, "");
		bindtextdomain(PACKAGE, LOCALEDIR);
		textdomain(PACKAGE);
		if ((log_stderr) && (log_level <= debug)) {
			if (log_exit)
				wattron(log_win, A_STANDOUT);

			wprintw(log_win, tmp_str);

			if (log_exit) {
				wattroff(log_win, A_STANDOUT);
				sleep(3);
			}
			wrefresh(log_win);
			log_y_line++;
		}
#endif
	} else {
		/// write log to stderr if log_stderr is true
		if ((log_stderr == 1) && (log_level <= debug))
			fprintf(stderr, "%s", tmp_str);
	}

	/// write log to logfile if debug is true
	if (log_level <= debug)
		fprintf(msg, "%s", tmp_str);

	/// clear message
	fflush(msg);

	/// exit if lexit true
	if ((!opt.force) && log_exit) {
		close_ncurses();
		fprintf(stderr, "Partclone fail, please check %s !\n", opt.logfile);
		exit(1);
	}
}

void close_log(void) {
	fclose(msg);
}

void load_image_desc_v1(file_system_info* fs_info, image_options* img_opt,
		const image_head_v1 img_hdr_v1, const file_system_info_v1 fs_info_v1, cmd_opt* opt) {

	unsigned long long dev_size = 0;

	strncpy(fs_info->fs, img_hdr_v1.fs, FS_MAGIC_SIZE);

	fs_info->block_size  = fs_info_v1.block_size;
	fs_info->device_size = fs_info_v1.device_size;
	fs_info->totalblock  = fs_info_v1.totalblock;
	fs_info->usedblocks  = fs_info_v1.usedblocks;

	dev_size = fs_info->totalblock * fs_info->block_size;
	if (fs_info->device_size != dev_size) {

		log_mesg(1, 0, 0, opt->debug, "INFO: adjusted device size reported by the image [%llu -> %llu]\n", fs_info->device_size, dev_size);
		fs_info->device_size = dev_size;
	}

	set_image_options_v1(img_opt);
}

void load_image_desc_v2(file_system_info* fs_info, image_options* img_opt,
		const image_head_v2 img_hdr_v2, const file_system_info_v2 fs_info_v2, const image_options_v2 img_opt_v2, cmd_opt* opt) {

	log_mesg(1, 0, 0, opt->debug, "Image created with Partclone v%s\n", img_hdr_v2.ptc_version);

	if (img_hdr_v2.endianess != ENDIAN_MAGIC)
		log_mesg(0, 1, 1, opt->debug, "The image have been created from an incompatible architecture\n");

	memcpy(fs_info, &fs_info_v2, sizeof(file_system_info_v2));
	memcpy(img_opt, &img_opt_v2, sizeof(image_options_v2));
}

/**
 * load the image description from the image file
 *
 * note: img_head is meaning full only when img_opt.image_version is >= 2.
 */
void load_image_desc(int* ret, cmd_opt* opt, image_head_v2* img_head, file_system_info* fs_info, image_options* img_opt) {

	image_desc_v2 buf_v2;
	int r_size;
	int debug = opt->debug;
	int img_version;

	r_size = read_all(ret, (char*)&buf_v2, sizeof(buf_v2), opt);
	if (r_size != sizeof(buf_v2))
		log_mesg(0, 1, 1, debug, "read image_hdr error=%d\n", r_size);

	/// check the image magic
	if (memcmp(buf_v2.head.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE))
		log_mesg(0, 1, 1, debug, "This is not partclone image.\n");

    assert(buf_v2.head.version != NULL);
	img_version = atol(buf_v2.head.version);

	switch(img_version) {

	case 0x0001: {
		const image_desc_v1* buf_v1 = (image_desc_v1*)&buf_v2;
		image_options_v1 extra;

		// read the extra bytes
		if (read_all(ret, extra.buff, sizeof(image_desc_v1) - sizeof(image_desc_v2), opt) == -1)
			log_mesg(0, 1, 1, debug, "read image_hdr error=%d\n (%s)", r_size, strerror(errno));

		load_image_desc_v1(fs_info, img_opt, buf_v1->head, buf_v1->fs_info, opt);
		memset(img_head, 0, sizeof(image_head_v2));
		break;
	}

	case 0x0002: {
		uint32_t crc;

		// Verify checksum
		init_crc32(&crc);
		crc = crc32(crc, &buf_v2, sizeof(buf_v2) - CRC32_SIZE);
		if (crc != buf_v2.crc)
			log_mesg(0, 1, 1, debug, "Invalid header checksum [0x%08X != 0x%08X]\n", crc, buf_v2.crc);

		load_image_desc_v2(fs_info, img_opt, buf_v2.head, buf_v2.fs_info, buf_v2.options, opt);
		memcpy(img_head, &(buf_v2.head), sizeof(image_head_v2));
		break;
	}

	default: {

		char version[IMAGE_VERSION_SIZE+1] = { 0x00 };

		memcpy(version, buf_v2.head.version, IMAGE_VERSION_SIZE);
		log_mesg(0, 1, 1, debug, "The image version is not supported [%s]\n", version);
		break;
	}

	} //switch
}

void write_image_desc(int* ret, file_system_info fs_info, image_options img_opt, cmd_opt* opt) {

	image_desc_v2 buf_v2;

	init_image_head_v2(&buf_v2.head);

	memcpy(&buf_v2.fs_info, &fs_info, sizeof(file_system_info));
	memcpy(&buf_v2.options, &img_opt, sizeof(image_options));

	init_crc32(&buf_v2.crc);
	buf_v2.crc = crc32(buf_v2.crc, &buf_v2, sizeof(image_desc_v2) - CRC32_SIZE);

	if (write_all(ret, (char*)&buf_v2, sizeof(image_desc_v2), opt) != sizeof(image_desc_v2))
		log_mesg(0, 1, 1, opt->debug, "error writing image header to image: %s\n", strerror(errno));
}

void write_image_bitmap(int* ret, file_system_info fs_info, image_options img_opt, unsigned long* bitmap, cmd_opt* opt) {

	int i, debug = opt->debug;

	switch(img_opt.bitmap_mode) {

	case BM_BIT:
	{
		if (write_all(ret, (char*)bitmap, BITS_TO_BYTES(fs_info.totalblock), opt) == -1)
			log_mesg(0, 1, 1, debug, "write bitmap to image error: %s\n", strerror(errno));

		break;
	}

	case BM_BYTE:
	{
		char bbuffer[16384];

		for (i = 0; i < fs_info.totalblock; ++i) {

			bbuffer[i % sizeof(bbuffer)] = pc_test_bit(i, bitmap, fs_info.totalblock);

			if (i % sizeof(bbuffer) == sizeof(bbuffer) - 1 || i == fs_info.totalblock - 1) {

				if (write_all(ret, bbuffer, 1 + (i % sizeof(bbuffer)), opt) == -1)
					log_mesg(0, 1, 1, debug, "write bitmap to image error: %s\n", strerror(errno));
			}
		}

		break;
	}

	case BM_NONE:
	{
		// All blocks MUST be present
		if (fs_info.usedblocks != fs_info.totalblock)
			log_mesg(0, 1, 1, debug, "ERROR: used blocks count does not match total blocks count\n");
		break;
	}

	default:
		log_mesg(0, 1, 1, debug, "ERROR: unknown bitmap mode [%d]\n", img_opt.bitmap_mode);
		break;
	}

	if (img_opt.bitmap_mode != BM_NONE) {

		switch (img_opt.image_version) {

		case 0x0001:
			if (write_all(ret, BIT_MAGIC, BIT_MAGIC_SIZE, opt) != BIT_MAGIC_SIZE)
				log_mesg(0, 1, 1, debug, "write bitmap to image error: %s\n", strerror(errno));
			break;

		case 0x0002: {

			uint32_t crc;

			init_crc32(&crc);

			crc = crc32(crc, bitmap, BITS_TO_BYTES(fs_info.totalblock));

			if (write_all(ret, (char*)&crc, sizeof(crc), opt) != sizeof(crc))
				log_mesg(0, 1, 1, debug, "write bitmap to image error: %s\n", strerror(errno));

			break;
		}

		default:
			log_mesg(0, 1, 1, debug, "image_version is not set or write_image_bitmap() must to be updated [%d]\n",
				img_opt.image_version);
			break;
		}
	}
}

const char *get_bitmap_mode_str(bitmap_mode_t bitmap_mode)
{
	switch (bitmap_mode)
	{
	case BM_NONE:
		return "NONE";

	case BM_BIT:
		return "BIT";

	case BM_BYTE:
		return "BYTE";

	default:
		return "UNKNOWN";
	}
}


/// get partition size
unsigned long long get_partition_size(int* ret) {
	unsigned long long dest_size = 0;
	unsigned long dest_block;
	struct stat stat;
	int debug = 1;

	if (!fstat(*ret, &stat)) {
		if (S_ISFIFO(stat.st_mode)) {
			dest_size = 0;
		} else if (S_ISREG(stat.st_mode)) {
			dest_size = stat.st_size;
		} else {
#ifdef BLKGETSIZE64
			if (ioctl(*ret, BLKGETSIZE64, &dest_size) < 0)
				log_mesg(0, 0, 0, debug, "get device size error, Use option -C to disable size checking(Dangerous).\n");
			log_mesg(1, 0, 0, debug, "get device size %llu by ioctl BLKGETSIZE64,\n", dest_size);
			return dest_size;
#endif
#ifdef BLKGETSIZE
			if (ioctl(*ret, BLKGETSIZE, &dest_block) >= 0)
				dest_size = (unsigned long long)(dest_block * 512);
			log_mesg(1, 0, 0, debug, "get block %lu and device size %llu by ioctl BLKGETSIZE,\n", dest_block, dest_size);
			return dest_size;
#endif
		}
	} else {
		log_mesg(0, 0, 0, debug, "fstat size error, Use option -C to disable size checking(Dangerous).\n");
	}

	return dest_size;
}

/// check partition size
int check_size(int* ret, unsigned long long size) {

	unsigned long long dest_size;
	int debug = 1;

	dest_size = get_partition_size(ret);
	if (dest_size < size){
		log_mesg(0, 1, 1, debug, "Target partition size(%llu MB) is smaller than source(%llu MB). Use option -C to disable size checking(Dangerous).\n", print_size(dest_size, MBYTE), print_size(size, MBYTE));
		return 1;
	}

	return 0;

}

/// remove DIR, copy from http://stackoverflow.com/questions/2256945/removing-a-non-empty-directory-programmatically-in-c-or-c
int remove_directory(const char *path)
{
   DIR *d = opendir(path);
   size_t path_len = strlen(path);
   int r = -1;

   if (d)
   {
      struct dirent *p;

      r = 0;

      while (!r && (p=readdir(d)))
      {
          int r2 = -1;
          char *buf;
          size_t len;

          /* Skip the names "." and ".." as we don't want to recurse on them. */
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
          {
             continue;
          }

          len = path_len + strlen(p->d_name) + 2; 
          buf = malloc(len);

          if (buf)
          {
             struct stat statbuf;

             snprintf(buf, len, "%s/%s", path, p->d_name);

             if (!stat(buf, &statbuf))
             {
                if (S_ISDIR(statbuf.st_mode))
                {
                   r2 = remove_directory(buf);
                }
                else
                {
                   r2 = unlink(buf);
                }
             }

             free(buf);
          }

          r = r2;
      }

      closedir(d);
   }

   if (!r)
   {
      r = rmdir(path);
   }

   return r;
}

/// return the number of checksum required for the number of blocks
unsigned long get_checksum_count(unsigned long long block_count, const image_options *img_opt) {

	uint32_t blocks_per_cs = img_opt->blocks_per_checksum;

	if (blocks_per_cs == 0)
		return 0;
	else
		return block_count / blocks_per_cs;
}

void update_used_blocks_count(file_system_info* fs_info, unsigned long* bitmap) {

	unsigned long long used = 0;
	unsigned int i;

	for(i = 0; i < fs_info->totalblock; ++i) {
		if (pc_test_bit(i, bitmap, fs_info->totalblock))
			++used;
	}

	fs_info->used_bitmap = used;
}


unsigned long long get_bitmap_size_on_disk(const file_system_info* fs_info, const image_options* img_opt, cmd_opt* opt)
{
	unsigned long long size = 0;

	switch(img_opt->bitmap_mode)
	{
	case BM_BIT:
		size = BITS_TO_BYTES(fs_info->totalblock);
		break;

	case BM_BYTE:
		size = fs_info->totalblock;
		break;

	case BM_NONE:
		size = 0;
		break;

	default:
		log_mesg(0, 1, 1, opt->debug, "ERROR: unknown bitmap mode [%d]\n", img_opt->bitmap_mode);
		break;
	}

	return size;
}

/// get free space
unsigned long long get_free_space(char* path){

	unsigned long long dest_size;
	struct statvfs stvfs;
	struct stat statP;
	int debug=1;

	if (statvfs(path, &stvfs) == -1) {
		printf("WARNING: Unknown free space on the destination: %s\n",
			strerror(errno));
		return 0;
	}

	/* if file is a FIFO there is no point in checking the size */
	if (!stat(path, &statP)) {
		if (S_ISFIFO(statP.st_mode))
			return 0;
	} else {
		printf("WARNING: Couldn't get file info because of the following error: %s\n",
			strerror(errno));
	}

	dest_size = (unsigned long long)stvfs.f_frsize * stvfs.f_bfree;
	if (!dest_size)
		dest_size = (unsigned long long)stvfs.f_bsize * stvfs.f_bfree;
	log_mesg(0, 0, 0, debug, "Destination have free space: %llu MB \n", print_size(dest_size, MBYTE));

	return dest_size;
}

/// check free space 
void check_free_space(char* path, unsigned long long size) {

	unsigned long long dest_size;
	struct statvfs stvfs;
	struct stat statP;
	int debug = 1;

	if (statvfs(path, &stvfs) == -1) {
		printf("WARNING: Unknown free space on the destination: %s\n",
			strerror(errno));
		return;
	}

	/* if file is a FIFO there is no point in checking the size */
	if (!stat(path, &statP)) {
		if (S_ISFIFO(statP.st_mode))
			return;
	} else {
		printf("WARNING: Couldn't get file info because of the following error: %s\n",
			strerror(errno));
	}

	dest_size = (unsigned long long)stvfs.f_frsize * stvfs.f_bfree;
	if (!dest_size)
		dest_size = (unsigned long long)stvfs.f_bsize * stvfs.f_bfree;

	if (dest_size < size)
		log_mesg(0, 1, 1, debug, "Destination doesn't have enough free space: %llu MB < %llu MB\n", print_size(dest_size, MBYTE), print_size(size, MBYTE));
}

void check_mem_size(file_system_info fs_info, image_options img_opt, cmd_opt opt) {

	const unsigned long long bitmap_size = BITS_TO_BYTES(fs_info.totalblock);

	const uint32_t blkcs = img_opt.blocks_per_checksum;
	const uint32_t block_size = fs_info.block_size;
	const unsigned int buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1; // in blocks

	const unsigned long long raw_io_size = buffer_capacity * block_size;
	unsigned long long cs_size = 0, needed_size = 0;
	void *test_bitmap, *test_read, *test_write;

	if (img_opt.checksum_mode != CSM_NONE) {

		unsigned long long cs_in_buffer = buffer_capacity / blkcs;

		cs_size = cs_in_buffer * img_opt.checksum_size;
	}

	needed_size = bitmap_size + 2 * raw_io_size + cs_size;

	log_mesg(0, 0, 0, 1, "memory needed: %llu bytes\nbitmap %llu bytes, blocks 2*%llu bytes, checksum %llu bytes\n",
		needed_size, bitmap_size, raw_io_size, cs_size);

	test_bitmap = malloc(bitmap_size);
	test_read   = malloc(raw_io_size);
	test_write  = malloc(raw_io_size + cs_size);

    free(test_bitmap);
    free(test_read);
    free(test_write);
	if (test_bitmap == NULL || test_read == NULL || test_write == NULL) {
        log_mesg(0, 1, 1, opt.debug, "There is not enough free memory, partclone suggests you should have %llu bytes memory\n", needed_size);
    }
}

void load_image_bitmap_bits(int* ret, cmd_opt opt, file_system_info fs_info, unsigned long* bitmap) {

	unsigned long long r_size, bitmap_size = BITS_TO_BYTES(fs_info.totalblock);
	uint32_t r_crc, crc;

	r_size = read_all(ret, (char*)bitmap, bitmap_size, &opt);
	if (r_size != bitmap_size)
		log_mesg(0, 1, 1, opt.debug, "unable to read bitmap.\n");

	r_size = read_all(ret, (char*)&r_crc, sizeof(r_crc), &opt);
	if (r_size != sizeof(r_crc))
		log_mesg(0, 1, 1, opt.debug, "read bitmap's crc error\n");

	init_crc32(&crc);

	crc = crc32(crc, bitmap, bitmap_size);
	if (crc != r_crc)
		log_mesg(0, 1, 1, opt.debug, "read bitmap's crc error\n");
}

void load_image_bitmap_bytes(int* ret, cmd_opt opt, file_system_info fs_info, unsigned long* bitmap) {

	unsigned long long size, r_size, r_need;
	char buffer[16384];
	unsigned long long offset = 0;
	unsigned long long bused = 0, bfree = 0;
	int i, debug = opt.debug;
	int err_exit = 1;
	char bitmagic_r[8]="00000000";/// read magic string from image

	size = fs_info.totalblock;

	while (size > 0) {
		r_need = size > sizeof(buffer) ? sizeof(buffer) : size;
		r_size = read_all(ret, buffer, r_need, &opt);
		if (r_size < r_need)
			log_mesg(0, 1, 1, debug, "Unable to read bitmap.\n");
		for (i = 0; i < r_need; i++) {
			if (buffer[i] == 1) {
				pc_set_bit(offset + i, bitmap, fs_info.totalblock);
				bused++;
			} else {
				pc_clear_bit(offset + i, bitmap, fs_info.totalblock);
				bfree++;
			}
		}
		offset += r_need;
		size -= r_need;
	}

	if (debug >= 2) {
		if (fs_info.usedblocks != bused) {
			if (opt.force)
				err_exit = 0;
			else
				err_exit = 1;
			log_mesg(0, err_exit, 1, debug, "The Used Block count is different. (bitmap %llu != file system %llu)\n"
				"Try to use --force to skip the metadata error.\n", bused, fs_info.usedblocks);
		}
	}

	/// read magic string from image file and check it.
	if (read_all(ret, bitmagic_r, BIT_MAGIC_SIZE, &opt) != BIT_MAGIC_SIZE)
		log_mesg(0, 1, 1, debug, "read magic ERROR:%s\n", strerror(errno));
	if (memcmp(bitmagic_r, BIT_MAGIC, BIT_MAGIC_SIZE))
		log_mesg(0, 1, 1, debug, "can't find bitmagic\n");
}

/// get bitmap from image file to restore data
void load_image_bitmap(int* ret, cmd_opt opt, file_system_info fs_info, image_options img_opt, unsigned long* bitmap) {

	switch(img_opt.bitmap_mode) {

	case BM_BIT:
		load_image_bitmap_bits(ret, opt, fs_info, bitmap);
		break;

	case BM_BYTE:
		load_image_bitmap_bytes(ret, opt, fs_info, bitmap);
		break;

	case BM_NONE:
		// All blocks are present
		pc_init_bitmap(bitmap, 0xFF, fs_info.totalblock);
		break;

	default:
		log_mesg(0, 1, 1, opt.debug, "ERROR: unknown bitmap mode [%d]\n", img_opt.bitmap_mode);
		break;
	}
}

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

int check_mount(const char* device, char* mount_p){

	char *real_file = NULL, *real_fsname = NULL;
	FILE * f;
	struct mntent * mnt;
	int isMounted = 0;

	real_file = malloc(PATH_MAX + 1);
	if (!real_file) {
		return -1;
	}

	real_fsname = malloc(PATH_MAX + 1);
	if (!real_fsname) {
		free(real_file); real_file = NULL;
		return -1;
	}

	if (!realpath(device, real_file)) {
		free(real_fsname); real_fsname = NULL;
        if (real_file){ free(real_file); real_file = NULL;}
		return -1;
	}

	if ((f = setmntent(MOUNTED, "r")) == 0) {
		if (real_file){ free(real_file); real_file = NULL;}
		if (real_fsname){ free(real_fsname); real_fsname = NULL;}
		return -1;
	}

	while ((mnt = getmntent (f)) != 0) {
		if (!realpath(mnt->mnt_fsname, real_fsname))
			continue;
		if (strcmp(real_file, real_fsname) == 0) {
			isMounted = 1;
			strcpy(mount_p, mnt->mnt_dir);
		}
	}
	endmntent(f);

	if (real_file){ free(real_file); real_file = NULL;}
	if (real_fsname){ free(real_fsname); real_fsname = NULL;}
	return isMounted;
}

int open_source(char* source, cmd_opt* opt) {
	int ret = 0;
	int debug = opt->debug;
	char *mp = NULL;
	int flags = O_RDONLY | O_LARGEFILE;
        struct stat st_dev;
	int ddd_block_device = -1;

	log_mesg(1, 0, 0, debug, "open source file/device %s\n", source);

	if (opt->ddd) {
	    if (stat(source, &st_dev) != -1) {
		if (S_ISBLK(st_dev.st_mode)) 
		    ddd_block_device = 1;
		else
		    ddd_block_device = 0;
	    }else{
		ddd_block_device = 0;   
	    }
	    log_mesg(1, 0, 0, debug, "ddd source file(0) or device(1) ? %i \n", ddd_block_device);
	}
	if ((opt->clone) || (opt->dd) || (opt->domain) || (ddd_block_device == 1)) { /// always is device, clone from device=source

		mp = malloc(PATH_MAX + 1);
		if (!mp)
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);

		if (check_mount(source, mp) == 1) {
			log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", source, mp);
			free(mp); mp = NULL;
			log_mesg(0, 1, 1, debug, "error exit\n");
		}
		if (mp){ free(mp); mp = NULL;}

		if ((ret = open(source, flags, S_IRUSR)) == -1)
			log_mesg(0, 1, 1, debug, "clone: open %s error\n", source);

	} else if ((opt->restore) || (ddd_block_device == 0)) {

		if (strcmp(source, "-") == 0) {
			if ((ret = fileno(stdin)) == -1)
				log_mesg(0, 1, 1, debug, "restore: open %s(stdin) error\n", source);
		} else {
			if ((ret = open (source, flags, S_IRWXU)) == -1)
				log_mesg(0, 1, 1, debug, "restore: open %s error\n", source);
		}
	}

	return ret;
}

static FILE *compress_pipe = NULL;

int open_target(char* target, cmd_opt* opt) {
	int ret = 0;
	int debug = opt->debug;
	char *mp = NULL;
	int flags = O_WRONLY | O_LARGEFILE;
	struct stat st_dev;
	int ddd_block_device = -1;

	log_mesg(1, 0, 0, debug, "open target file/device %s\n", target);
	if (opt->ddd) {
	    if (stat(target, &st_dev) != -1) {
		if (S_ISBLK(st_dev.st_mode)) 
		    ddd_block_device = 1;
		else
		    ddd_block_device = 0;
	    } else {
		ddd_block_device = 0;
	    }

	    log_mesg(1, 0, 0, debug, "ddd target file(0) or device(1) ? %i \n", ddd_block_device);
	}

	if (opt->restore_raw_file == 1) {
	    ddd_block_device = 0;
	}

	if ((opt->clone || opt->domain || (ddd_block_device == 0)) && (opt->blockfile == 0)) {
		if (opt->compresscmd) {
			int strsz = strlen(opt->compresscmd) + strlen(target) + 4;
			char *compresscmd = malloc(strsz);

			sprintf(compresscmd, "%s >%s", opt->compresscmd, target);
			compress_pipe = popen(compresscmd, "w");
			free(compresscmd);
			if ((ret = (compress_pipe ? fileno(compress_pipe) : -1)) == -1)
				log_mesg(0, 1, 1, debug, "clone: popen (%s >%s) error\n", opt->compresscmd, target);
		} else if (strcmp(target, "-") == 0) {
			if ((ret = fileno(stdout)) == -1)
				log_mesg(0, 1, 1, debug, "clone: open %s(stdout) error\n", target);
		} else {
			flags |= O_CREAT | O_TRUNC;
			if (!opt->overwrite)
				flags |= O_EXCL;
			if ((ret = open(target, flags, S_IRUSR|S_IWUSR)) == -1) {
				if (errno == EEXIST) {
					log_mesg(0, 0, 1, debug, "Output file '%s' already exists.\n"
						"Use option --overwrite if you want to replace its content.\n", target);
				}
				log_mesg(0, 0, 1, debug, "open target fail %s: %s (%i)\n", target, strerror(errno), errno);
			}
		}
	} else if (((opt->restore) || (opt->dd) || (ddd_block_device == 1)) && (opt->blockfile == 0)) {    /// always is device, restore to device=target

		/// check mounted
		mp = malloc(PATH_MAX + 1);
		if (!mp)
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		if (check_mount(target, mp)) {
			log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", target, mp);
			free(mp); mp = NULL;
			log_mesg(0, 1, 1, debug, "error exit\n");
		}
		if (mp) {
		    free(mp);
		    mp = NULL;
		}

		/// check block device
		stat(target, &st_dev);
		if (!S_ISBLK(st_dev.st_mode)) {
			log_mesg(1, 0, 1, debug, "Warning, did you restore to non-block device(%s)?\n", target);
			flags |= O_CREAT;
			if (!opt->overwrite)
				flags |= O_EXCL;
		}

		if ((ret = open (target, flags, S_IRUSR)) == -1) {
			if (errno == EEXIST) {
				log_mesg(0, 0, 1, debug, "Output file '%s' already exists.\n"
					"Use option --overwrite if you want to replace its content.\n", target);
			}
			log_mesg(0, 0, 1, debug, "%s,%s,%i: open %s error(%i)\n", __FILE__, __func__, __LINE__, target, errno);
		}
	} else if ((opt->restore || (ddd_block_device == 0)) && (opt->blockfile == 1)) {    /// always is folder

	    if ((stat(target, &st_dev) == -1) || (opt->overwrite)){
		remove_directory(target);
		mkdir(target, 0700);
		if ( opendir (target) == NULL) {
		    log_mesg(0, 0, 1, debug, "%s,%s,%i: open %s error(%i)\n", __FILE__, __func__, __LINE__, target, errno);
		}
		ret = 0;
	    } else {
		log_mesg(1, 1, 1, debug, "%s,%s,%i: directory %s exist\n", __FILE__, __func__, __LINE__, target);
	    }
	}

	return ret;
}

int close_target(int dfw) {
	int ret = 0;

	if (compress_pipe)
		ret = pclose(compress_pipe);
	else
		ret = close(dfw);
	compress_pipe = NULL;
	return ret;
}

int write_block_file(char* target, char *buf, unsigned long long count, unsigned long long offset, cmd_opt* opt){
	long long int i;
	int debug = opt->debug;
	unsigned long long size = count;
	extern unsigned long long rescue_write_size;
	int flags = O_WRONLY | O_LARGEFILE | O_CREAT ;
        int torrent_fd = 0;
	char *block_filename = malloc(PATH_MAX + 1);
	log_mesg(0, 0, 0,debug,  "offset %lld, size %lld\n", offset, count);
	sprintf(block_filename,"%s/%032llx", target, offset);
	
	if ((torrent_fd = open (block_filename, flags, S_IRUSR)) == -1) {
	    log_mesg(0, 0, 1, debug, "%s,%s,%i: open %s error(%i)\n", __FILE__, __func__, __LINE__, block_filename, errno);
	}

	// for sync I/O buffer, when use stdin or pipe.
	while (count > 0) {
	    i = write(torrent_fd, buf, count);

	    if (i < 0) {
		log_mesg(1, 0, 1, debug, "%s: errno = %i(%s)\n",__func__, errno, strerror(errno));
		if (errno != EAGAIN && errno != EINTR) {
		    return -1;
		}
	    } else if (i == 0) {
		log_mesg(1, 0, 1, debug, "%s: nothing to read. errno = %i(%s)\n",__func__, errno, strerror(errno));
		rescue_write_size = size - count;
		log_mesg(1, 0, 0, debug, "%s: rescue write size = %llu\n",__func__, rescue_write_size);
		return 0;
	    } else {
		count -= i;
		buf = i + (char *) buf;
		log_mesg(2, 0, 0, debug, "%s: %s %lli, %llu left.\n", __func__, "write block file", i, count);
	    }
	}
    close(torrent_fd);
    return size;
}

/// the io function, reference from ntfsprogs(ntfsclone).
int io_all(int *fd, char *buf, unsigned long long count, int do_write, cmd_opt* opt) {
	long long int i;
	int debug = opt->debug;
	unsigned long long size = count;
	extern unsigned long long rescue_write_size;

	// for sync I/O buffer, when use stdin or pipe.
	while (count > 0) {
		if (do_write) {
			i = write(*fd, buf, count);
                } else {
			i = read(*fd, buf, count);
                }
		if (i < 0) {
			log_mesg(1, 0, 1, debug, "%s: errno = %i(%s)\n",__func__, errno, strerror(errno));
			if (errno != EAGAIN && errno != EINTR) {
				return -1;
			}
		} else if (i == 0) {
			log_mesg(1, 0, 1, debug, "%s: nothing to read. errno = %i(%s)\n",__func__, errno, strerror(errno));
			rescue_write_size = size - count;
			log_mesg(1, 0, 0, debug, "%s: rescue write size = %llu\n",__func__, rescue_write_size);
			return 0;
		} else {
			count -= i;
			buf = i + (char *) buf;
			log_mesg(2, 0, 0, debug, "%s: %s %lli, %llu left.\n",
				__func__, do_write ? "write" : "read", i, count);
		}
	}
	return size;
}

void sync_data(int fd, cmd_opt* opt) {
	log_mesg(0, 0, 1, opt->debug, "Syncing... ");
	if (fsync(fd) && errno != EINVAL)
		log_mesg(0, 1, 1, opt->debug, "fsync error: errno = %i\n", errno);
	log_mesg(0, 0, 1, opt->debug, "OK!\n");
}

void rescue_sector(int *fd, unsigned long long pos, char *buff, cmd_opt *opt) {
	const char badsector_magic[10] = {'B', 'A', 'D', 'S', 'E', 'C', 'T', 'O', 'R', '\0'};

	if (lseek(*fd, pos, SEEK_SET) == (off_t)-1) {
		log_mesg(0, 0, 1, opt->debug, "WARNING: lseek error at %llu\n", pos);
		memset(buff, '?', PART_SECTOR_SIZE);
		memcpy(buff, badsector_magic, strlen(badsector_magic)+1);
		return;
	}

	if (io_all(fd, buff, PART_SECTOR_SIZE, 0, opt) == -1) { /// read_all
		log_mesg(0, 0, 1, opt->debug, "WARNING: Can't read sector at %llu, lost data.\n", pos);
		memset(buff, '?', PART_SECTOR_SIZE);
		memcpy(buff, badsector_magic, strlen(badsector_magic)+1);
	}
}

/// print options to log file
void print_opt(cmd_opt opt) {
	int debug = opt.debug;

	if (opt.clone)
		log_mesg(1, 0, 0, debug, "MODE: clone\n");
	else if (opt.restore)
		log_mesg(1, 0, 0, debug, "MODE: restore\n");
	else if (opt.dd)
		log_mesg(1, 0, 0, debug, "MODE: device to device\n");
	else if (opt.domain)
		log_mesg(1, 0, 0, debug, "MODE: create domain log for ddrescue\n");
	else if (opt.ddd)
		log_mesg(1, 0, 0, debug, "MODE: work like command dd\n");

	log_mesg(1, 0, 0, debug, "DEBUG: %i\n", opt.debug);
	log_mesg(1, 0, 0, debug, "SOURCE: %s\n", opt.source);
	log_mesg(1, 0, 0, debug, "TARGET: %s\n", opt.target);
	log_mesg(1, 0, 0, debug, "OVERWRITE: %i\n", opt.overwrite);
	log_mesg(1, 0, 0, debug, "RESCUE: %i\n", opt.rescue);
	log_mesg(1, 0, 0, debug, "CHECK: %i\n", opt.check);
	log_mesg(1, 0, 0, debug, "QUIET: %i\n", opt.quiet);
	log_mesg(1, 0, 0, debug, "FRESH: %i\n", opt.fresh);
	log_mesg(1, 0, 0, debug, "FORCE: %i\n", opt.force);
	log_mesg(1, 0, 0, debug, "BTFILES: %i\n", opt.blockfile);
#ifdef HAVE_LIBNCURSESW
	log_mesg(1, 0, 0, debug, "NCURSES: %i\n", opt.ncurses);
#endif
	log_mesg(1, 0, 0, debug, "OFFSET DOMAIN: 0x%llX\n", opt.offset_domain);
	log_mesg(1, 0, 0, debug, "CHECKSUM: %s\n", get_checksum_str(opt.checksum_mode));
	log_mesg(1, 0, 0, debug, "CS SIZE: %u\n", get_checksum_size(opt.checksum_mode, debug));
	log_mesg(1, 0, 0, debug, "BLOCKS/CS: %lu\n", opt.blocks_per_checksum);
	opt.note[NOTE_SIZE-1] = '\0';
	log_mesg(1, 0, 0, debug, "NOTE: %s\n", opt.note);
}

/// print partclone info
void print_partclone_info(cmd_opt opt) {

	int debug = opt.debug;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	log_mesg(0, 0, 1, debug, _("Partclone v%s http://partclone.org\n"), VERSION);
	if (opt.chkimg)
		log_mesg(0, 0, 1, debug, _("Starting to check image (%s)\n"), opt.source);	
	else if (opt.clone) {
		if (opt.compresscmd)
			log_mesg(0, 0, 1, debug, _("Starting to clone device (%s) to compressed image (%s)\n"), opt.source, opt.target);
		else
			log_mesg(0, 0, 1, debug, _("Starting to clone device (%s) to image (%s)\n"), opt.source, opt.target);
	} else if(opt.restore){
	        if (opt.blockfile)
		    log_mesg(0, 0, 1, debug, _("Starting to restore image (%s) to block files (%s)\n"), opt.source, opt.target);
		else
		    log_mesg(0, 0, 1, debug, _("Starting to restore image (%s) to device (%s)\n"), opt.source, opt.target);
	}else if(opt.dd)
		log_mesg(0, 0, 1, debug, _("Starting to back up device(%s) to device(%s)\n"), opt.source, opt.target);
	else if (opt.domain)
		log_mesg(0, 0, 1, debug, _("Starting to map device (%s) to domain log (%s)\n"), opt.source, opt.target);
	else if (opt.ddd)
	        if (opt.blockfile)
		    log_mesg(0, 0, 1, debug, _("Starting to dd image (%s) to block files (%s)\n"), opt.source, opt.target);
		else
		    log_mesg(0, 0, 1, debug, _("Starting to clone/restore (%s) to (%s) with dd mode\n"), opt.source, opt.target);
	else if (opt.info)
		log_mesg(0, 0, 1, debug, _("Showing info of image (%s)\n"), opt.source);
	else
		log_mesg(0, 0, 1, debug, "Unknown mode\n");
	if ( strlen(opt.note) > 0 ){
	    opt.note[NOTE_SIZE-1] = '\0';
	    log_mesg(0, 0, 1, debug, "note: %s\n", opt.note);
	}
}

/// print image head
void print_file_system_info(file_system_info fs_info, cmd_opt opt) {

	unsigned int     block_s = fs_info.block_size;
	unsigned long long total = fs_info.totalblock;
	unsigned long long used  = fs_info.usedblocks;
	int debug = opt.debug;
	char size_str[11];

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	log_mesg(0, 0, 1, debug, _("File system:  %s\n"), fs_info.fs);

	print_readable_size_str(total*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Device size:  %s = %llu Blocks\n"), size_str, total);

	print_readable_size_str(used*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Space in use: %s = %llu Blocks\n"), size_str, used);

	print_readable_size_str((total-used)*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Free Space:   %s = %llu Blocks\n"), size_str, (total-used));

	log_mesg(0, 0, 1, debug, _("Block size:   %i Byte\n"), block_s);
}

/// print image info
void print_image_info(image_head_v2 img_head, image_options img_opt, cmd_opt opt)
{
	int debug = opt.debug;
	char bufstr[64];

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	log_mesg(0, 0, 1, debug, _("image format:    %04d\n"), img_opt.image_version);

	if (img_opt.image_version == 0x0001)
	{
		log_mesg(0, 0, 1, debug, _("created on a:    %s\n"), "n/a");

		log_mesg(0, 0, 1, debug, _("with partclone:  %s\n"), "n/a");
	}
	else
	{
		sprintf(bufstr, _("%d bits platform"), img_opt.cpu_bits);
		log_mesg(0, 0, 1, debug, _("created on a:    %s\n"), bufstr);

		sprintf(bufstr, _("v%s"), img_head.ptc_version);
		log_mesg(0, 0, 1, debug, _("with partclone:  %s\n"), bufstr);
	}

	log_mesg(0, 0, 1, debug, _("bitmap mode:     %s\n"), get_bitmap_mode_str(img_opt.bitmap_mode));

	log_mesg(0, 0, 1, debug, _("checksum algo:   %s\n"), get_checksum_str(img_opt.checksum_mode));

	if (img_opt.checksum_mode == CSM_NONE)
	{
		log_mesg(0, 0, 1, debug, _("checksum size:   %s\n"), "n/a");

		log_mesg(0, 0, 1, debug, _("blocks/checksum: %s\n"), "n/a");

		log_mesg(0, 0, 1, debug, _("reseed checksum: %s\n"), "n/a");
	}
	else
	{
		sprintf(bufstr, "%d", img_opt.checksum_size);
		log_mesg(0, 0, 1, debug, _("checksum size:   %s\n"), bufstr);

		sprintf(bufstr, "%d", img_opt.blocks_per_checksum);
		log_mesg(0, 0, 1, debug, _("blocks/checksum: %s\n"), bufstr);

		log_mesg(0, 0, 1, debug, _("reseed checksum: %s\n"), img_opt.reseed_checksum?_("yes"):_("no"));
	}
}

/// print finish message
void print_finish_info(cmd_opt opt) {

	int debug = opt.debug;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	if (opt.chkimg)
		log_mesg(0, 0, 1, debug, _("Partclone successfully checked the image (%s)\n"), opt.source);
	else if (opt.clone)
		log_mesg(0, 0, 1, debug, _("Partclone successfully cloned the device (%s) to the image (%s)\n"), opt.source, opt.target);
	else if (opt.restore)
		log_mesg(0, 0, 1, debug, _("Partclone successfully restored the image (%s) to the device (%s)\n"), opt.source, opt.target);
	else if (opt.dd)
		log_mesg(0, 0, 1, debug, _("Partclone successfully cloned the device (%s) to the device (%s)\n"), opt.source, opt.target);
	else if (opt.domain)
		log_mesg(0, 0, 1, debug, _("Partclone successfully mapped the device (%s) to the domain log (%s)\n"), opt.source, opt.target);
}
