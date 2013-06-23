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
#include "gettext.h"
#include <linux/fs.h>
#define _(STRING) gettext(STRING)
//#define PACKAGE "partclone"
#include "version.h"
#include "partclone.h"

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
		"    -c,  --clone            Save to the special image format\n"
		"    -r,  --restore          Restore from the special image format\n"
		"    -b,  --dev-to-dev       Local device to device copy mode\n"
		"    -D,  --domain           Create ddrescue domain log from source device\n"
		"         --offset_domain=X  Add offset X (bytes) to domain log values\n"
		"    -R,  --rescue           Continue clone while disk read errors\n"
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
		"    -i,  --ignore_crc       Ignore crc check error\n"
		"    -F,  --force            Force progress\n"
		"    -f,  --UI-fresh         Fresh times of progress\n"
		"    -B,  --no_block_detail  Show progress message without block detail\n"
		"    -z,  --buffer_size SIZE Read/write buffer size (default: %d)\n"
#ifndef CHKIMG
		"    -q,  --quiet            Disable progress message\n"
		"    -E,  --offset=X         Add offset X (bytes) to OUTPUT\n"
#endif
		"    -v,  --version          Display partclone version\n"
		"    -h,  --help             Display this help\n"
		, EXECNAME, VERSION, EXECNAME, DEFAULT_BUFFER_SIZE);
	exit(0);
}

void print_version(void){
	printf("Partclone : v%s (%s) \n", VERSION, git_version);
	exit(0);
}

enum {
	OPT_OFFSET_DOMAIN = 1000
};

void parse_options(int argc, char **argv, cmd_opt* opt) {
#ifdef CHKIMG
	static const char *sopt = "-hvd::L:s:f:CFiBz:N";
#else
#ifdef RESTORE
	static const char *sopt = "-hvd::L:o:O:s:f:CFINiqWBz:E:";
#else
	static const char *sopt = "-hvd::L:cbrDo:O:s:f:RCFINiqWBz:E:";
#endif
#endif
	static const struct option lopt[] = {
// common options
		{ "help",		no_argument,		NULL,   'h' },
		{ "print_version",	no_argument,		NULL,   'v' },
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
		{ "clone",		no_argument,		NULL,   'c' },
		{ "restore",		no_argument,		NULL,   'r' },
		{ "dev-to-dev",		no_argument,		NULL,   'b' },
		{ "domain",		no_argument,		NULL,   'D' },
		{ "offset_domain",	required_argument,	NULL,   OPT_OFFSET_DOMAIN },
		{ "rescue",		no_argument,		NULL,   'R' },
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
#endif
#ifdef HAVE_LIBNCURSESW
		{ "ncurses",		no_argument,		NULL,   'N' },
#endif
		{ NULL,			0,			NULL,    0  }
	};

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

#ifdef RESTORE
	opt->restore++;
	mode++;
#endif
#ifdef CHKIMG
	opt->restore++;
	opt->chkimg++;
	mode++;
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
			case 's':
				opt->source = optarg;
				break;
			case 'd':
				if (optarg)
					opt->debug = atol(optarg);
				else
					opt->debug = 1;
				break;
			case 'L':
				opt->logfile = optarg;
				break;
			case 'f':
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
				opt->buffer_size = atol(optarg);
				break;
#ifndef CHKIMG
#ifndef RESTORE
			case 'c':
				opt->clone++;
				mode++;
				break;
			case 'r':
				opt->restore++;
				mode++;
				break;
			case 'b':
				opt->dd++;
				mode++;
				break;
			case 'D':
				opt->domain++;
				mode++;
				break;
			case OPT_OFFSET_DOMAIN:
				opt->offset_domain = strtoull(optarg, NULL, 0);
				break;
			case 'R':
				opt->rescue++;
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
			case 'E':
				opt->offset = atol(optarg);
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

	if ((!opt->target) && (!opt->source)) {
		fprintf(stderr, "There is no image name. Use --help get more info.\n");
		exit(0);
	}

	if (opt->buffer_size < 512) {
		fprintf(stderr, "Too small or bad buffer size. Use --help get more info.\n");
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
				EXECNAME);
			exit(0);
		}
	}

#ifndef CHKIMG
	if (opt->restore) {
		if ((!strcmp(opt->target, "-")) || (!opt->target)) {
			fprintf(stderr, "Partclone can't restore to stdout.\nFor help,type: %s -h\n", EXECNAME);
			exit(0);
		}
	}
#endif
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

