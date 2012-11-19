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
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>

/**
 * progress.h - only for progress bar
 */
#include "progress.h"

void *thread_update_pui(void *arg);
progress_bar    prog;           /// progress_bar structure defined in progress.h
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
char *EXECNAME="partclone.restore";


/**
 * main functiom - for colne or restore data
 */
int main(int argc, char **argv){ 

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
    unsigned long long	needed_mem  = 0;	/// the copied size 
    char		bitmagic[8] = "BiTmAgIc";// only for check postition
    char		bitmagic_r[8];		/// read magic string from image
    int			cmp;			/// compare magic string
    unsigned long	*bitmap;		/// the point for bitmap data
    int			debug = 0;		/// debug or not
    unsigned long	crc_ck = 0xffffffffL;	/// CRC32 check code for checking
    unsigned long	crc_ck2 = 0xffffffffL;	/// CRC32 check code for checking
    int			c_size;			/// CRC32 code size
    //int			done = 0;
    int			tui = 0;		/// text user interface
    int			pui = 0;		/// progress mode(default text)
    int			raw = 0;
    char		image_hdr_magic[512];
    int			next=1,next_int=1,next_max_count=7,next_count=7;
    unsigned long long	next_block_id;
    char*		cache_buffer;
    int			nx_current=0;
    int	flag;
    pthread_t prog_thread;
    int pres;
    void *p_result;
	
    //progress_bar	prog;			/// progress_bar structure defined in progress.h
    image_head		image_hdr;		/// image_head structure defined in partclone.h

    /**
     * get option and assign to opt structure
     * check parameter and read from argv
     */
    parse_options(argc, argv, &opt);

    /**
     * if "-d / --debug" given
     * open debug file in "/var/log/partclone.log" for log message 
     */
    debug = opt.debug;
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

    next_max_count = opt.max_block_cache-1;
    next_count = opt.max_block_cache-1;

    /// print partclone info
    print_partclone_info(opt);

    if (geteuid() != 0)
	log_mesg(0, 1, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n"); 
    else
	log_mesg(1, 0, 0, debug, "UID is root.\n");

    /// ignore crc check
    if(opt.ignore_crc)
	log_mesg(1, 0, 1, debug, "Ignore CRC error\n");

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
	log_mesg(0, 1, 1, debug, "Erro EXIT.\n");
    }

    dfw = open_target(target, &opt);
    if (dfw == -1) {
	log_mesg(0, 1, 1, debug, "Error Exit.\n");
    }

    /**
     * get partition information like super block, image_head, bitmap
     * from device or image file.
     */

    if (opt.restore){

	log_mesg(1, 0, 0, debug, "restore image hdr - get image_head from image file\n");
	/// get first 512 byte
	r_size = read_all(&dfr, image_hdr_magic, 512, &opt);

	/// check the image magic
	if (memcmp(image_hdr_magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) == 0){
	    restore_image_hdr_sp(&dfr, &opt, &image_hdr, image_hdr_magic);

	    /// check memory size
	    if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
		log_mesg(0, 1, 1, debug, "Ther is no enough free memory, partclone suggests you should have %llu bytes memory\n", needed_mem);

	    /// alloc a memory to restore bitmap
	    bitmap = (unsigned long*)calloc(sizeof(unsigned long), LONGS(image_hdr.totalblock));
	    if(bitmap == NULL){
		log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	    }

	    /// check the file system
	    //if (strcmp(image_hdr.fs, FS) != 0)
	    //    log_mesg(0, 1, 1, debug, "%s can't restore from the image which filesystem is %s not %s\n", argv[0], image_hdr.fs, FS);

	    log_mesg(2, 0, 0, debug, "initial main bitmap pointer %p\n", bitmap);
	    log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

	    /// read and check bitmap from image file
	    log_mesg(0, 0, 1, debug, "Calculating bitmap... ");
	    log_mesg(0, 0, 1, debug, "Please wait... ");
	    get_image_bitmap(&dfr, opt, image_hdr, bitmap);

	    /// check the dest partition size.
	    if (opt.restore_raw_file)
		check_free_space(&dfw, image_hdr.device_size);
	    else if(opt.check)
		check_size(&dfw, image_hdr.device_size);

	    log_mesg(2, 0, 0, debug, "check main bitmap pointer %p\n", bitmap);
	    log_mesg(0, 0, 1, debug, "done!\n");
	}else{
	    log_mesg(1, 0, 0, debug, "This is not partclone image.\n");
	    raw = 1;
	    //sf = lseek(dfr, 0, SEEK_SET);

	    log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");

	    /// get Super Block information from partition
	    if (dfr != 0)
		initial_dd_hdr(dfr, &image_hdr);
	    else
		initial_dd_hdr(dfw, &image_hdr);

	    /// check the dest partition size.
	    if(opt.check){
		check_size(&dfw, image_hdr.device_size);
	    }
	}
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
    stop = (image_hdr.usedblocks+1);	/// get the end of progress number, only used block
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
    if ((opt.restore) && (!raw)) {

	/**
	 * read magic string from image file
	 * and check it.
	 */
	r_size = read_all(&dfr, bitmagic_r, 8, &opt); /// read a magic string
	cmp = memcmp(bitmagic, bitmagic_r, 8);
	if(cmp != 0)
	    log_mesg(0, 1, 1, debug, "bitmagic error %i\n", cmp);

	cache_buffer = (char*)malloc(image_hdr.block_size * (next_max_count+1));
	if(cache_buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}
	buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
	if(buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}

	/// seek to the first
	sf = lseek(dfw, opt.offset, SEEK_SET);
	log_mesg(1, 0, 0, debug, "seek %lli for writing dtat string\n",sf);
	if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %ji\n", (intmax_t)sf);
	/// start restore image file to partition
	for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){

	    r_size = 0;
	    w_size = 0;

	    if (pc_test_bit(block_id, bitmap)){
		/// The block is used
		log_mesg(2, 0, 0, debug, "block_id=%llu, ",block_id);
		log_mesg(1, 0, 0, debug, "bitmap=%i, ",pc_test_bit(block_id, bitmap));

		memset(buffer, 0, image_hdr.block_size);
		r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
		log_mesg(1, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
		if (r_size <0)
		    log_mesg(0, 1, 1, debug, "read errno = %i \n", errno);

		/// read crc32 code and check it.
		crc_ck = crc32(crc_ck, buffer, r_size);
		char crc_buffer[CRC_SIZE];
		c_size = read_all(&dfr, crc_buffer, CRC_SIZE, &opt);
		if (c_size < CRC_SIZE)
		    log_mesg(0, 1, 1, debug, "read CRC error: %s, please check your image file. \n", strerror(errno));
		/*FIX: 64bit image can't ignore crc error*/
		if ((memcmp(crc_buffer, &crc_ck, CRC_SIZE) != 0) && (!opt.ignore_crc) ){
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
		    if ((memcmp(crc_buffer, &crc_ck2, CRC_SIZE) != 0) && (!opt.ignore_crc)){
			log_mesg(0, 1, 1, debug, "CRC error again at %ji...\n ", (intmax_t)sf);
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
		    offset = (off_t)(((block_id-next+1) * image_hdr.block_size)+opt.offset);
		    sf = lseek(dfw, offset, SEEK_SET);
		    if (sf == -1)
			log_mesg(0, 1, 1, debug, "target seek error = %ji, ", (intmax_t)sf);
#endif
		    /// write block from buffer to partition
		    w_size = write_all(&dfw, cache_buffer, (image_hdr.block_size*nx_current), &opt);
		    log_mesg(1, 0, 0, debug, "bs=%i and w=%i, ",(image_hdr.block_size*nx_current), w_size);
		    if (w_size != image_hdr.block_size*nx_current)
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
		    log_mesg(2, 0, 0, debug, "block_id=%llu, ",block_id);
		    sf = lseek(dfw, image_hdr.block_size, SEEK_CUR);
		    log_mesg(2, 0, 0, debug, "seek=%ji, ", (intmax_t)sf);
		    if (sf == (off_t)-1)
			log_mesg(0, 1, 1, debug, "seek error %ji errno=%i\n", (intmax_t)offset, errno);
		    log_mesg(2, 0, 0, debug, "end\n");
#endif
		}
	    }
	    //if (!opt.quiet)
	//	update_pui(&prog, copied, block_id, done);
	} // end of for
	/// free buffer
	free(cache_buffer);
	free(buffer);
	done = 1;
	pres = pthread_join(prog_thread, &p_result);
	update_pui(&prog, copied, block_id, done);
	sync_data(dfw, &opt);	
    } else if ((opt.restore) && (raw)){
	/// start clone partition to image file

	//write image_head to image file
	w_size = write_all(&dfw, image_hdr_magic, 512, &opt);
	if(w_size == -1)
	    log_mesg(0, 1, 1, debug, "write image_hdr to image error\n");

	block_id = 1;

	buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
	if(buffer == NULL){
	    log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}

	do {
	    r_size = 0;
	    w_size = 0;
	    memset(buffer, 0, image_hdr.block_size);

	    log_mesg(1, 0, 0, debug, "block_id=%llu, ",block_id);

	    /// read data from source to buffer
	    r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
	    log_mesg(1, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
	    if (r_size != (int)image_hdr.block_size){
		if (r_size == 0){
		    //EIO
		    done = 1;
		} else {
		    log_mesg(0, 1, 1, debug, "read error: %s(%i) rsize=%i\n", strerror(errno), errno, r_size);
		}
	    }

	    if (r_size == image_hdr.block_size){
		/// write buffer to target
		w_size = write_all(&dfw, buffer, image_hdr.block_size, &opt);
		log_mesg(2, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
		if (w_size != (int)image_hdr.block_size)
		    log_mesg(0, 1, 1, debug, "write error %i \n", w_size);
	    } else if (r_size < image_hdr.block_size){
		/// write readed buffer to target
		w_size = write_all(&dfw, buffer, r_size, &opt);
		log_mesg(2, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
		if (w_size != r_size)
		    log_mesg(0, 1, 1, debug, "write error %i \n", w_size);
	    } else {
		w_size = 0;
	    }

	    /// read or write error
	    if (r_size != w_size)
		log_mesg(0, 1, 1, debug, "read and write different\n");
	    log_mesg(1, 0, 0, debug, "end\n");
	    block_id++;
	    copied++;					/// count copied block
	    total_write += (unsigned long long)(w_size);	/// count copied size
	    log_mesg(1, 0, 0, debug, "total=%llu, ", total_write);

	    //if (!opt.quiet)
		//update_pui(&prog, copied, block_id, done);
	} while (done == 0);/// end of for    
	pres = pthread_join(prog_thread, &p_result);
	update_pui(&prog, copied, block_id, done);
	sync_data(dfw, &opt);	
	/// free buffer
	free(buffer);
    }

    print_finish_info(opt);

    close (dfr);    /// close source
    close (dfw);    /// close target
    free(bitmap);   /// free bitmp
    close_pui(pui);
    fprintf(stderr, "Cloned successfully.\n");
    if(opt.debug)
	close_log();
    return 0;	    /// finish
}

void *thread_update_pui(void *arg){

    while (done == 0) {
	if(!opt.quiet) {
	    update_pui(&prog, copied, block_id, done);
	}
	sleep(opt.fresh);
    }
    pthread_exit("exit");
}

