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


/**
 * progress.h - only for progress bar
 */
#include "progress.h"

/**
 * partclone.h - include some structures like image_head, opt_cmd, ....
 *               and functions for main used.
 */
#include "partclone.h"

/**
 * Include different filesystem header depend on what flag you want.
 * If cflag is _EXTFS, output to extfsclone.
 * My goal is give different clone utils by makefile .
 */
#ifdef EXTFS
    #include "extfsclone.h"
#elif REISERFS
    #include "reiserfsclone.h"
#elif REISER4
    #include "reiser4clone.h"
#elif XFS
    #include "xfsclone.h"
#elif HFSPLUS
    #include "hfsplusclone.h"
#endif

/**
 * main functiom - for colne or restore data
 */
int main(int argc, char **argv){ 

    char*		source;			/// source data
    char*		target;			/// target data
    char*		buffer;			/// buffer data for malloc used
    int			dfr, dfw;		/// file descriptor for source and target
    int			r_size, w_size;		/// read and write size
    unsigned long long	block_id, copied = 0;	/// block_id is every block in partition
						/// copied is copied block count
    off_t		offset = 0, sf = 0;	/// seek postition, lseek result
    int			start, res, stop;	/// start, range, stop number for progress bar
    unsigned long long	total_write = 0;	/// the copied size 
    char		bitmagic[8] = "BiTmAgIc";// only for check postition
    char		bitmagic_r[8];		/// read magic string from image
    int			cmp;			/// compare magic string
    char		*bitmap;		/// the point for bitmap data
    int			debug;			/// debug or not

    progress_bar	prog;			/// progress_bar structure defined in progress.h
    cmd_opt		opt;			/// cmd_opt structure defined in partclone.h
    image_head		image_hdr;		/// image_head structure defined in partclone.h

    /**
     * get option and assign to opt structure
     * check parameter and read from argv
     */
    parse_options(argc, argv, &opt);

    /**
     * open source and target 
     * clone mode, source is device and target is image file/stdout
     * restore mode, source is image file/stdin and target is device
     * dd mode, source is device and target is device !!not complete
     */
    source = opt.source;
    target = opt.target;
    dfr = open_source(source, &opt);
    dfw = open_target(target, &opt);

    /**
     * if "-d / --debug" given
     * open debug file in "/var/log/partclone.log" for log message 
     */
    debug = opt.debug;
    //if(opt.debug)
	open_log();

    /**
     * get partition information like super block, image_head, bitmap
     * from device or image file.
     */
    if (opt.clone){

	log_mesg(0, 0, 0, debug, "Initial image hdr: get Super Block from partition\n");
	
	/// get Super Block information from partition
        initial_image_hdr(source, &image_hdr);

	/// alloc a memory to restore bitmap
	bitmap = (char*)malloc(sizeof(char)*image_hdr.totalblock);
	
	log_mesg(0, 0, 0, debug, "initial main bitmap pointer %i\n", bitmap);
	log_mesg(0, 0, 0, debug, "Initial image hdr: read bitmap table\n");

	/// read and check bitmap from partition
	readbitmap(source, image_hdr, bitmap);

	log_mesg(0, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);

	/*
	sf = lseek(dfw, 0, SEEK_SET);
	log_mesg(0, 0, 0, debug, "seek %lli for writing image_head\n",sf);
        if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);
	*/

	// write image_head to image file
	w_size = write_all(&dfw, (char *)&image_hdr, sizeof(image_head));
	if(w_size == -1)
	   log_mesg(0, 1, 1, debug, "write image_hdr to image error\n");

	// write bitmap information to image file
    	w_size = write_all(&dfw, bitmap, sizeof(char)*image_hdr.totalblock);
	if(w_size == -1)
	    log_mesg(0, 1, 1, debug, "write bitmap to image error\n");

    } else if (opt.restore){

	log_mesg(0, 0, 0, debug, "restore image hdr: get image_head from image file\n");
        /// get image information from image file
	restore_image_hdr(&dfr, &opt, &image_hdr);

	/// alloc a memory to restore bitmap
	bitmap = (char*)malloc(sizeof(char)*image_hdr.totalblock);

	log_mesg(0, 0, 0, debug, "initial main bitmap pointer %lli\n", bitmap);
	log_mesg(0, 0, 0, debug, "Initial image hdr: read bitmap table\n");

	/// read and check bitmap from image file
	get_image_bitmap(&dfr, opt, image_hdr, bitmap);

	/// check the dest partition size.
	check_size(&dfw, image_hdr.device_size);

	log_mesg(0, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
    }

    log_mesg(0, 0, 0, debug, "print image_head\n");

    /// print image_head
    print_image_hdr_info(image_hdr, opt);
	
    /**
     * initial progress bar
     */
    start = 0;				/// start number of progress bar
    stop = (int)image_hdr.usedblocks;	/// get the end of progress number, only used block
    res = 100;				/// the end of progress number
    log_mesg(0, 0, 0, debug, "Initial Progress bar\n");
    /// Initial progress bar
    progress_init(&prog, start, stop, res);
    copied = 1;				/// initial number is 1

    /**
     * start read and write data between device and image file
     */
    if (opt.clone) {

	w_size = write_all(&dfw, bitmagic, 8); /// write a magic string

	/*
	/// log the offset
        sf = lseek(dfw, 0, SEEK_CUR);
	log_mesg(0, 0, 0, debug, "seek %lli for writing data string\n",sf); 
	if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);
	*/

	/// read data from the first block and log the offset
	sf = lseek(dfr, 0, SEEK_SET);
	log_mesg(0, 0, 0, debug, "seek %lli for reading data string\n",sf);
        if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);

	log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);
//        log_mesg(0, 0, 0, debug, "blockid,\tbitmap,\tread,\twrite,\tsize,\tseek,\tcopied,\terror\n");

	/// start clone partition to image file
        for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){
	    
	    r_size = 0;
	    w_size = 0;
	    log_mesg(0, 0, 0, debug, "block_id=%lli, ",block_id);

	    if (bitmap[block_id] == 1){
		/// if the block is used

		log_mesg(0, 0, 0, debug, "bitmap=%i, ",bitmap[block_id]);

		progress_update(&prog, copied);
        	
		offset = (off_t)(block_id * image_hdr.block_size);
		//sf = lseek(dfr, offset, SEEK_SET);
                //if (sf == (off_t)-1)
                //    log_mesg(0, 1, 1, debug, "seek error %lli errno=%i\n", (long long)offset, (int)errno);
        	buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
        	
		/// read data from source to buffer
		r_size = read_all(&dfr, buffer, image_hdr.block_size);
		log_mesg(0, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
		if (r_size != (int)image_hdr.block_size)
		    log_mesg(0, 1, 1, debug, "read error %i \n", r_size);
        	
		/// write buffer to target
		w_size = write_all(&dfw, buffer, image_hdr.block_size);
		log_mesg(0, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
		if (w_size != (int)image_hdr.block_size)
		    log_mesg(0, 1, 1, debug, "write error %i \n", w_size);
        	
		/// free buffer
		free(buffer);

		copied++;					/// count copied block
		total_write += (unsigned long long)(w_size);	/// count copied size
		log_mesg(0, 0, 0, debug, "total=%lli, ", total_write);
		
		/// read or write error
		if (r_size != w_size)
		    log_mesg(0, 1, 1, debug, "read and write different\n");
            } else {
		/// if the block is not used, I just skip it.
        	sf = lseek(dfr, image_hdr.block_size, SEEK_CUR);
		log_mesg(0, 0, 0, debug, "skip seek=%lli, ",sf);
                if (sf == (off_t)-1)
                    log_mesg(0, 1, 1, debug, "clone seek error %lli errno=%i\n", (long long)offset, (int)errno);
	    
	    }
	    log_mesg(0, 0, 0, debug, "end\n");
        }
    
    } else if (opt.restore) {

	/**
	 * read magic string from image file
	 * and check it.
	 */
	r_size = read_all(&dfr, bitmagic_r, 8); /// read a magic string
        cmp = memcmp(bitmagic, bitmagic_r, 8);
        if(cmp != 0)
	    log_mesg(0, 1, 1, debug, "bitmagic error %i\n", cmp);

	/// seek to the first
        sf = lseek(dfw, 0, SEEK_SET);
	log_mesg(0, 0, 0, debug, "seek %lli for writing dtat string\n",sf);
	if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);
    
	/*
	/// log the offset
        sf = lseek(dfr, 0, SEEK_CUR);
	log_mesg(0, 0, 0, debug, "seek %lli for reading data string\n",sf);
	if (sf == (off_t)-1)
	    log_mesg(0, 1, 1, debug, "seek set %lli\n", sf);
	*/

	/// start restore image file to partition
        for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){

	r_size = 0;
	w_size = 0;
        log_mesg(0, 0, 0, debug, "block_id=%lli, ",block_id);

    	if (bitmap[block_id] == 1){ 
	    /// The block is used
	    log_mesg(0, 0, 0, debug, "bitmap=%i, ",bitmap[block_id]);

	    progress_update(&prog, copied);

	    offset = (off_t)(block_id * image_hdr.block_size);
	    //sf = lseek(dfw, offset, SEEK_SET);
            //if (sf == (off_t)-1)
            //    log_mesg(0, 1, 1, debug, "seek error %lli errno=%i\n", (long long)offset, (int)errno);
	    buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
	    r_size = read_all(&dfr, buffer, image_hdr.block_size);
	    log_mesg(0, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
	    if (r_size <0)
		log_mesg(0, 1, 1, debug, "read errno = %i \n", errno);

	    /// write block from buffer to partition
	    w_size = write_all(&dfw, buffer, image_hdr.block_size);
	    log_mesg(0, 0, 0, debug, "bs=%i and w=%i, ",image_hdr.block_size, w_size);
	    if (w_size != (int)image_hdr.block_size)
		log_mesg(0, 1, 1, debug, "write error %i \n", w_size);

	    /// free buffer
	    free(buffer);

       	    copied++;					/// count copied block
	    total_write += (unsigned long long) w_size;	/// count copied size

	    /// read or write error
	    //if ((r_size != w_size) || (r_size != image_hdr.block_size))
	    //	log_mesg(0, 1, 1, debug, "read and write different\n");
       	} else {
	    /// if the block is not used, I just skip it.
	    sf = lseek(dfw, image_hdr.block_size, SEEK_CUR);
	    log_mesg(0, 0, 0, debug, "seek=%lli, ",sf);
	    if (sf == (off_t)-1)
		log_mesg(0, 1, 1, debug, "seek error %lli errno=%i\n", (long long)offset, (int)errno);
	}
	
	log_mesg(0, 0, 0, debug, "end\n");

    	}
    }

    close (dfr);    /// close source
    close (dfw);    /// close target
    free(bitmap);   /// free bitmp
    if(opt.debug)
	close_log();
    return 0;	    /// finish
}