/// get image_head from image file
void restore_image_hdr(int* ret, cmd_opt* opt, image_head* image_hdr) {
	int r_size;
	char* buffer;
	unsigned long long dev_size;
	int debug = opt->debug;

	buffer = (char*)malloc(sizeof(image_head));
	if (buffer == NULL)
		log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);

	memset(buffer, 0, sizeof(image_head));
	r_size = read_all(ret, buffer, sizeof(image_head), opt);
	if (r_size == -1)
		log_mesg(0, 1, 1, debug, "read image_hdr error\n");

	memcpy(image_hdr, buffer, sizeof(image_head));
	free(buffer);

	dev_size = (unsigned long long)(image_hdr->totalblock * image_hdr->block_size);
	if (image_hdr->device_size != dev_size)
		image_hdr->device_size = dev_size;

	/// check the image magic
	if (memcmp(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE))
		log_mesg(0, 1, 1, debug, "This is not partclone image.\n");

	/// check the image version
	if (memcmp(image_hdr->version, IMAGE_VERSION_CURRENT, IMAGE_VERSION_SIZE)) {
		char version[IMAGE_VERSION_SIZE+1] = { '\x00' };
		memcpy(version, image_hdr->version, IMAGE_VERSION_SIZE);
		log_mesg(0, 1, 1, debug, "The image version is not supported [%s]\n", version);
	}
}

void write_image_head(int* ret, image_head image_hdr, cmd_opt* opt) {

	if (write_all(ret, (char *)&image_hdr, sizeof(image_head), opt) == -1)
		log_mesg(0, 1, 1, opt->debug, "write image_head to image error: %s\n", strerror(errno));
}

