/**
 * The part of partclone 
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * The utility to print out the Image information. 
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
 * partclone.h - include some structures like image_head, opt_cmd, ....
 *               and functions for main used.
 */
#include "partclone.h"

cmd_opt             opt;                    /// cmd_opt structure defined in partclone.h
p_dialog_mesg	    m_dialog;

/**
 * main functiom - print Image file metadata.
 */
int main(int argc, char **argv){ 

    char*		source;			/// source data
    int			dfr;			/// file descriptor for source and target
    char		*bitmap;		/// the point for bitmap data
    image_head		image_hdr;		/// image_head structure defined in partclone.h
    int debug = 1;

    if( !argv[1]) {
	printf("Please give the image file name.\n");
	exit(0);
    }
    opt.source  = argv[1];
    opt.target  = 0;
    opt.debug   = 0;
    opt.clone   = 0;
    opt.restore = 0;
    opt.logfile = "/var/log/partclone.log";
    open_log(opt.logfile);

    /// print partclone info
    print_partclone_info(opt);

    /**
     * open Image file
     */
    source = argv[1];

    dfr = open(source, O_RDONLY);
    if (dfr == -1)
	printf("Can't open file(%s)\n", source);

    /// get image information from image file
    restore_image_hdr(&dfr, &opt, &image_hdr);
    if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0)
	log_mesg(0, 1, 1, debug, "The Image magic error. This file is NOT partclone Image\n");

    /// alloc a memory to restore bitmap
    bitmap = (char*)malloc(sizeof(char)*image_hdr.totalblock);

    log_mesg(0, 0, 0, debug, "initial main bitmap pointer %lli\n", bitmap);
    log_mesg(0, 0, 0, debug, "Initial image hdr: read bitmap table\n");

    /// read and check bitmap from image file
    get_image_bitmap(&dfr, opt, image_hdr, bitmap);

    log_mesg(0, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
    log_mesg(0, 0, 0, debug, "print image_head\n");

    /// print image_head
    print_image_hdr_info(image_hdr, opt);
	
    close (dfr);    /// close source
    free(bitmap);   /// free bitmp
    close_log();
    return 0;	    /// finish
}
