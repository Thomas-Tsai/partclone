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

/**
 * progress.h - only for progress bar
 */
#include "progress.h"

/**
 * partclone.h - include some structures like image_head, opt_cmd, ....
 *               and functions for main used.
 */
#include "partclone.h"

/// global variable
cmd_opt		opt;			/// cmd_opt structure defined in partclone.h
p_dialog_mesg	m_dialog;			/// dialog format string
char *EXECNAME = "partclone.dd";

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
    int			start, stop;		/// start, range, stop number for progress bar
    unsigned long long	total_write = 0;	/// the copied size 
    unsigned long long	needed_size = 0;	/// the copied size 
    unsigned long long	needed_mem  = 0;	/// the copied size 
    char		bitmagic[8] = "BiTmAgIc";// only for check postition
    char		bitmagic_r[8];		/// read magic string from image
    int			cmp;			/// compare magic string
    int			debug = 0;		/// debug or not
    int			done = 0;
    int			s_count = 0;
    int			rescue_num = 0;
    unsigned long long	rescue_pos = 0;
    int			tui = 0;		/// text user interface
    int			pui = 0;		/// progress mode(default text)
    char *bad_sectors_warning_msg =
        "*************************************************************************\n"
        "* WARNING: The disk has bad sector. This means physical damage on the   *\n"
        "* disk surface caused by deterioration, manufacturing faults or other   *\n"
        "* reason. The reliability of the disk may stay stable or degrade fast.  *\n"
        "* Use the --rescue option to efficiently save as much data as possible! *\n"
        "*************************************************************************\n";

    progress_bar	prog;			/// progress_bar structure defined in progress.h
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
    } else if (opt.dialog){
        pui = DIALOG;
        log_mesg(1, 0, 0, debug, "Using Dialog User Interface mode.\n");
    } else
        pui = TEXT;

    tui = open_pui(pui, opt.fresh);
    if ((opt.ncurses) && (tui == 0)){
        opt.ncurses = 0;
        log_mesg(1, 0, 0, debug, "Open Ncurses User Interface Error.\n");
    } else if ((opt.dialog) && (tui == 1)){
        m_dialog.percent = 1;
    }

    /// print partclone info
    print_partclone_info(opt);

    if (geteuid() != 0)
        log_mesg(0, 1, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n"); 
    else
        log_mesg(1, 0, 0, debug, "UID is root.\n");

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

    log_mesg(1, 0, 0, debug, "Initial image hdr - get Super Block from partition\n");

    /// get Super Block information from partition
    initial_dd_hdr(dfr, &image_hdr);

    /// check memory size
    if (check_mem_size(image_hdr, opt, &needed_mem) == -1)
        log_mesg(0, 1, 1, debug, "Ther is no enough free memory, partclone suggests you should have %i bytes memory\n", needed_mem);

    needed_size = (unsigned long long)(((image_hdr.block_size+sizeof(unsigned long))*image_hdr.usedblocks)+sizeof(image_hdr)+sizeof(char)*image_hdr.totalblock);

    if (opt.check){
        if (opt.clone){
            check_free_space(&dfw, needed_size);
        } else {
            check_size(&dfw, image_hdr.device_size);
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
    progress_init(&prog, start, stop, image_hdr.block_size);
    copied = 0;				/// initial number is 0

    /**
     * start read and write data between device and image file
     */

    /// read data from the first block and log the offset
    log_mesg(0, 0, 0, debug, "Total block %i\n", image_hdr.totalblock);

    /// start clone partition to image file
    block_id = 0;
    buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
    if(buffer == NULL){
	log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
    }

    do {

        r_size = 0;
        w_size = 0;
	memset(buffer, 0, image_hdr.block_size);

        log_mesg(1, 0, 0, debug, "block_id=%lli, ",block_id);

        /// read data from source to buffer
	rescue_pos = lseek(dfr, 0, SEEK_CUR);
        r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
        log_mesg(1, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
        if (r_size != (int)image_hdr.block_size){

            if ((r_size == -1) && (errno == EIO)){
                if (opt.rescue){
		    r_size = 0;
                    for (rescue_num = 0; rescue_num < image_hdr.block_size; rescue_num += SECTOR_SIZE){
                        rescue_sector(&dfr, rescue_pos + rescue_num, buffer + rescue_num, &opt);
			r_size+=SECTOR_SIZE;
		    }
                }else{
                    log_mesg(0, 1, 1, debug, "%s", bad_sectors_warning_msg);
                }

            }else if (r_size == 0) {
		//EIO
		done = 1;
	    }else
		log_mesg(0, 1, 1, debug, "read error: %s(%i) rsize=%i\n", strerror(errno), errno, r_size);

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


        copied++;					/// count copied block
        total_write += (unsigned long long)(w_size);	/// count copied size
        log_mesg(1, 0, 0, debug, "total=%lli, ", total_write);

        /// read or write error
        if (r_size != w_size)
            log_mesg(0, 1, 1, debug, "read(%i) and write(%i) different\n", r_size, w_size);
        log_mesg(1, 0, 0, debug, "end\n");
        block_id++;
	update_pui(&prog, copied, done);
    } while (done == 0);/// end of for    
    sync_data(dfw, &opt);	
    /// free buffer
    free(buffer);

    print_finish_info(opt);

    close (dfr);    /// close source
    close (dfw);    /// close target
    close_pui(pui);    /// close target
    printf("Cloned successfully.\n");
    if(opt.debug)
        close_log();
    return 0;	    /// finish
}