void write_image_bitmap(int* ret, image_head image_hdr, cmd_opt* opt, unsigned long* bitmap) {

	int i;
	char bbuffer[16384];

	/// write bitmap information to image file
	for (i = 0; i < image_hdr.totalblock; i++) {
		bbuffer[i % sizeof(bbuffer)] = pc_test_bit(i, bitmap);

		if (i % sizeof(bbuffer) == sizeof(bbuffer) - 1 || i == image_hdr.totalblock - 1) {
			if (write_all(ret, bbuffer, 1 + (i % sizeof(bbuffer)), opt) == -1)
				log_mesg(0, 1, 1, opt->debug, "write bitmap to image error: %s\n", strerror(errno));
		}
	}

	/// write a magic string
	if (write_all(ret, BIT_MAGIC, BIT_MAGIC_SIZE, opt) != BIT_MAGIC_SIZE)
		log_mesg(0, 1, 1, opt->debug, "write bitmap to image error: %s\n", strerror(errno));
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

/// check free space 
void check_free_space(int* ret, unsigned long long size) {

	unsigned long long dest_size;
	struct statvfs stvfs;
	struct stat stat;
	int debug = 1;

	if (fstatvfs(*ret, &stvfs) == -1) {
		printf("WARNING: Unknown free space on the destination: %s\n",
			strerror(errno));
		return;
	}

	/* if file is a FIFO there is no point in checking the size */
	if (!fstat(*ret, &stat)) {
		if (S_ISFIFO(stat.st_mode))
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

/// check free memory size
int check_mem_size(image_head image_hdr, cmd_opt opt, unsigned long long *mem_size) {
	unsigned long long image_head_size = 0;
	unsigned long long bitmap_size = 0;
	int crc_io_size = 0;
	void *test_mem;

	image_head_size = sizeof(image_head);
	bitmap_size = PART_BYTES_PER_LONG * BITS_TO_LONGS(image_hdr.totalblock);
	crc_io_size = CRC_SIZE+image_hdr.block_size;
	*mem_size = image_head_size + bitmap_size + crc_io_size;
	log_mesg(0, 0, 0, 1, "we need memory: %llu bytes\nimage head %llu, bitmap %llu, crc %i bytes\n", *mem_size, image_head_size, bitmap_size, crc_io_size);

	test_mem = malloc(*mem_size);
	if (test_mem == NULL){
		free(test_mem);
		return -1;
	} else {
		free(test_mem);
	}
	return 1;
}

/// get bitmap from image file to restore data
void get_image_bitmap(int* ret, cmd_opt opt, image_head image_hdr, unsigned long* bitmap) {
	unsigned long long size, r_size, r_need;
	char buffer[16384];
	unsigned long long offset = 0;
	unsigned long long bused = 0, bfree = 0;
	int i, debug = opt.debug;
	int err_exit = 1;
	char bitmagic_r[8]="00000000";/// read magic string from image

	size = image_hdr.totalblock;

	while (size > 0) {
		r_need = size > sizeof(buffer) ? sizeof(buffer) : size;
		r_size = read_all(ret, buffer, r_need, &opt);
		if (r_size < r_need)
			log_mesg(0, 1, 1, debug, "Unable to read bitmap.\n");
		for (i = 0; i < r_need; i++) {
			if (buffer[i] == 1) {
				pc_set_bit(offset + i, bitmap);
				bused++;
			} else {
				pc_clear_bit(offset + i, bitmap);
				bfree++;
			}
		}
		offset += r_need;
		size -= r_need;
	}

	if (debug >= 2) {
		if (image_hdr.usedblocks != bused) {
			if (opt.force)
				err_exit = 0;
			else
				err_exit = 1;
			log_mesg(0, err_exit, 1, debug, "The Used Block count is different. (bitmap %llu != image_head %llu)\n"
				"Try to use --force to skip the metadata error.\n", bused, image_hdr.usedblocks);
		}
	}

	/// read magic string from image file and check it.
	if (read_all(ret, bitmagic_r, BIT_MAGIC_SIZE, &opt) != BIT_MAGIC_SIZE)
		log_mesg(0, 1, 1, debug, "read magic ERROR:%s\n", strerror(errno));
	if (memcmp(bitmagic_r, BIT_MAGIC, BIT_MAGIC_SIZE))
		log_mesg(0, 1, 1, debug, "can't find bitmagic\n");
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
		free(real_file);
		return -1;
	}

	if (!realpath(device, real_file))
		return -1;

	if ((f = setmntent(MOUNTED, "r")) == 0) {
		free(real_file);
		free(real_fsname);
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

	free(real_file);
	free(real_fsname);
	return isMounted;
}

int open_source(char* source, cmd_opt* opt) {
	int ret = 0;
	int debug = opt->debug;
	char *mp;
	int flags = O_RDONLY | O_LARGEFILE;

	log_mesg(1, 0, 0, debug, "open source file/device %s\n", source);
	if ((opt->clone) || (opt->dd) || (opt->domain)) { /// always is device, clone from device=source

		mp = malloc(PATH_MAX + 1);
		if (!mp)
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);

		if (check_mount(source, mp) == 1) {
			log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", source, mp);
			free(mp);
			log_mesg(0, 1, 1, debug, "error exit\n");
		}
		free(mp);

		if ((ret = open(source, flags, S_IRUSR)) == -1)
			log_mesg(0, 1, 1, debug, "clone: open %s error\n", source);

	} else if (opt->restore) {

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

int open_target(char* target, cmd_opt* opt) {
	int ret = 0;
	int debug = opt->debug;
	char *mp;
	int flags = O_WRONLY | O_LARGEFILE;
	struct stat st_dev;

	log_mesg(1, 0, 0, debug, "open target file/device\n");
	if (opt->clone || opt->domain) {
		if (strcmp(target, "-") == 0) {
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
	} else if ((opt->restore) || (opt->dd)) {    /// always is device, restore to device=target

		/// check mounted
		mp = malloc(PATH_MAX + 1);
		if (!mp)
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		if (check_mount(target, mp)) {
			log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", target, mp);
			free(mp);
			log_mesg(0, 1, 1, debug, "error exit\n");
		}
		free(mp);

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
	}

	return ret;
}

/// the io function, reference from ntfsprogs(ntfsclone).
int io_all(int *fd, char *buf, unsigned long long count, int do_write, cmd_opt* opt) {
	long long int i;
	int debug = opt->debug;
	unsigned long long size = count;

	// for sync I/O buffer, when use stdin or pipe.
	while (count > 0) {
		if (do_write)
			i = write(*fd, buf, count);
		else
			i = read(*fd, buf, count);

		if (i < 0) {
			log_mesg(1, 0, 1, debug, "%s: errno = %i(%s)\n",__func__, errno, strerror(errno));
			if (errno != EAGAIN && errno != EINTR) {
				return -1;
			}
		} else if (i == 0) {
			log_mesg(1, 0, 1, debug, "%s: nothing to read. errno = %i(%s)\n",__func__, errno, strerror(errno));
			return 0;
		} else {
			count -= i;
			buf = i + (char *) buf;
			log_mesg(2, 0, 0, debug, "%s: read %lli, %llu left.\n",__func__, i, count);
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
	const char *badsector_magic = "BADSECTOR\0";

	if (lseek(*fd, pos, SEEK_SET) == (off_t)-1) {
		log_mesg(0, 0, 1, opt->debug, "WARNING: lseek error at %llu\n", pos);
		memset(buff, '?', PART_SECTOR_SIZE);
		memmove(buff, badsector_magic, sizeof(badsector_magic));
		return;
	}

	if (io_all(fd, buff, PART_SECTOR_SIZE, 0, opt) == -1) { /// read_all
		log_mesg(0, 0, 1, opt->debug, "WARNING: Can't read sector at %llu, lost data.\n", pos);
		memset(buff, '?', PART_SECTOR_SIZE);
		memmove(buff, badsector_magic, sizeof(badsector_magic));
	}
}

/// the crc32 function, reference from libcrc.
/// Author is Lammert Bies  1999-2007
/// Mail: info@lammertbies.nl
/// http://www.lammertbies.nl/comm/info/nl_crc-calculation.html 
/// generate crc32 code
unsigned long crc32(unsigned long crc, char *buf, int size){

	static unsigned long crc_tab32[256];
	static int init = 0;

	unsigned long tmp, long_c;
	int s = 0;

	if (init == 0) {
		/// initial crc table
		unsigned long init_crc, init_p;
		int i, j;
		init_p = 0xEDB88320L;

		for (i = 0; i < 256; i++) {
			init_crc = (unsigned long) i;
			for (j = 0; j < 8; j++) {
				if ( init_crc & 0x00000001L ) init_crc = ( init_crc >> 1 ) ^ init_p;
				else init_crc = init_crc >> 1;
			}
			crc_tab32[i] = init_crc;
		}
		init = 1;
	}

	do {
		/// update crc
		long_c = 0x000000ffL & (unsigned long) *buf;
		tmp = crc ^ long_c;
		crc = (crc >> 8) ^ crc_tab32[ tmp & 0xff ];
	} while (++s < size);

	return crc;
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

	log_mesg(1, 0, 0, debug, "DEBUG: %i\n", opt.debug);
	log_mesg(1, 0, 0, debug, "SOURCE: %s\n", opt.source);
	log_mesg(1, 0, 0, debug, "TARGET: %s\n", opt.target);
	log_mesg(1, 0, 0, debug, "OVERWRITE: %i\n", opt.overwrite);
	log_mesg(1, 0, 0, debug, "RESCUE: %i\n", opt.rescue);
	log_mesg(1, 0, 0, debug, "CHECK: %i\n", opt.check);
	log_mesg(1, 0, 0, debug, "QUIET: %i\n", opt.quiet);
	log_mesg(1, 0, 0, debug, "FRESH: %i\n", opt.fresh);
	log_mesg(1, 0, 0, debug, "FORCE: %i\n", opt.force);
#ifdef HAVE_LIBNCURSESW
	log_mesg(1, 0, 0, debug, "NCURSES: %i\n", opt.ncurses);
#endif
	log_mesg(1, 0, 0, debug, "OFFSET DOMAIN: 0x%llX\n", opt.offset_domain);
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
	else if (opt.clone)
		log_mesg(0, 0, 1, debug, _("Starting to clone device (%s) to image (%s)\n"), opt.source, opt.target);	
	else if(opt.restore)
		log_mesg(0, 0, 1, debug, _("Starting to restore image (%s) to device (%s)\n"), opt.source, opt.target);
	else if(opt.dd)
		log_mesg(0, 0, 1, debug, _("Starting to back up device(%s) to device(%s)\n"), opt.source, opt.target);
	else if (opt.domain)
		log_mesg(0, 0, 1, debug, _("Starting to map device (%s) to domain log (%s)\n"), opt.source, opt.target);
	else
		log_mesg(0, 0, 1, debug, "unknow mode\n");
}

/// print image head
void print_image_hdr_info(image_head image_hdr, cmd_opt opt) {

	int block_s  = image_hdr.block_size;
	unsigned long long total    = image_hdr.totalblock;
	unsigned long long used     = image_hdr.usedblocks;
	int debug = opt.debug;
	char size_str[11];

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	log_mesg(0, 0, 1, debug, _("File system:  %s\n"), image_hdr.fs);

	print_readable_size_str(total*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Device size:  %s = %llu Blocks\n"), size_str, total);

	print_readable_size_str(used*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Space in use: %s = %llu Blocks\n"), size_str, used);

	print_readable_size_str((total-used)*block_s, size_str);
	log_mesg(0, 0, 1, debug, _("Free Space:   %s = %llu Blocks\n"), size_str, (total-used));

	log_mesg(0, 0, 1, debug, _("Block size:   %i Byte\n"), block_s);
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
