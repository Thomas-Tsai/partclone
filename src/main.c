/**
 * The main program of partclone 
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * clone/restore partition to a image, device or stdout.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>
#include <errno.h>
#include <features.h>
#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <mcheck.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/**
 * progress.h - only for progress bar
 */
#include "progress.h"

void *thread_update_pui(void *arg);
/// progress_bar structure defined in progress.h
progress_bar prog;
unsigned long long copied;
unsigned long long block_id;
int done;


/**
 * partclone.h - include some structures like image_head, opt_cmd, ....
 *               and functions for main used.
 */
#include "partclone.h"

/// cmd_opt structure defined in partclone.h
cmd_opt opt;

/**
 * Include different filesystem header depend on what flag you want.
 * If cflag is _EXTFS, output to extfsclone.
 * My goal is give different clone utils by makefile .
 */
#ifdef EXTFS
#include "extfsclone.h"
#define FS "EXTFS"
#elif REISERFS
#include "reiserfsclone.h"
#define FS "REISERFS"
#elif REISER4
#include "reiser4clone.h"
#define FS "REISER4"
#elif XFS
#include "xfsclone.h"
#define FS "XFS"
#elif HFSPLUS
#include "hfsplusclone.h"
#define FS "HFS Plus"
#elif FAT
#include "fatclone.h"
#define FS "FAT"
#elif NTFS
#include "ntfsclone-ng.h"
#define FS "NTFS"
#elif NTFS3G
#include "ntfsclone-ng.h"
#define FS "NTFS"
#elif UFS
#include "ufsclone.h"
#define FS "UFS"
#elif VMFS
#include "vmfsclone.h"
#define FS "VMFS"
#elif JFS
#include "jfsclone.h"
#define FS "JFS"
#elif BTRFS
#include "btrfsclone.h"
#define FS "BTRFS"
#elif EXFAT
#include "exfatclone.h"
#define FS "EXFAT"
#elif MINIX
#include "minixclone.h"
#define FS "MINIX"
#elif DD
#include "ddclone.h"
#define FS "raw"
#ifdef RESTORE
char *EXECNAME = "partclone.restore";
#else
#ifdef CHKIMG
char *EXECNAME = "partclone.chkimg";
#else
char *EXECNAME = "partclone.dd";
#endif
#endif
#endif

/// fs option
#include "fs_common.h"
/// cmd_opt structure defined in partclone.h
fs_cmd_opt fs_opt;

/**
 * main function - for clone or restore data
 */
