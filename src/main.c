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
#define _GNU_SOURCE
#define _LARGEFILE64_SOURCE
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
#include <assert.h>
#include <dirent.h>
#include <limits.h>

// SHA1 for torrent info
#include "torrent_helper.h"

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

#include "partclone.h"

/// cmd_opt structure defined in partclone.h
cmd_opt opt;

#include "checksum.h"

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
	unsigned		cs_size = 0;		/// checksum_size
	int			cs_reseed = 1;
	int			start;
	unsigned long long      stop;		/// start, range, stop number for progress bar
	unsigned long *bitmap = NULL;		/// the point for bitmap data
	int			debug = 0;		/// debug level
	int			tui = 0;		/// text user interface
	int			pui = 0;		/// progress mode(default text)
	
	int			flag;
	int			pres = 0;
	pthread_t		prog_thread;
	void			*p_result;
	struct stat st_dev;

	static const char *const bad_sectors_warning_msg =
		"*************************************************************************\n"
		"* WARNING: The disk has bad sectors. This means physical damage on the  *\n"
		"* disk surface caused by deterioration, manufacturing faults, or        *\n"
		"* another reason. The reliability of the disk may remain stable or      *\n"
		"* degrade quickly. Use the --rescue option to efficiently save as much  *\n"
		"* data as possible!                                                     *\n"
		"*************************************************************************\n";

	file_system_info fs_info;   /// description of the file system
	image_options    img_opt;

	init_fs_info(&fs_info);
	init_image_options(&img_opt);

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
		log_mesg(0, 0, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n");
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
	if (opt.blockfile == 0) {
	    if (dfw == -1) {
		log_mesg(0, 1, 1, debug, "Error exit\n");
	    }
	}
#else
	dfw = -1;
#endif

	/**
	 * get partition information like super block, bitmap from device or image file.
	 */
	if (opt.clone) {

		log_mesg(1, 0, 0, debug, "Initiate image options - version %s\n", IMAGE_VERSION_CURRENT);

		img_opt.checksum_mode = opt.checksum_mode;
		img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
		img_opt.blocks_per_checksum = opt.blocks_per_checksum;
		img_opt.reseed_checksum = opt.reseed_checksum;

		cs_size = img_opt.checksum_size;
		cs_reseed = img_opt.reseed_checksum;

		log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
		log_mesg(0, 0, 1, debug, "Reading Super Block\n");

		/// get Super Block information from partition
		read_super_blocks(source, &fs_info);

		if (img_opt.checksum_mode != CSM_NONE && img_opt.blocks_per_checksum == 0) {

			const unsigned int buffer_capacity = opt.buffer_size > fs_info.block_size
				? opt.buffer_size / fs_info.block_size : 1; // in blocks

			img_opt.blocks_per_checksum = buffer_capacity;

		}
		log_mesg(1, 0, 0, debug, "%u blocks per checksum\n", img_opt.blocks_per_checksum);

		check_mem_size(fs_info, img_opt, opt);

		/// alloc a memory to store bitmap
		bitmap = pc_alloc_bitmap(fs_info.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from partition
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... \n");
		read_bitmap(source, fs_info, bitmap, pui);
		update_used_blocks_count(&fs_info, bitmap);

		/* skip check free space while torrent_only on */
		if ((opt.check) && (opt.torrent_only == 0)) {

			unsigned long long needed_space = 0;

			needed_space += sizeof(image_head) + sizeof(file_system_info) + sizeof(image_options);
			needed_space += get_bitmap_size_on_disk(&fs_info, &img_opt, &opt);
			needed_space += cnv_blocks_to_bytes(0, fs_info.usedblocks, fs_info.block_size, &img_opt);

			check_free_space(target, needed_space);
		}

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Writing super block and bitmap...\n");

		if (opt.blockfile == 0) {
			write_image_desc(&dfw, fs_info, img_opt, &opt);
			write_image_bitmap(&dfw, fs_info, img_opt, bitmap, &opt);
		}

		log_mesg(0, 0, 1, debug, "done!\n");

	} else if (opt.restore) {

		image_head_v2 img_head;

		log_mesg(1, 0, 0, debug, "restore image hdr - get information from image file\n");
		log_mesg(1, 0, 1, debug, "Reading Super Block\n");

		/// get image information from image file
		load_image_desc(&dfr, &opt, &img_head, &fs_info, &img_opt);
		cs_size = img_opt.checksum_size;
		cs_reseed = img_opt.reseed_checksum;

		check_mem_size(fs_info, img_opt, opt);

		/// alloc a memory to restore bitmap
		bitmap = pc_alloc_bitmap(fs_info.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from image file
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait...\n");
		load_image_bitmap(&dfr, opt, fs_info, img_opt, bitmap);

#ifndef CHKIMG
		/// check the dest partition size.
		if (opt.restore_raw_file)
			check_free_space(target, fs_info.device_size);
		else if ((opt.check) && (opt.blockfile == 0))
			check_size(&dfw, fs_info.device_size);
		else if (opt.blockfile == 1 && opt.torrent_only == 0)
			check_free_space(target, fs_info.usedblocks*fs_info.block_size);
#endif

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(0, 0, 1, debug, "done!\n");

	} else if (opt.dd || opt.domain) {

		log_mesg(1, 0, 0, debug, "Initiate image options - version %s\n", IMAGE_VERSION_CURRENT);
		img_opt.checksum_mode = opt.checksum_mode;
		img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
		img_opt.blocks_per_checksum = opt.blocks_per_checksum;
		img_opt.reseed_checksum = opt.reseed_checksum;
		log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
		log_mesg(1, 0, 1, debug, "Reading Super Block\n");

		/// get Super Block information from partition
		read_super_blocks(source, &fs_info);

		check_mem_size(fs_info, img_opt, opt);

		/// alloc a memory to restore bitmap
		bitmap = pc_alloc_bitmap(fs_info.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from partition
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
		read_bitmap(source, fs_info, bitmap, pui);

		/// check the dest partition size.
		if (opt.dd && opt.check) {
		    if (!opt.restore_raw_file)
			check_size(&dfw, fs_info.device_size);
		    else
			check_free_space(target, fs_info.device_size);
		}

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(0, 0, 1, debug, "done!\n");
	} else if (opt.ddd){

		if (dfr != 0){
		    fs_info.device_size = get_partition_size(&dfr);
		    read_super_blocks(source, &fs_info);
		}else{
		    if (stat(target, &st_dev) != -1) {
		        if (S_ISBLK(st_dev.st_mode)) 
			    fs_info.device_size = get_partition_size(&dfw);
			else
			    fs_info.device_size = get_free_space(target);
		    } else {
			fs_info.device_size = get_free_space(target);
		    }
		    read_super_blocks(target, &fs_info);
		}
		img_opt.checksum_mode = opt.checksum_mode;
		img_opt.checksum_size = get_checksum_size(opt.checksum_mode, opt.debug);
		img_opt.blocks_per_checksum = opt.blocks_per_checksum;
		check_mem_size(fs_info, img_opt, opt);

		/// alloc a memory to restore bitmap
		bitmap = pc_alloc_bitmap(fs_info.totalblock);
		if (bitmap == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
		log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

		/// read and check bitmap from partition
		log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
		read_bitmap(source, fs_info, bitmap, pui);

		/// check the dest partition size.
		/* skip check free space while torrent_only on */
		if ((opt.check) && (opt.torrent_only == 0)) {
		    struct stat target_stat;
		    if ((stat(opt.target, &target_stat) != -1) && (strcmp(opt.target, "-") != 0)) {
			if (S_ISBLK(target_stat.st_mode)) 
			    check_size(&dfw, fs_info.device_size);
			else {
			    unsigned long long needed_space = 0;

			    needed_space += sizeof(image_head) + sizeof(file_system_info) + sizeof(image_options);
			    needed_space += get_bitmap_size_on_disk(&fs_info, &img_opt, &opt);
			    needed_space += cnv_blocks_to_bytes(0, fs_info.usedblocks, fs_info.block_size, &img_opt);

			    check_free_space(target, needed_space);

			}
		    }
		}

		log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
		log_mesg(0, 0, 1, debug, "done!\n");
    
	}

	log_mesg(1, 0, 0, debug, "print image information\n");

	/// print option to log file
	if (debug)
		print_opt(opt);

	print_file_system_info(fs_info, opt);

	/**
	 * initial progress bar
	 */
	start = 0;				/// start number of progress bar
	stop = (fs_info.usedblocks);		/// get the end of progress number, only used block
	log_mesg(1, 0, 0, debug, "Initial Progress bar\n");
	/// Initial progress bar
	if (opt.no_block_detail)
		flag = NO_BLOCK_DETAIL;
	else
		flag = IO;
	progress_init(&prog, start, stop, fs_info.totalblock, flag, fs_info.block_size);
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

		const unsigned long long blocks_total = fs_info.totalblock;
		const unsigned int block_size = fs_info.block_size;
		const unsigned int buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1; // in blocks
		unsigned char checksum[cs_size];
		unsigned int blocks_in_cs, blocks_per_cs, write_size;
		char *read_buffer, *write_buffer;

		// SHA1 for torrent info
		int tinfo = -1;
		torrent_generator torrent;

		blocks_per_cs = img_opt.blocks_per_checksum;

		log_mesg(1, 0, 0, debug, "#\nBuffer capacity = %u, Blocks per cs = %u\n#\n", buffer_capacity, blocks_per_cs);

		write_size = cnv_blocks_to_bytes(0, buffer_capacity, block_size, &img_opt);

		read_buffer = (char*)malloc(buffer_capacity * block_size);
		write_buffer = (char*)malloc(write_size + cs_size);

		if (read_buffer == NULL || write_buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		/// read data from the first block
		if (lseek(dfr, 0, SEEK_SET) == (off_t)-1)
			log_mesg(0, 1, 1, debug, "source seek ERROR:%s\n", strerror(errno));

		log_mesg(0, 0, 0, debug, "Total block %llu\n", blocks_total);

		/// start clone partition to image file
		log_mesg(1, 0, 0, debug, "start backup data...\n");

		blocks_in_cs = 0;
		init_checksum(img_opt.checksum_mode, checksum, debug);

		if (opt.blockfile == 1) {
			char torrent_name[PATH_MAX + 1] = {'\0'};
			sprintf(torrent_name,"%s/torrent.info", target);
			tinfo = open(torrent_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

			torrent_init(&torrent, tinfo);
			dprintf(tinfo, "block_size: %u\n", block_size);
			dprintf(tinfo, "blocks_total: %llu\n", blocks_total);
		}

		block_id = 0;
		do {
			/// scan bitmap
			unsigned long long i, blocks_skip, blocks_read;
			unsigned int cs_added = 0, write_offset = 0;
			off_t offset;

			/// skip unused blocks
			for (blocks_skip = 0;
			     block_id + blocks_skip < blocks_total &&
			     !pc_test_bit(block_id + blocks_skip, bitmap, fs_info.totalblock);
			     blocks_skip++);
			if (block_id + blocks_skip == blocks_total)
				break;

			if (blocks_skip)
				block_id += blocks_skip;

			/// read blocks
			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < buffer_capacity &&
			     pc_test_bit(block_id + blocks_read, bitmap, fs_info.totalblock);
			     ++blocks_read);
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

			log_mesg(2, 0, 0, debug, "blocks_read = %i\n", blocks_read);

			/// calculate checksum
			if (opt.blockfile == 0) {
				for (i = 0; i < blocks_read; ++i) {

					memcpy(write_buffer + write_offset,
						read_buffer + i * block_size, block_size);

					write_offset += block_size;

					update_checksum(checksum, read_buffer + i * block_size, block_size);

					if (blocks_per_cs > 0 && ++blocks_in_cs == blocks_per_cs) {
					    log_mesg(3, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);

						memcpy(write_buffer + write_offset, checksum, cs_size);

						++cs_added;
						write_offset += cs_size;

						blocks_in_cs = 0;
						if (cs_reseed)
							init_checksum(img_opt.checksum_mode, checksum, debug);
					}
				}
			}

			/// write buffer to target
			if (opt.blockfile == 1) {
				// SHA1 for torrent info
				// Not always bigger or smaller than 16MB

				// first we write out block_id * block_size for filename
				// because when calling write_block_file
				// we will create a new file to describe a continuous block (or buffer is full)
				// and never write to same file again
				torrent_start_offset(&torrent, block_id * block_size);
				torrent_end_length(&torrent, blocks_read * block_size);

				torrent_update(&torrent, read_buffer, blocks_read * block_size);

				if (opt.torrent_only == 1) {
					w_size = blocks_read * block_size;
				} else {
					w_size = write_block_file(target, read_buffer, blocks_read * block_size, block_id * block_size, &opt);
				}
			} else {
				w_size = write_all(&dfw, write_buffer, write_offset, &opt);
				if (w_size != write_offset)
					log_mesg(0, 1, 1, debug, "image write ERROR:%s\n", strerror(errno));
			}

			/// count copied block
			copied += blocks_read;
			log_mesg(2, 0, 0, debug, "copied = %lld\n", copied);

			/// next block
			block_id += blocks_read;

			/// read or write error
			if (r_size + cs_added * cs_size != w_size)
				log_mesg(0, 1, 1, debug, "read(%i) and write(%i) different\n", r_size, w_size);

		} while (1);

		if (opt.blockfile == 1) {
			torrent_final(&torrent);
		} else {
			if (blocks_in_cs > 0) {

				// Write the checksum for the latest blocks
				log_mesg(1, 0, 0, debug, "Write the checksum for the latest blocks. size = %i\n", cs_size);
				log_mesg(3, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);
				w_size = write_all(&dfw, (char*)checksum, cs_size, &opt);
				if (w_size != cs_size)
					log_mesg(0, 1, 1, debug, "image write ERROR:%s\n", strerror(errno));
			}
		}

		free(write_buffer);
		free(read_buffer);

	// check only the size when the image does not contains checksums and does not
	// comes from a pipe
	} else if (opt.chkimg && img_opt.checksum_mode == CSM_NONE
		&& strcmp(opt.source, "-") != 0) {

		unsigned long long total_offset = (fs_info.usedblocks - 1) * fs_info.block_size;
		char last_block[fs_info.block_size];
		off_t partial_offset = INT32_MAX;

		while (total_offset) {

			if (partial_offset > total_offset)
				partial_offset = total_offset;

			if (lseek(dfr, partial_offset, SEEK_CUR) == (off_t)-1)
				log_mesg(0, 1, 1, debug, "source seek ERROR: %s\n", strerror(errno));

			total_offset -= partial_offset;
		}

		if (read_all(&dfr, last_block, fs_info.block_size, &opt) != fs_info.block_size)
			log_mesg(0, 1, 1, debug, "ERROR: source image too short\n");

	} else if (opt.restore) {

		const unsigned long long blocks_total = fs_info.totalblock;
		const unsigned int block_size = fs_info.block_size;
		const unsigned int buffer_capacity = opt.buffer_size > block_size ? opt.buffer_size / block_size : 1; // in blocks
		const unsigned int blocks_per_cs = img_opt.blocks_per_checksum;
		unsigned long long blocks_used = fs_info.usedblocks;
		unsigned int blocks_in_cs, buffer_size, read_offset;
		unsigned char checksum[cs_size];
		char *read_buffer, *write_buffer;
		unsigned long long blocks_used_fix = 0, test_block = 0;

		// SHA1 for torrent info
		int tinfo = -1;
		torrent_generator torrent;

		log_mesg(1, 0, 0, debug, "#\nBuffer capacity = %u, Blocks per cs = %u\n#\n", buffer_capacity, blocks_per_cs);

		// fix some super block record incorrect
		for (test_block = 0; test_block < blocks_total; ++test_block)
			if (pc_test_bit(test_block, bitmap, fs_info.totalblock))
				blocks_used_fix++;

		if (blocks_used_fix != blocks_used) {
			blocks_used = blocks_used_fix;
			log_mesg(1, 0, 0, debug, "info: fixed used blocks count\n");
		}
		buffer_size = cnv_blocks_to_bytes(0, buffer_capacity, block_size, &img_opt);

		if (img_opt.image_version != 0x0001)
			read_buffer = (char*)malloc(buffer_size);
		else {
			// Allocate more memory in case the image is affected by the 64 bits bug
			read_buffer = (char*)malloc(buffer_size + buffer_capacity * cs_size);
		}
		//write_buffer = (char*)malloc(buffer_capacity * block_size);
		#define BSIZE 512
		posix_memalign((void**)&write_buffer, BSIZE, buffer_capacity * block_size);
		if (read_buffer == NULL || write_buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

#ifndef CHKIMG
		/// seek to the first
		if (opt.blockfile == 0) {
		    if (lseek(dfw, opt.offset, SEEK_SET) == (off_t)-1){
			log_mesg(0, 1, 1, debug, "target seek ERROR:%s\n", strerror(errno));
		    }
		}
#endif

		/// start restore image file to partition
		log_mesg(1, 0, 0, debug, "start restore data...\n");

		blocks_in_cs = 0;
		if (!opt.ignore_crc)
			init_checksum(img_opt.checksum_mode, checksum, debug);

		// init SHA1 for torrent info
		if (opt.blockfile == 1) {
			char torrent_name[PATH_MAX + 1] = {'\0'};
			sprintf(torrent_name,"%s/torrent.info", target);
			tinfo = open(torrent_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

			torrent_init(&torrent, tinfo);
			dprintf(tinfo, "block_size: %u\n", block_size);
			dprintf(tinfo, "blocks_total: %llu\n", blocks_total);
		}

		block_id = 0;
		do {
			unsigned int i;
			unsigned long long blocks_written, bytes_skip;
			unsigned int read_size;
			// max chunk to read using one read(2) syscall
			unsigned int blocks_read = copied + buffer_capacity < blocks_used ?
				buffer_capacity : blocks_used - copied;
			if (!blocks_read)
			    break;
			if (blocks_read < 0)
			    log_mesg(0, 1, 1, debug, "blocks_read ERROR: impossible size of blocks_read\n");

			log_mesg(1, 0, 0, debug, "blocks_read = %d and copied = %lld\n", blocks_read, copied);
			read_size = cnv_blocks_to_bytes(copied, blocks_read, block_size, &img_opt);

			// increase read_size to make room for the oversized checksum
			if (blocks_per_cs && blocks_read < buffer_capacity &&
					(blocks_read % blocks_per_cs) && (blocks_used % blocks_per_cs)) {
				/// it is the last read and there is a partial chunk at the end
				log_mesg(1, 0, 0, debug, "# PARTIAL CHUNK\n");
				read_size += cs_size;
			}

			// read chunk from image
			log_mesg(1, 0, 0, debug, "read more: ");

			r_size = read_all(&dfr, read_buffer, read_size, &opt);
			if (r_size != read_size)
				log_mesg(0, 1, 1, debug, "read ERROR:%s\n", strerror(errno));

			// read buffer is the follows:
			// <blocks_per_cs><cs1><blocks_per_cs><cs2>...

			// write buffer should be the following:
			// <block1><block2>...

			read_offset = 0;
			for (i = 0; i < blocks_read; ++i) {

				memcpy(write_buffer + i * block_size,
					read_buffer + read_offset, block_size);

				if (opt.ignore_crc) {
					read_offset += block_size;
					if (++blocks_in_cs == blocks_per_cs){
						read_offset += cs_size;
                                                blocks_in_cs = 0;
                                            }
					continue;
				}

				update_checksum(checksum, read_buffer + read_offset, block_size);

				if (++blocks_in_cs == blocks_per_cs) {

				    unsigned char checksum_orig[cs_size];
				    memcpy(checksum_orig, read_buffer + read_offset + block_size, cs_size);
				    log_mesg(3, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);
				    log_mesg(3, 0, 0, debug, "CRC.orig = %x%x%x%x \n", checksum_orig[0], checksum_orig[1], checksum_orig[2], checksum_orig[3]);
					if (memcmp(read_buffer + read_offset + block_size, checksum, cs_size)) {
					    log_mesg(0, 1, 1, debug, "CRC error, block_id=%llu...\n ", block_id + i);
					}

					read_offset += cs_size;

					blocks_in_cs = 0;
					if (cs_reseed)
						init_checksum(img_opt.checksum_mode, checksum, debug);
				}

				read_offset += block_size;
			}
			if (!opt.ignore_crc && blocks_in_cs && blocks_per_cs && blocks_read < buffer_capacity &&
					(blocks_read % blocks_per_cs)) {

			    log_mesg(1, 0, 0, debug, "check latest chunk's checksum covering %u blocks\n", blocks_in_cs);
			    if (memcmp(read_buffer + read_offset, checksum, cs_size)){
				unsigned char checksum_orig[cs_size];
				memcpy(checksum_orig, read_buffer + read_offset, cs_size);
				log_mesg(1, 0, 0, debug, "CRC = %x%x%x%x \n", checksum[0], checksum[1], checksum[2], checksum[3]);
				log_mesg(1, 0, 0, debug, "CRC.orig = %x%x%x%x \n", checksum_orig[0], checksum_orig[1], checksum_orig[2], checksum_orig[3]);
				log_mesg(0, 1, 1, debug, "CRC error, block_id=%llu...\n ", block_id + i);
			    }

			}


			blocks_written = 0;
			do {
				unsigned int blocks_write = 0;

				/// count bytes to skip
				for (bytes_skip = 0;
				     block_id < blocks_total &&
				     !pc_test_bit(block_id, bitmap, fs_info.totalblock);
				     block_id++, bytes_skip += block_size);

#ifndef CHKIMG
				/// skip empty blocks
				if (blocks_write == 0) {
				    if (opt.blockfile == 0 && bytes_skip > 0 && lseek(dfw, (off_t)bytes_skip, SEEK_CUR) == (off_t)-1) {
					log_mesg(0, 1, 1, debug, "target seek ERROR:%s\n", strerror(errno));
				    }
				}
#endif

				/// blocks to write
				for (blocks_write = 0;
				     block_id + blocks_write < blocks_total &&
				     blocks_written + blocks_write < blocks_read &&
				     pc_test_bit(block_id + blocks_write, bitmap, fs_info.totalblock);
				     blocks_write++);

#ifndef CHKIMG
				// write blocks
				if (blocks_write > 0) {
				        if (opt.blockfile == 1){
					    // SHA1 for torrent info
					    // Not always bigger or smaller than 16MB
					    
					    // first we write out block_id * block_size for filename
					    // because when calling write_block_file
					    // we will create a new file to describe a continuous block (or buffer is full)
					    // and never write to same file again
					    torrent_start_offset(&torrent, block_id * block_size);
					    torrent_end_length(&torrent, blocks_write * block_size);

					    torrent_update(&torrent, write_buffer + blocks_written * block_size, blocks_write * block_size);

					    if (opt.torrent_only == 1) {
						w_size = blocks_write * block_size;
					    } else {
					    	w_size = write_block_file(target, write_buffer + blocks_written * block_size,
							blocks_write * block_size, (block_id*block_size), &opt);
					    }
					}else{
					    w_size = write_all(&dfw, write_buffer + blocks_written * block_size,
						    blocks_write * block_size, &opt);
					}
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

		// finish SHA1 for torrent info
		if (opt.blockfile == 1) {
			torrent_final(&torrent);
		}

		free(write_buffer);
		free(read_buffer);

#ifndef CHKIMG
		/// restore_raw_file option
		if (opt.restore_raw_file && !pc_test_bit(blocks_total - 1, bitmap, fs_info.totalblock)) {
		    if (ftruncate(dfw, (off_t)fs_info.device_size) == -1){
			log_mesg(0, 0, 1, debug, "ftruncate ERROR:%s\n", strerror(errno));
		    }
		    log_mesg(1, 0, 0, debug, "ftruncate:%llu\n", (off_t)fs_info.device_size);
		}
#endif

	} else if (opt.dd) {

		char *buffer;
		int block_size = fs_info.block_size;
		unsigned long long blocks_total = fs_info.totalblock;
		int buffer_capacity = block_size < opt.buffer_size ? opt.buffer_size / block_size : 1;

		buffer = (char*)malloc(buffer_capacity * block_size);
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

			/// skip unused blocks
			for (blocks_skip = 0;
			     block_id + blocks_skip < blocks_total &&
			     !pc_test_bit(block_id + blocks_skip, bitmap, fs_info.totalblock);
			     blocks_skip++);

			if (block_id + blocks_skip == blocks_total)
				break;

			if (blocks_skip)
				block_id += blocks_skip;

			/// read chunk from source
			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < buffer_capacity &&
			     pc_test_bit(block_id + blocks_read, bitmap, fs_info.totalblock);
			     ++blocks_read);

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
		if (opt.restore_raw_file && !pc_test_bit(blocks_total - 1, bitmap, fs_info.totalblock)) {
		    if (ftruncate(dfw, (off_t)fs_info.device_size) == -1){
			log_mesg(0, 0, 1, debug, "ftruncate ERROR:%s\n", strerror(errno));
		    }
		    log_mesg(1, 0, 0, debug, "ftruncate:%llu\n", (off_t)fs_info.device_size);
		}

	} else if (opt.domain) {

		int cmp, nx_current = 0;
		unsigned long long next_block_id = 0;
		log_mesg(0, 0, 0, debug, "Total block %i\n", fs_info.totalblock);
		log_mesg(1, 0, 0, debug, "start writing domain log...\n");
		// write domain log comment and status line
		dprintf(dfw, "# Domain logfile created by %s v%s\n", get_exec_name(), VERSION);
		dprintf(dfw, "# Source: %s\n", opt.source);
		dprintf(dfw, "# Offset: 0x%08llX\n", (unsigned long long)opt.offset_domain);
		dprintf(dfw, "# current_pos  current_status\n");
		dprintf(dfw, "0x%08llX     ?\n", opt.offset_domain + (fs_info.totalblock * fs_info.block_size));
		dprintf(dfw, "#      pos        size  status\n");
		// start logging the used/unused areas
		cmp = pc_test_bit(0, bitmap, fs_info.totalblock);
		for (block_id = 0; block_id <= fs_info.totalblock; block_id++) {
			if (block_id < fs_info.totalblock) {
				nx_current = pc_test_bit(block_id, bitmap, fs_info.totalblock);
				if (nx_current)
					copied++;
			} else
				nx_current = -1;
			if (nx_current != cmp) {
				dprintf(dfw, "0x%08llX  0x%08llX  %c\n",
					opt.offset_domain + (next_block_id * fs_info.block_size),
					(block_id - next_block_id) * fs_info.block_size,
					cmp ? '+' : '?');
				next_block_id = block_id;
				cmp = nx_current;
			}
			// don't bother updating progress
		} /// end of for
	} else if (opt.ddd) {

		char *buffer;
		int block_size = fs_info.block_size;
		unsigned long long blocks_total = fs_info.totalblock;
		int blocks_in_buffer = block_size < opt.buffer_size ? opt.buffer_size / block_size : 1;

		// SHA1 for torrent info
		int tinfo = -1;
		torrent_generator torrent;

		buffer = (char*)malloc(blocks_in_buffer * block_size);
		if (buffer == NULL) {
			log_mesg(0, 1, 1, debug, "%s, %i, not enough memory\n", __func__, __LINE__);
		}

		block_id = 0;

		// init SHA1 for torrent info
		if (opt.blockfile == 1) {
			char torrent_name[PATH_MAX + 1] = {'\0'};
			sprintf(torrent_name,"%s/torrent.info", target);
			tinfo = open(torrent_name, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
			torrent_init(&torrent, tinfo);
			dprintf(tinfo, "block_size: %u\n", block_size);
			dprintf(tinfo, "blocks_total: %llu\n", blocks_total);
		}

		log_mesg(0, 0, 0, debug, "Total block %llu\n", blocks_total);

		/// start clone partition to partition
		log_mesg(1, 0, 0, debug, "start backup data device-to-device...\n");
		do {
			/// scan bitmap
			unsigned long long blocks_read;

			/// read chunk from source
			for (blocks_read = 0;
			     block_id + blocks_read < blocks_total && blocks_read < blocks_in_buffer &&
			     pc_test_bit(block_id + blocks_read, bitmap, fs_info.totalblock);
			     blocks_read++);

			if (!blocks_read)
				break;

			r_size = read_all(&dfr, buffer, blocks_read * block_size, &opt);
			if (r_size != (int)(blocks_read * block_size)) {
				if ((r_size == -1) && (errno == EIO)) {
					if (opt.rescue) {
                        assert(buffer != NULL);
						memset(buffer, 0, blocks_read * block_size);
						for (r_size = 0; r_size < blocks_read * block_size; r_size += PART_SECTOR_SIZE)
							rescue_sector(&dfr, r_size, buffer + r_size, &opt);
					} else
						log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);
				} else if (r_size == 0){ // done for ddd
				    /// write buffer to target
                                    if (opt.blockfile == 1){
				        // SHA1 for torrent info
				        // Not always bigger or smaller than 16MB
				        
				        // first we write out block_id * block_size for filename
				        // because when calling write_block_file
				        // we will create a new file to describe a continuous block (or buffer is full)
				        // and never write to same file again
					torrent_start_offset(&torrent, copied * block_size);
					torrent_end_length(&torrent, rescue_write_size);
                                        
					torrent_update(&torrent, buffer, rescue_write_size);

					if (opt.torrent_only == 1) {
						w_size = rescue_write_size;
					} else {
                                        	w_size = write_block_file(target, buffer, rescue_write_size, copied*block_size, &opt);
					}
                                    } else {
                                        w_size = write_all(&dfw, buffer, rescue_write_size, &opt);
                                    }
				    break;
				} else
					log_mesg(0, 1, 1, debug, "source read ERROR %s\n", strerror(errno));
			}

			/// write buffer to target
			if (opt.blockfile == 1){
			    // SHA1 for torrent info
			    // Not always bigger or smaller than 16MB
			    
			    // first we write out block_id * block_size for filename
			    // because when calling write_block_file
			    // we will create a new file to describe a continuous block (or buffer is full)
			    // and never write to same file again
			    torrent_start_offset(&torrent, copied * block_size);
			    torrent_end_length(&torrent, blocks_read * block_size);

			    torrent_update(&torrent, buffer, blocks_read * block_size);

			    if (opt.torrent_only == 1) {
				    w_size = blocks_read * block_size;
			    } else {
			 	w_size = write_block_file(target, buffer, blocks_read * block_size, copied*block_size, &opt);
			    }
			} else {
			    w_size = write_all(&dfw, buffer, blocks_read * block_size, &opt);
			}
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

		// finish SHA1 for torrent info
		if (opt.blockfile == 1) {
			torrent_final(&torrent);
		}

		free(buffer);

		/// restore_raw_file option
		if (opt.restore_raw_file && !pc_test_bit(blocks_total - 1, bitmap, fs_info.totalblock)) {
		    if (ftruncate(dfw, (off_t)fs_info.device_size) == -1){
			log_mesg(0, 0, 1, debug, "ftruncate ERROR:%s\n", strerror(errno));
		    }
		    log_mesg(1, 0, 0, debug, "ftruncate:%llu\n", (off_t)fs_info.device_size);
		}



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
		close_target(dfw);
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
