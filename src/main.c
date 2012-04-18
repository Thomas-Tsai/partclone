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
progress_bar	prog;		/// progress_bar structure defined in progress.h
unsigned long long copied;
unsigned long long block_id;
int done;


/**
 * partclone.h - include some structures like image_head, opt_cmd, ....
 *               and functions for main used.
 */
#include "partclone.h"

/// global variable
cmd_opt		opt;			/// cmd_opt structure defined in partclone.h

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
#endif

/// fs option
#include "fs_common.h"
fs_cmd_opt		fs_opt;			/// cmd_opt structure defined in partclone.h

/**
 * main functiom - for colne or restore data
 */
int main(int argc, char **argv){ 
#ifdef MEMTRACE
    setenv("MALLOC_TRACE", "partclone_mtrace.log", 1);
    mtrace();
#endif
    char*		source;			/// source data
    char*		target;			/// target data
    char*		buffer;			/// buffer data for malloc used
    char*		buffer2;			/// buffer data for malloc used
    int			dfr, dfw;		/// file descriptor for source and target
    int			r_size, w_size;		/// read and write size
    //unsigned long long	block_id, copied = 0;	/// block_id is every block in partition
    /// copied is copied block count
    off_t		offset = 0, sf = 0;	/// seek postition, lseek result
    int			start, stop;		/// start, range, stop number for progress bar
    unsigned long long	total_write = 0;	/// the copied size 
    unsigned long long	needed_size = 0;	/// the copied size 
    unsigned long long	needed_mem  = 0;	/// the copied size 
    char		bitmagic[8] = "BiTmAgIc";// only for check postition
    char		bitmagic_r[8]="00000000";/// read magic string from image
    int			cmp;			/// compare magic string
    unsigned long	*bitmap;		/// the point for bitmap data
    int			debug = 0;		/// debug or not
    unsigned long	crc = 0xffffffffL;	/// CRC32 check code for writint to image
    unsigned long	crc_ck = 0xffffffffL;	/// CRC32 check code for checking
    unsigned long	crc_ck2 = 0xffffffffL;	/// CRC32 check code for checking
    int			c_size;			/// CRC32 code size
    int			n_crc_size = CRC_SIZE;
    //int			done = 0;
    int			s_count = 0;
    int			rescue_num = 0;
    unsigned long long			rescue_pos = 0;
    unsigned long long			main_pos = 0;
    int			tui = 0;		/// text user interface
    int			pui = 0;		/// progress mode(default text)
    int                 next=1,next_int=1,next_max_count=7,next_count=7,i;
    unsigned long long  next_block_id;
    char*               cache_buffer;
    int                 nx_current=0;
    char                bbuffer[4096];
    int flag;
    int pres;
    pthread_t prog_thread;
    void *p_result;

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

    next_max_count = opt.max_block_cache-1;
    next_count = opt.max_block_cache-1;

    //if(opt.debug)
    open_log(opt.logfile);

    /**
     * using Text User Interface
     */
    if (opt.ncurses){
        pui = NCURSES;
        log_mesg(1, 0, 0, debug, "Using Ncurses User Interface mode.\n");
    } else
        pui = TEXT;

    tui = open_pui(pui, opt.fresh);
    if ((opt.ncurses) && (tui == 0)){
        opt.ncurses = 0;
        log_mesg(1, 0, 0, debug, "Open Ncurses User Interface Error.\n");
    }

    /// print partclone info
    print_partclone_info(opt);

    if (geteuid() != 0)
        log_mesg(0, 1, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n"); 
    else
        log_mesg(1, 0, 0, debug, "UID is root.\n");

    /// ignore crc check
    if(opt.ignore_crc)
	log_mesg(1, 0, 1, debug, "Ignore CRC errors\n");

    /**
     * open source and target 
     * clone mode, source is device and target is image file/stdout
     * restore mode, source is image file/stdin and target is device
     * dd mode, source is device and target is device !!not complete
     */
#ifdef _FILE_OFFSET_BITS
    log_mesg(1, 0, 0, debug, "enable _FILE_OFFSET_BITS %i\n", _FILE_OFFSET_BITS);
#endif
    source = opt.source;
    target = opt.target;
    dfr = open_source(source, &opt);
    if (dfr == -1) {
        log_mesg(0, 1, 1, debug, "Error exit\n");
    }

    dfw = open_target(target, &opt);
    if (dfw == -1) {
        log_mesg(0, 1, 1, debug, "Error exit\n");
    }

    /**
     * get partition information like super block, image_head, bitmap
     * from device or image file.
     */
    if (opt.clone){

        log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
        log_mesg(0, 0, 1, debug, "Reading Super Block\n");

        /// get Super Block information from partition
        initial_image_hdr(source, &image_hdr);

        /// check memory size
        if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
            log_mesg(0, 1, 1, debug, "There is not enough free memory, partclone suggests you should have %lld bytes memory\n", needed_mem);

	strncpy(image_hdr.version, IMAGE_VERSION, VERSION_SIZE);

        /// alloc a memory to restore bitmap
        bitmap = (unsigned long*)calloc(sizeof(unsigned long), LONGS(image_hdr.totalblock));
        if(bitmap == NULL){
            log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
        }

        log_mesg(2, 0, 0, debug, "initial main bitmap pointer %i\n", bitmap);
        log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

        /// read and check bitmap from partition
        log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
        readbitmap(source, image_hdr, bitmap, pui);

        needed_size = (unsigned long long)(((image_hdr.block_size+sizeof(unsigned long))*image_hdr.usedblocks)+sizeof(image_hdr)+sizeof(char)*image_hdr.totalblock);
        if (opt.check)
            check_free_space(&dfw, needed_size);

        log_mesg(2, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);

        log_mesg(1, 0, 0, debug, "Writing super block and bitmap... ");
        // write image_head to image file
        w_size = write_all(&dfw, (char *)&image_hdr, sizeof(image_head), &opt);
        if(w_size == -1)
            log_mesg(0, 1, 1, debug, "write image_hdr to image error\n");

        // write bitmap information to image file
        for (i = 0; i < image_hdr.totalblock; i++){
            if (pc_test_bit(i, bitmap)){
                bbuffer[i % sizeof(bbuffer)] = 1;
            } else {
                bbuffer[i % sizeof(bbuffer)] = 0;
            }
            if (i % sizeof(bbuffer) == sizeof(bbuffer) - 1 || i == image_hdr.totalblock - 1) {
                w_size = write_all(&dfw, bbuffer, 1 + (i % sizeof(bbuffer)), &opt);
                if(w_size == -1)
                    log_mesg(0, 1, 1, debug, "write bitmap to image error\n");
            }
        }
        log_mesg(0, 0, 1, debug, "done!\n");
    } else if (opt.restore){

        log_mesg(1, 0, 0, debug, "restore image hdr - get image_head from image file\n");
        log_mesg(1, 0, 1, debug, "Reading Super Block\n");
        /// get image information from image file
        restore_image_hdr(&dfr, &opt, &image_hdr);

        /// check memory size
        if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
            log_mesg(0, 1, 1, debug, "Ther is no enough free memory, partclone suggests you should have %lld bytes memory\n", needed_mem);

        /// alloc a memory to restore bitmap
        bitmap = (unsigned long*)calloc(sizeof(unsigned long), LONGS(image_hdr.totalblock));
        if(bitmap == NULL){
            log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
        }

        /// check the image magic
        if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0)
            log_mesg(0, 1, 1, debug, "This is not partclone image.\n");

        /// check the file system
        //if (strcmp(image_hdr.fs, FS) != 0)
        //    log_mesg(0, 1, 1, debug, "%s can't restore from the image which filesystem is %s not %s\n", argv[0], image_hdr.fs, FS);

        log_mesg(2, 0, 0, debug, "initial main bitmap pointer %lli\n", bitmap);
        log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

        /// read and check bitmap from image file
        log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
        get_image_bitmap(&dfr, opt, image_hdr, bitmap);

        /// check the dest partition size.
        if (opt.restore_raw_file)
	    check_free_space(&dfw, image_hdr.device_size);
        else if(opt.check)
            check_size(&dfw, image_hdr.device_size);

        log_mesg(2, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
        log_mesg(0, 0, 1, debug, "done!\n");
    } else if (opt.dd){
        log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
        log_mesg(1, 0, 1, debug, "Reading Super Block\n");

        /// get Super Block information from partition
        initial_image_hdr(source, &image_hdr);

        /// check memory size
        if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
            log_mesg(0, 1, 1, debug, "Ther is no enough free memory, partclone suggests you should have %lld bytes memory\n", needed_mem);

	strncpy(image_hdr.version, IMAGE_VERSION, VERSION_SIZE);

        /// alloc a memory to restore bitmap
        bitmap = (unsigned long*)calloc(sizeof(unsigned long), LONGS(image_hdr.totalblock));
        if(bitmap == NULL){
            log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
        }

        log_mesg(2, 0, 0, debug, "initial main bitmap pointer %i\n", bitmap);
        log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

        /// read and check bitmap from partition
        log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
        readbitmap(source, image_hdr, bitmap, pui);

        /// check the dest partition size.
        if(opt.check){
            check_size(&dfw, image_hdr.device_size);
        }

        log_mesg(2, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
        log_mesg(0, 0, 1, debug, "done!\n");
    } else if (opt.domain) {
        log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");
        log_mesg(1, 0, 1, debug, "Reading Super Block\n");

        /// get Super Block information from partition
        initial_image_hdr(source, &image_hdr);

        /// check memory size
        if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
            log_mesg(0, 1, 1, debug, "Ther is no enough free memory, partclone suggests you should have %lld bytes memory\n", needed_mem);

	strncpy(image_hdr.version, IMAGE_VERSION, VERSION_SIZE);

        /// alloc a memory to restore bitmap
        bitmap = (unsigned long*)calloc(sizeof(unsigned long), LONGS(image_hdr.totalblock));
        if(bitmap == NULL){
            log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
        }

        log_mesg(2, 0, 0, debug, "initial main bitmap pointer %i\n", bitmap);
        log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

        /// read and check bitmap from partition
        log_mesg(0, 0, 1, debug, "Calculating bitmap... Please wait... ");
        readbitmap(source, image_hdr, bitmap, pui);

        log_mesg(2, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
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
    //progress_bar	prog;		/// progress_bar structure defined in progress.h
    start = 0;				/// start number of progress bar
    stop = (image_hdr.usedblocks);	/// get the end of progress number, only used block
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

    /**
     * start read and write data between device and image file
     */
    if (opt.clone) {

        w_size = write_all(&dfw, bitmagic, 8, &opt); /// write a magic string

        /// read data from the first block and log the offset
        sf = lseek(dfr, 0, SEEK_SET);
        log_mesg(1, 0, 0, debug, "seek %lli for reading data string\n",sf);
        if (sf == (off_t)-1)
            log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);

        buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
        if(buffer == NULL){
            log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
        }

        log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);


        /// start clone partition to image file
        log_mesg(1, 0, 0, debug, "start backup data...\n");
        for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){

            r_size = 0;
            w_size = 0;

	    main_pos = lseek(dfr, 0, SEEK_CUR);
	    log_mesg(3, 0, 0, debug, "man pos = %lli\n", main_pos);

            if (pc_test_bit(block_id, bitmap)){
                /// if the block is used
                log_mesg(1, 0, 0, debug, "block_id=%lli, ",block_id);
                log_mesg(2, 0, 0, debug, "bitmap=%i, ",pc_test_bit(block_id, bitmap));

                offset = (off_t)(block_id * image_hdr.block_size);
#ifdef _FILE_OFFSET_BITS
                sf = lseek(dfr, offset, SEEK_SET);
                if (sf == -1)
                    log_mesg(0, 1, 1, debug, "source seek error = %lli, ",sf);
#endif
                /// read data from source to buffer
                memset(buffer, 0, image_hdr.block_size);
                rescue_pos = lseek(dfr, 0, SEEK_CUR);
                r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
                log_mesg(3, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
                if (r_size != (int)image_hdr.block_size){

                    if ((r_size == -1) && (errno == EIO)){
                        if (opt.rescue){
                            r_size = 0;
                            for (rescue_num = 0; rescue_num < image_hdr.block_size; rescue_num += SECTOR_SIZE){
                                rescue_sector(&dfr, rescue_pos + rescue_num, buffer + rescue_num, &opt);
                                r_size+=SECTOR_SIZE;
                            }
                        }else
                            log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);

                    }else
                        log_mesg(0, 1, 1, debug, "read error: %s(%i) \n", strerror(errno), errno);
                }

                /// write buffer to target
                w_size = write_all(&dfw, buffer, image_hdr.block_size, &opt);
                log_mesg(3, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
                if (w_size != (int)image_hdr.block_size)
                    log_mesg(0, 1, 1, debug, "write error %i \n", w_size);

                /// generate crc32 code and write it.
                crc = crc32(crc, buffer, w_size);
                char crc_buffer[CRC_SIZE];
                memcpy(crc_buffer, &crc, CRC_SIZE);
                c_size = write_all(&dfw, crc_buffer, CRC_SIZE, &opt);

                copied++;					/// count copied block
                total_write += (unsigned long long)(w_size);	/// count copied size
                log_mesg(3, 0, 0, debug, "total=%lli, ", total_write);

                /// read or write error
                if (r_size != w_size)
                    log_mesg(0, 1, 1, debug, "read(%i) and write(%i) different\n", r_size, w_size);
                log_mesg(2, 0, 0, debug, "end\n");
            } else {
#ifndef _FILE_OFFSET_BITS
                /// if the block is not used, I just skip it.
                log_mesg(2, 0, 0, debug, "block_id=%lli, ",block_id);
                sf = lseek(dfr, image_hdr.block_size, SEEK_CUR);
                log_mesg(2, 0, 0, debug, "skip seek=%lli, ",sf);
                if (sf == (off_t)-1)
                    log_mesg(0, 1, 1, debug, "clone seek error %lli errno=%i\n", (long long)offset, (int)errno);

                log_mesg(2, 0, 0, debug, "end\n");
#endif
            }
        } /// end of for    
	free(buffer);
    } else if (opt.restore) {

        /**
         * read magic string from image file
         * and check it.
         */
        r_size = read_all(&dfr, bitmagic_r, 8, &opt); /// read a magic string
        cmp = memcmp(bitmagic, bitmagic_r, 8);
        if(cmp != 0)
            log_mesg(0, 1, 1, debug, "bitmagic error %i\n", cmp);

        /// seek to the first
        sf = lseek(dfw, 0, SEEK_SET);
        log_mesg(1, 0, 0, debug, "seek %lli for writing data string\n",sf);
        if (sf == (off_t)-1)
            log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);

	cache_buffer = (char*)malloc(image_hdr.block_size * (next_max_count+1));
	if(cache_buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}

	buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
	if(buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}

        /// start restore image file to partition
        log_mesg(1, 0, 0, debug, "start restore data...\n");

        for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){

            r_size = 0;
            w_size = 0;


            if (pc_test_bit(block_id, bitmap)){ 
                /// The block is used
                log_mesg(1, 0, 0, debug, "block_id=%lli, ",block_id);
                log_mesg(2, 0, 0, debug, "bitmap=%i, ",pc_test_bit(block_id, bitmap));

                memset(buffer, 0, image_hdr.block_size);
                r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
                log_mesg(3, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
                if (r_size <0)
                    log_mesg(0, 1, 1, debug, "read errno = %i \n", errno);

                /// read crc32 code and check it.
                crc_ck = crc32(crc_ck, buffer, r_size);
                char crc_buffer[CRC_SIZE];
                c_size = read_all(&dfr, crc_buffer, CRC_SIZE, &opt);
                if (c_size < CRC_SIZE)
                    log_mesg(0, 1, 1, debug, "read CRC error: %s, please check your image file. \n", strerror(errno));

		/*FIX: 64bit image can't ignore crc error*/
                if ((memcmp(crc_buffer, &crc_ck, CRC_SIZE) != 0) && (!opt.ignore_crc)){
                    log_mesg(1, 0, 0, debug, "CRC Check error. 64bit bug before v0.1.0 (Rev:250M), enlarge crc size and recheck again....\n ");
                    /// check again
                    buffer2 = (char*)malloc(image_hdr.block_size+CRC_SIZE); ///alloc a memory to copy data
                    if(buffer2 == NULL){
                        log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
                    }
                    memcpy(buffer2, buffer, image_hdr.block_size);
                    memcpy(buffer2+image_hdr.block_size, crc_buffer, CRC_SIZE);
                    memcpy(buffer, buffer2+CRC_SIZE, image_hdr.block_size);

                    crc_ck2 = crc32(crc_ck2, buffer, r_size);
                    c_size = read_all(&dfr, crc_buffer, CRC_SIZE, &opt);
                    if (c_size < CRC_SIZE)
                        log_mesg(0, 1, 1, debug, "read CRC error: %s, please check your image file. \n", strerror(errno));
                    if ((memcmp(crc_buffer, &crc_ck2, CRC_SIZE) != 0 )&& (!opt.ignore_crc)) {
                        log_mesg(0, 1, 1, debug, "CRC error again at %i...\n ", sf);
                    } else {
                        crc_ck = crc_ck2;
                    }
                    free(buffer2);
                } else {
                    crc_ck2 = crc_ck;
                }

		if(next != next_count){
		    memset(cache_buffer, 0, image_hdr.block_size*next_max_count);
		    for (next_int = 1; next_int <= next_max_count; next_int++)
		    {
			next_block_id = block_id+next_int;
			if (pc_test_bit(next_block_id, bitmap)) {
			    next++;
			} else {
			    next_count = next;
			    break;
			}
			next_count = next;
		    }
		    log_mesg(1, 0, 0, debug, "next = %i\n",next);
		}

		if ((next == next_count) &&(nx_current < next)){
		    memcpy(cache_buffer+(image_hdr.block_size*nx_current), buffer, image_hdr.block_size);
		    w_size = 0;
		    nx_current++;
		}

		if ((next == next_count) && (nx_current == next)){
#ifdef _FILE_OFFSET_BITS
		    offset = (off_t)((block_id-next+1) * image_hdr.block_size);
		    sf = lseek(dfw, offset, SEEK_SET);
		    if (sf == -1)
			log_mesg(0, 1, 1, debug, "target seek error = %lli, ",sf);
#endif
		    /// write block from buffer to partition
		    w_size = write_all(&dfw, cache_buffer, (image_hdr.block_size*nx_current), &opt);
		    log_mesg(1, 0, 0, debug, "bs=%i and w=%i, ",(image_hdr.block_size*nx_current), w_size);
		    if (w_size != (int)image_hdr.block_size*nx_current)
			log_mesg(0, 1, 1, debug, "write error %i \n", w_size);
		    next = 1;
		    next_count = next_max_count;
		    nx_current=0;
		}

                copied++;					/// count copied block
                total_write += (unsigned long long) w_size;	/// count copied size

                /// read or write error
                //if ((r_size != w_size) || (r_size != image_hdr.block_size))
                //	log_mesg(0, 1, 1, debug, "read and write different\n");
                log_mesg(1, 0, 0, debug, "end\n");
            } else {
		/// for restore to raw file, mount -o loop used.
		if ((block_id == (image_hdr.totalblock-1)) && (opt.restore_raw_file)){
		    write_last_block(&dfw, image_hdr.block_size, block_id, &opt);

		} else {
#ifndef _FILE_OFFSET_BITS
		    /// if the block is not used, I just skip it.
		    log_mesg(2, 0, 0, debug, "block_id=%lli, ",block_id);
		    sf = lseek(dfw, image_hdr.block_size, SEEK_CUR);
		    log_mesg(2, 0, 0, debug, "seek=%lli, ",sf);
		    if (sf == (off_t)-1)
			log_mesg(0, 1, 1, debug, "seek error %lli errno=%i\n", (long long)offset, (int)errno);
		    log_mesg(2, 0, 0, debug, "end\n");
#endif
		}
            }
        } // end of for
	free(buffer);
    } else if (opt.dd){
        sf = lseek(dfr, 0, SEEK_SET);
        log_mesg(1, 0, 0, debug, "seek %lli for reading data string\n",sf);
        if (sf == (off_t)-1)
            log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);

	main_pos = lseek(dfr, 0, SEEK_CUR);
	log_mesg(1, 0, 0, debug, "man pos = %lli\n", main_pos);

        log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);

	buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
	if(buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}
 
        /// start clone partition to image file
        log_mesg(1, 0, 0, debug, "start backup data device-to-device...\n");
        for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){
            r_size = 0;
            w_size = 0;

            if (pc_test_bit(block_id, bitmap)){
                /// if the block is used

                log_mesg(1, 0, 0, debug, "block_id=%lli, ",block_id);
                log_mesg(2, 0, 0, debug, "bitmap=%i, ",pc_test_bit(block_id, bitmap));
                offset = (off_t)(block_id * image_hdr.block_size);
#ifdef _FILE_OFFSET_BITS
                sf = lseek(dfr, offset, SEEK_SET);
                if (sf == -1)
                    log_mesg(0, 1, 1, debug, "source seek error = %lli, ",sf);
                sf = lseek(dfw, offset, SEEK_SET);
                if (sf == -1)
                    log_mesg(0, 1, 1, debug, "target seek error = %lli, ",sf);
#endif
               /// read data from source to buffer
                memset(buffer, 0, image_hdr.block_size);
                rescue_pos = lseek(dfr, 0, SEEK_CUR);
                r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
                log_mesg(3, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
                if (r_size != (int)image_hdr.block_size){
                    if ((r_size == -1) && (errno == EIO)){
                        if (opt.rescue){
                            r_size = 0;
                            for (rescue_num = 0; rescue_num < image_hdr.block_size; rescue_num += SECTOR_SIZE){
                                rescue_sector(&dfr, rescue_pos + rescue_num, buffer + rescue_num, &opt);
				r_size+=SECTOR_SIZE;
			      }
                        }else
                            log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);

                    }else
                        log_mesg(0, 1, 1, debug, "read error: %s(%i) \n", strerror(errno), errno);
                }

                /// write buffer to target
                w_size = write_all(&dfw, buffer, image_hdr.block_size, &opt);
                log_mesg(3, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
                if (w_size != (int)image_hdr.block_size)
                    log_mesg(0, 1, 1, debug, "write error %i \n", w_size);

                copied++;                                       /// count copied block
                total_write += (unsigned long long)(w_size);    /// count copied size
                log_mesg(2, 0, 0, debug, "total=%lli, ", total_write);
                /// read or write error
                if (r_size != w_size)
                    log_mesg(0, 1, 1, debug, "read and write different\n");
                log_mesg(1, 0, 0, debug, "end\n");
            } else {
#ifndef _FILE_OFFSET_BITS
                /// if the block is not used, I just skip it.
                log_mesg(2, 0, 0, debug, "block_id=%lli, ",block_id);
                sf = lseek(dfr, image_hdr.block_size, SEEK_CUR);
                log_mesg(2, 0, 0, debug, "skip source seek=%lli, ",sf);
                sf = lseek(dfw, image_hdr.block_size, SEEK_CUR);
                log_mesg(2, 0, 0, debug, "skip target seek=%lli, ",sf);
                if (sf == (off_t)-1)
                    log_mesg(0, 1, 1, debug, "clone seek error %lli errno=%i\n", (long long)offset, (int)errno);
#endif
            }
        } /// end of for
	free(buffer);
    } else if (opt.domain) {
        log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);
        log_mesg(1, 0, 0, debug, "start writing domain log...\n");
        // write domain log comment and status line
        dprintf(dfw, "# Domain logfile created by %s v%s\n", EXECNAME, VERSION);
        dprintf(dfw, "# Source: %s\n", opt.source);
        dprintf(dfw, "# Offset: 0x%08llX\n", opt.offset_domain);
        dprintf(dfw, "# current_pos  current_status\n");
        dprintf(dfw, "0x%08llX     ?\n",
                opt.offset_domain + (image_hdr.totalblock * image_hdr.block_size));
        dprintf(dfw, "#      pos        size  status\n");
        // start logging the used/unused areas
        next_block_id = 0;
        cmp = pc_test_bit(0, bitmap);
        for( block_id = 0; block_id <= image_hdr.totalblock; block_id++ ){
            if (block_id < image_hdr.totalblock){
                nx_current = pc_test_bit(block_id, bitmap);
                if (nx_current == 1)
                    copied++;
            } else
                nx_current = -1;
            if (nx_current != cmp){
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
    update_pui(&prog, copied, block_id, done);
    sync_data(dfw, &opt);	
    print_finish_info(opt);

    close (dfr);    /// close source
    close (dfw);    /// close target
    free(bitmap);   /// free bitmp
    close_pui(pui);
    printf("Cloned successfully.\n");
    if(opt.debug)
        close_log();
#ifdef MEMTRACE
    muntrace();
#endif
    return 0;	    /// finish
}

void *thread_update_pui(void *arg){

    while (done == 0) {
	if(!opt.quiet){
	    update_pui(&prog, copied, block_id, done);
	    sleep(opt.fresh);
	}
    }
    pthread_exit("exit");
}