int main(int argc, char **argv) {
#ifdef MEMTRACE
	setenv("MALLOC_TRACE", "partclone_mtrace.log", 1);
	mtrace();
#endif
	char*			source;			/// source data
	char*			target;			/// target data
	int			dfr, dfw;		/// file descriptor for source and target
	int			r_size, w_size;		/// read and write size
	int			start, stop;		/// start, range, stop number for progress bar
	char			bitmagic[8] = "BiTmAgIc";// only for check postition
	char			bitmagic_r[8]="00000000";/// read magic string from image
	unsigned long		*bitmap = NULL;		/// the point for bitmap data
	int			debug = 0;		/// debug level
	int			tui = 0;		/// text user interface
	int			pui = 0;		/// progress mode(default text)
	
	int			flag;
	int			pres = 0;
	pthread_t		prog_thread;
	void			*p_result;

	static const char *const bad_sectors_warning_msg =
		"*************************************************************************\n"
		"* WARNING: The disk has bad sectors. This means physical damage on the  *\n"
		"* disk surface caused by deterioration, manufacturing faults, or        *\n"
		"* another reason. The reliability of the disk may remain stable or      *\n"
		"* degrade quickly. Use the --rescue option to efficiently save as much  *\n"
		"* data as possible!                                                     *\n"
		"*************************************************************************\n";

	image_head		image_hdr;		/// image_head structure defined in partclone.h
	memset(&image_hdr, 0, sizeof(image_hdr));

	/**
	 * get option and assign to opt structure
	 * check parameter and read from argv
	 */
	parse_options(argc, argv, &opt);

	/**
	 * if "-d / --debug" given
	 * open debug file in "/var/log/partclone.log" for log message 
	 */
	memset(&fs_opt, 0, sizeof(fs_cmd_opt));
	debug = opt.debug;
	fs_opt.debug = debug;
	fs_opt.ignore_fschk = opt.ignore_fschk;

	//if(opt.debug)
	open_log(opt.logfile);

	/**
	 * using Text User Interface
	 */
	if (opt.ncurses) {
		pui = NCURSES;
		log_mesg(1, 0, 0, debug, "Using Ncurses User Interface mode.\n");
	} else
		pui = TEXT;

	tui = open_pui(pui, opt.fresh);
	if ((opt.ncurses) && (!tui)) {
		opt.ncurses = 0;
		pui = TEXT;
		log_mesg(1, 0, 0, debug, "Open Ncurses User Interface Error.\n");
	}

	/// print partclone info
	print_partclone_info(opt);

#ifndef CHKIMG
	if (geteuid() != 0)
		log_mesg(0, 1, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n");
	else
		log_mesg(1, 0, 0, debug, "UID is root.\n");
#endif

	/// ignore crc check
	if (opt.ignore_crc)
		log_mesg(1, 0, 1, debug, "Ignore CRC errors\n");

	/**
	 * open source and target
	 * clone mode, source is device and target is image file/stdout
	 * restore mode, source is image file/stdin and target is device
	 * dd mode, source is device and target is device !!not complete
	 */
	source = opt.source;
	target = opt.target;
	log_mesg(1, 0, 0, debug, "source=%s, target=%s \n", source, target);
	dfr = open_source(source, &opt);
	if (dfr == -1) {
		log_mesg(0, 1, 1, debug, "Error exit\n");
	}

#ifndef CHKIMG
	dfw = open_target(target, &opt);
	if (dfw == -1) {
		log_mesg(0, 1, 1, debug, "Error exit\n");
	}
#else
	dfw = -1;
#endif

	/**
	 * get partition information like super block, image_head, bitmap
	 * from device or image file.
	 */
	if (opt.clone) {

		char bbuffer[16384];
		unsigned long long i, needed_size, needed_mem;

		log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
		log_mesg(0, 0, 1, debug, "Reading Super Block\n");

		/// get Super Block information from partition
		initial_image_hdr(source, &image_hdr);

		/// check memory size
		if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
			log_mesg(0, 1, 1, debug, "There is not enough free memory, partclone suggests you should have %llu bytes memory\n", needed_mem);

		strncpy(image_hdr.version, IMAGE_VERSION, VERSION_SIZE);

		/// alloc a memory to store bitmap
		bitmap = pc_alloc_bitmap(image_hdr.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from partition
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
		read_bitmap(source, image_hdr, bitmap, pui);

		needed_size = (unsigned long long)(((image_hdr.block_size+CRC_SIZE)*image_hdr.usedblocks)+sizeof(image_hdr)+image_hdr.totalblock);
		if (opt.check)
			check_free_space(&dfw, needed_size);

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Writing super block and bitmap... ");

		/// write image_head to image file
		if (write_all(&dfw, (char *)&image_hdr, sizeof(image_head), &opt) == -1)
			log_mesg(0, 1, 1, debug, "write image_hdr to image error: %s\n", strerror(errno));

		/// write bitmap information to image file
		for (i = 0; i < image_hdr.totalblock; i++) {
			if (pc_test_bit(i, bitmap)) {
				bbuffer[i % sizeof(bbuffer)] = 1;
			} else {
				bbuffer[i % sizeof(bbuffer)] = 0;
			}
			if (i % sizeof(bbuffer) == sizeof(bbuffer) - 1 || i == image_hdr.totalblock - 1) {
				if (write_all(&dfw, bbuffer, 1 + (i % sizeof(bbuffer)), &opt) == -1)
					log_mesg(0, 1, 1, debug, "write bitmap to image error\n");
			}
		}

		log_mesg(0, 0, 1, debug, "done!\n");

	} else if (opt.restore) {

		unsigned long long needed_mem;

		log_mesg(1, 0, 0, debug, "restore image hdr - get image_head from image file\n");
		log_mesg(1, 0, 1, debug, "Reading Super Block\n");

		/// get image information from image file
		restore_image_hdr(&dfr, &opt, &image_hdr);

		/// check memory size
		if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
			log_mesg(0, 1, 1, debug, "There is not enough free memory, partclone suggests you should have %llu bytes memory\n", needed_mem);

		/// alloc a memory to restore bitmap
		bitmap = pc_alloc_bitmap(image_hdr.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		/// check the image magic
		if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE))
			log_mesg(0, 1, 1, debug, "This is not partclone image.\n");

		/// check the file system
		//if (strcmp(image_hdr.fs, FS) != 0)
		//	log_mesg(0, 1, 1, debug, "%s can't restore from the image which filesystem is %s not %s\n", argv[0], image_hdr.fs, FS);

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from image file
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
		get_image_bitmap(&dfr, opt, image_hdr, bitmap);

#ifndef CHKIMG
		/// check the dest partition size.
		if (opt.restore_raw_file)
			check_free_space(&dfw, image_hdr.device_size);
		else if (opt.check)
			check_size(&dfw, image_hdr.device_size);
#endif

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(0, 0, 1, debug, "done!\n");

	} else if (opt.dd || opt.domain) {

		unsigned long long needed_mem;

		log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
		log_mesg(1, 0, 1, debug, "Reading Super Block\n");

		/// get Super Block information from partition
		initial_image_hdr(source, &image_hdr);

		/// check memory size
		if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
			log_mesg(0, 1, 1, debug, "There is not enough free memory, partclone suggests you should have %llu bytes memory\n", needed_mem);

		strncpy(image_hdr.version, IMAGE_VERSION, VERSION_SIZE);

		/// alloc a memory to restore bitmap
		bitmap = pc_alloc_bitmap(image_hdr.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from partition
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
		read_bitmap(source, image_hdr, bitmap, pui);

		/// check the dest partition size.
		if (opt.dd && opt.check) {
			check_size(&dfw, image_hdr.device_size);
		}

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(0, 0, 1, debug, "done!\n");
	}

	log_mesg(1, 0, 0, debug, "print image_head\n");

	/// print option to log file
	if (debug)
		print_opt(opt);

	/// print image_head
	print_image_hdr_info(image_hdr, opt);

	/**
	 * initial progress bar
	 */
	start = 0;				/// start number of progress bar
	stop = (image_hdr.usedblocks);		/// get the end of progress number, only used block
	log_mesg(1, 0, 0, debug, "Initial Progress bar\n");
	/// Initial progress bar
	if (opt.no_block_detail)
		flag = NO_BLOCK_DETAIL;
	else
		flag = IO;
	progress_init(&prog, start, stop, image_hdr.totalblock, flag, image_hdr.block_size);
	copied = 0;				/// initial number is 0

	/**
	 * thread to print progress
	 */
	pres = pthread_create(&prog_thread, NULL, thread_update_pui, NULL);
	if(pres)
	    log_mesg(0, 1, 1, debug, "%s, %i, thread create error\n", __func__, __LINE__);


	/**
	 * start read and write data between source and destination
	 */
	if (opt.clone) {

		unsigned long crc = 0xffffffffL;
		int block_size = image_hdr.block_size;
		unsigned long long blocks_total = image_hdr.totalblock;
		int blocks_in_buffer = block_size < opt.buffer_size ? opt.buffer_size / block_size : 1;
		char *read_buffer, *write_buffer;

		read_buffer = (char*)malloc(blocks_in_buffer * block_size);
		write_buffer = (char*)malloc(blocks_in_buffer * (block_size + CRC_SIZE));
		if (read_buffer == NULL || write_buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		/// write a magic string
		w_size = write_all(&dfw, bitmagic, 8, &opt);

		/// read data from the first block
		if (lseek(dfr, 0, SEEK_SET) == (off_t)-1)
			log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));

		log_mesg(0, 0, 0, debug, "Total block %llu\n", blocks_total);

		/// start clone partition to image file
		log_mesg(1, 0, 0, debug, "start backup data...\n");

		block_id = 0;
		do {
			/// scan bitmap
			unsigned long long i, blocks_skip, blocks_read;
			off_t offset;

			/// skip chunk
			for (blocks_skip = 0;
			     block_id + blocks_skip < blocks_total &&
			     !pc_test_bit(block_id + blocks_skip, bitmap);
			     blocks_skip++);
			if (block_id + blocks_skip == blocks_total)
				break;

			if (blocks_skip)
				block_id += blocks_skip;

			/// read chunk
			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < blocks_in_buffer &&
			     pc_test_bit(block_id + blocks_read, bitmap);
			     blocks_read++);
			if (!blocks_read)
				break;

			offset = (off_t)(block_id * block_size);
			if (lseek(dfr, offset, SEEK_SET) == (off_t)-1)
				log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));

			r_size = read_all(&dfr, read_buffer, blocks_read * block_size, &opt);
			if (r_size != (int)(blocks_read * block_size)) {
				if ((r_size == -1) && (errno == EIO)) {
					if (opt.rescue) {
						memset(read_buffer, 0, blocks_read * block_size);
						for (r_size = 0; r_size < blocks_read * block_size; r_size += PART_SECTOR_SIZE)
							rescue_sector(&dfr, offset + r_size, read_buffer + r_size, &opt);
					} else
						log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);
				} else
					log_mesg(0, 1, 1, debug, "read error: %s\n", strerror(errno));
			}

			/// calculate crc
			for (i = 0; i < blocks_read; i++) {
				crc = crc32(crc, read_buffer + i * block_size, block_size);
				memcpy(write_buffer + i * (block_size + CRC_SIZE),
					read_buffer + i * block_size, block_size);
				memcpy(write_buffer + i * (block_size + CRC_SIZE) + block_size,
					&crc, CRC_SIZE);
			}

			/// write buffer to target
			w_size = write_all(&dfw, write_buffer, blocks_read * (block_size + CRC_SIZE), &opt);
			if (w_size != blocks_read * (block_size + CRC_SIZE))
				log_mesg(0, 1, 1, debug, "image write ERROR:%s\n", strerror(errno));

			/// count copied block
			copied += blocks_read;

			/// next block
			block_id += blocks_read;

			/// read or write error
			if (r_size + blocks_read * CRC_SIZE != w_size)
				log_mesg(0, 1, 1, debug, "read(%i) and write(%i) different\n", r_size, w_size);

		} while (1);

		free(write_buffer);
		free(read_buffer);

	} else if (opt.restore) {

		unsigned long crc = 0xffffffffL;
		char *read_buffer, *write_buffer;
		int block_size = image_hdr.block_size;
		unsigned long long blocks_used_fix = 0, test_block = 0;
		unsigned long long blocks_used = image_hdr.usedblocks;
		unsigned long long blocks_total = image_hdr.totalblock;
		int blocks_in_buffer = block_size < opt.buffer_size ? opt.buffer_size / block_size : 1;


		// fix some super block record incorrect
		for (test_block = 0; test_block < blocks_total; test_block++)
		    if ( pc_test_bit(test_block, bitmap))
			blocks_used_fix++;
		
		if (blocks_used_fix != blocks_used)
		    blocks_used = blocks_used_fix;

		read_buffer = (char*)malloc(blocks_in_buffer * (block_size + 2 * CRC_SIZE));
		write_buffer = (char*)malloc(blocks_in_buffer * (block_size + CRC_SIZE));
		if (read_buffer == NULL || write_buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		/// read magic string from image file and check it.
		if (read_all(&dfr, bitmagic_r, 8, &opt) != 8)
			log_mesg(0, 1, 1, debug, "read magic ERROR:%s\n", strerror(errno));
		if (memcmp(bitmagic, bitmagic_r, 8))
			log_mesg(0, 1, 1, debug, "can't find bitmagic\n");

#ifndef CHKIMG
		/// seek to the first
		if (lseek(dfw, opt.offset, SEEK_SET) == (off_t)-1)
			log_mesg(0, 1, 1, debug, "target seek ERROR:%s\n", strerror(errno));
#endif

		/// start restore image file to partition
		log_mesg(1, 0, 0, debug, "start restore data...\n");

		block_id = 0;
		do {
			int i, bugcheck = 0;
			unsigned long crc_saved;
			unsigned long long blocks_written, bytes_skip;
			// max chunk to read using one read(2) syscall
			int blocks_read = copied + blocks_in_buffer < blocks_used ?
				blocks_in_buffer : blocks_used - copied;
			if (!blocks_read)
				break;

			// read chunk from image
			r_size = read_all(&dfr, read_buffer, blocks_read * (block_size + CRC_SIZE), &opt);
			if (r_size != blocks_read * (block_size + CRC_SIZE))
				log_mesg(0, 1, 1, debug, "read ERROR:%s\n", strerror(errno));

			// read buffer is the follows:
			// <block1><crc1><block2><crc2>...

			// write buffer should be the follows:
			// <block1><block2>...

			// fill up write buffer, check crc
			recheck:
			crc_saved = crc;
			for (i = 0; i < blocks_read; i++) {
				memcpy(write_buffer + i * block_size,
					read_buffer + i * (block_size + CRC_SIZE), block_size);
				if (opt.ignore_crc)
					continue;
				// check crc
				crc = crc32(crc, read_buffer + i * (block_size + CRC_SIZE), block_size);
				if (memcmp(read_buffer + i * (block_size + CRC_SIZE) + block_size, &crc, CRC_SIZE)) {
					if (bugcheck)
						log_mesg(0, 1, 1, debug, "CRC error, block_id=%llu...\n ", block_id + i);
					else
						log_mesg(1, 0, 0, debug, "CRC Check error. 64bit bug before v0.1.0 (Rev:250M), "
							"trying enlarge crc size and recheck again...\n");
					// try to read (blocks_read * CRC_SIZE) bytes to the end of read_buffer
					if (read_all(&dfr, read_buffer + blocks_read * (block_size + CRC_SIZE),
						blocks_read * CRC_SIZE, &opt) != blocks_read * CRC_SIZE) {
						log_mesg(0, 1, 1, debug, "read CRC error: %s, please check your image file.\n",
							strerror(errno));
					}
					// reshape read_buffer and try to check crc again
					if (i > 0) {
						// first block correct
						// i.e. read buffer is the follows:
						// <block1><crc1><bug><block2><crc2><bug>...
						for (i = 0; i < blocks_read; i++) {
							memcpy(write_buffer + i * (block_size + CRC_SIZE),
								read_buffer + i * (block_size + 2*CRC_SIZE),
								block_size + CRC_SIZE);
						}
					} else {
						// first block incorrect
						// i.e. read buffer is the follows:
						// <bug><block1><crc1><bug><block2><crc2>...
						for (i = 0; i < blocks_read; i++) {
							memcpy(write_buffer + i * (block_size + CRC_SIZE),
								read_buffer + i * (block_size + 2*CRC_SIZE) + CRC_SIZE,
								block_size + CRC_SIZE);
						}
					}
					memcpy(read_buffer, write_buffer, blocks_read * (block_size + CRC_SIZE));
					crc = crc_saved;
					bugcheck = 1;
					goto recheck;
				}
			}

			blocks_written = 0;
			do {
				int blocks_write;

				/// count bytes to skip
				for (bytes_skip = 0;
				     block_id < blocks_total &&
				     !pc_test_bit(block_id, bitmap);
				     block_id++, bytes_skip += block_size);

#ifndef CHKIMG
				/// skip empty blocks
				if (bytes_skip > 0 && lseek(dfw, (off_t)bytes_skip, SEEK_CUR) == (off_t)-1)
					log_mesg(0, 1, 1, debug, "target seek ERROR:%s\n", strerror(errno));
#endif

				/// blocks to write
				for (blocks_write = 0;
				     block_id + blocks_write < blocks_total &&
				     blocks_written + blocks_write < blocks_read &&
				     pc_test_bit(block_id + blocks_write, bitmap);
				     blocks_write++);

#ifndef CHKIMG
				// write blocks
				if (blocks_write > 0) {
					w_size = write_all(&dfw, write_buffer + blocks_written * block_size,
						blocks_write * block_size, &opt);
					if (w_size != blocks_write * block_size) {
						if (!opt.skip_write_error)
							log_mesg(0, 1, 1, debug, "write block %llu ERROR:%s\n", block_id + blocks_written, strerror(errno));
						else
							log_mesg(0, 0, 1, debug, "skip write block %llu error:%s\n", block_id + blocks_written, strerror(errno));
					}
				}
#endif

				blocks_written += blocks_write;
				block_id += blocks_write;
				copied += blocks_write;
			} while (blocks_written < blocks_read);

		} while(1);

		free(write_buffer);
		free(read_buffer);

#ifndef CHKIMG
		/// restore_raw_file option
		if (opt.restore_raw_file && !pc_test_bit(blocks_total - 1, bitmap)) {
			if (ftruncate(dfw, (off_t)(blocks_total * block_size)) == -1)
				log_mesg(0, 0, 1, debug, "ftruncate ERROR:%s\n", strerror(errno));
		}
#endif

	} else if (opt.dd) {

		char *buffer;
		int block_size = image_hdr.block_size;
		unsigned long long blocks_total = image_hdr.totalblock;
		int blocks_in_buffer = block_size < opt.buffer_size ? opt.buffer_size / block_size : 1;

		buffer = (char*)malloc(blocks_in_buffer * block_size);
		if (buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		block_id = 0;

		if (lseek(dfr, 0, SEEK_SET) == (off_t)-1)
			log_mesg(0, 1, 1, debug, "source seek ERROR:%d\n", strerror(errno));

		log_mesg(0, 0, 0, debug, "Total block %llu\n", blocks_total);

		/// start clone partition to partition
		log_mesg(1, 0, 0, debug, "start backup data device-to-device...\n");
		do {
			/// scan bitmap
			unsigned long long blocks_skip, blocks_read;
			off_t offset;

			/// skip chunk
			for (blocks_skip = 0;
			     block_id + blocks_skip < blocks_total &&
			     !pc_test_bit(block_id + blocks_skip, bitmap);
			     blocks_skip++);

			if (block_id + blocks_skip == blocks_total)
				break;

			if (blocks_skip)
				block_id += blocks_skip;

			/// read chunk from source
			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < blocks_in_buffer &&
			     pc_test_bit(block_id + blocks_read, bitmap);
			     blocks_read++);

			if (!blocks_read)
				break;

			offset = (off_t)(block_id * block_size);
			if (lseek(dfr, offset, SEEK_SET) == (off_t)-1)
				log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));
			if (lseek(dfw, offset + opt.offset, SEEK_SET) == (off_t)-1)
				log_mesg(0, 1, 1, debug, "target seek ERROR:%s\n", strerror(errno));

			r_size = read_all(&dfr, buffer, blocks_read * block_size, &opt);
			if (r_size != (int)(blocks_read * block_size)) {
				if ((r_size == -1) && (errno == EIO)) {
					if (opt.rescue) {
						memset(buffer, 0, blocks_read * block_size);
						for (r_size = 0; r_size < blocks_read * block_size; r_size += PART_SECTOR_SIZE)
							rescue_sector(&dfr, offset + r_size, buffer + r_size, &opt);
					} else
						log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);
				} else
					log_mesg(0, 1, 1, debug, "source read ERROR %s\n", strerror(errno));
			}

			/// write buffer to target
			w_size = write_all(&dfw, buffer, blocks_read * block_size, &opt);
			if (w_size != (int)(blocks_read * block_size)) {
				if (opt.skip_write_error)
					log_mesg(0, 0, 1, debug, "skip write block %lli error:%s\n", block_id, strerror(errno));
				else
					log_mesg(0, 1, 1, debug, "write block %lli ERROR:%s\n", block_id, strerror(errno));
			}

			/// count copied block
			copied += blocks_read;

			/// next block
			block_id += blocks_read;

			/// read or write error
			if (r_size != w_size) {
				if (opt.skip_write_error)
					log_mesg(0, 0, 1, debug, "read and write different\n");
				else
					log_mesg(0, 1, 1, debug, "read and write different\n");
			}
		} while (1);

		free(buffer);

		/// restore_raw_file option
		if (opt.restore_raw_file && !pc_test_bit(blocks_total - 1, bitmap)) {
			if (ftruncate(dfw, (off_t)(blocks_total * block_size)) == -1)
				log_mesg(0, 0, 1, debug, "ftruncate ERROR:%s\n", strerror(errno));
		}

	} else if (opt.domain) {

		int cmp, nx_current = 0;
		unsigned long long next_block_id = 0;
		log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);
		log_mesg(1, 0, 0, debug, "start writing domain log...\n");
		// write domain log comment and status line
		dprintf(dfw, "# Domain logfile created by %s v%s\n", EXECNAME, VERSION);
		dprintf(dfw, "# Source: %s\n", opt.source);
		dprintf(dfw, "# Offset: 0x%08llX\n", opt.offset_domain);
		dprintf(dfw, "# current_pos  current_status\n");
		dprintf(dfw, "0x%08llX     ?\n", opt.offset_domain + (image_hdr.totalblock * image_hdr.block_size));
		dprintf(dfw, "#      pos        size  status\n");
		// start logging the used/unused areas
		cmp = pc_test_bit(0, bitmap);
		for (block_id = 0; block_id <= image_hdr.totalblock; block_id++) {
			if (block_id < image_hdr.totalblock) {
				nx_current = pc_test_bit(block_id, bitmap);
				if (nx_current)
					copied++;
			} else
				nx_current = -1;
			if (nx_current != cmp) {
				dprintf(dfw, "0x%08llX  0x%08llX  %c\n",
					opt.offset_domain + (next_block_id * image_hdr.block_size),
					(block_id - next_block_id) * image_hdr.block_size,
					cmp ? '+' : '?');
				next_block_id = block_id;
				cmp = nx_current;
			}
			// don't bother updating progress
		} /// end of for
	}

	done = 1;
	pres = pthread_join(prog_thread, &p_result);
	if(pres)
	    log_mesg(0, 1, 1, debug, "%s, %i, thread join error\n", __func__, __LINE__);
	update_pui(&prog, copied, block_id, done);
#ifndef CHKIMG
	sync_data(dfw, &opt);
#endif
	print_finish_info(opt);

	/// close source
	close(dfr);
	/// close target
	if (dfw != -1)
		close(dfw);
	/// free bitmp
	free(bitmap);
	close_pui(pui);
#ifndef CHKIMG
	fprintf(stderr, "Cloned successfully.\n");
#else
	printf("Checked successfully.\n");
#endif
	if (opt.debug)
		close_log();
#ifdef MEMTRACE
	muntrace();
#endif
	return 0;      /// finish
}

void *thread_update_pui(void *arg) {

	while (!done) {
		if (!opt.quiet)
			update_pui(&prog, copied, block_id, done);
		sleep(opt.fresh);
	}
	pthread_exit("exit");
}
