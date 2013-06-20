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

/// cmd_opt structure defined in partclone.h
cmd_opt opt;

void info_usage(void) {
	fprintf(stderr, "partclone v%s http://partclone.org\nUsage: partclone.info [OPTIONS]\n"
	"\n"
		"    -s,  --source FILE      Source FILE\n"
		"    -L,  --logfile FILE     Log FILE\n"
		"    -dX, --debug=X          Set the debug level to X = [0|1|2]\n"
		"    -q,  --quiet            Disable progress message\n"
		"    -v,  --version          Display partclone version\n"
		"    -h,  --help             Display this help\n"
		, VERSION);
	exit(0);
}


void info_options (int argc, char **argv){

    static const char *sopt = "-hvqd::L:s:";
    static const struct option lopt[] = {
	{ "help",	no_argument,	    NULL,   'h' },
	{ "print_version",  no_argument,	NULL,   'v' },
	{ "source",	required_argument,  NULL,   's' },
	{ "debug",	optional_argument,  NULL,   'd' },
	{ "logfile",	    required_argument,	NULL,   'L' },
	{ "quiet",              no_argument,            NULL,   'q' },
	{ NULL,			0,			NULL,    0  }
    };
	int c;
	memset(&opt, 0, sizeof(cmd_opt));
	opt.debug = 0;
	opt.quiet = 0;
	opt.logfile = "/var/log/partclone.log";
	opt.target  = 0;
	opt.clone   = 0;
	opt.restore = 0;

    while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
	switch (c) {
	    case 'h':
	    case '?':
		info_usage();
		break;
	    case 'v':
		print_version();
		break;
	    case 's':
		opt.source = optarg;
		break;
	    case 'q':
		opt.quiet = 1;
		break;
	    case 'd':
		if (optarg)
		    opt.debug = atol(optarg);
		else
		    opt.debug = 1;
		break;
	    case 'L':
		opt.logfile = optarg;
		break;
	    default:
		fprintf(stderr, "Unknown option '%s'.\n", argv[optind-1]);
		info_usage();
	}
    }

}

/**
 * main functiom - print Image file metadata.
 */
int main(int argc, char **argv){ 

    int		dfr;			/// file descriptor for source and target
    unsigned long	*bitmap;		/// the point for bitmap data
    image_head	image_hdr;		/// image_head structure defined in partclone.h

    if (argc == 2){
	memset(&opt, 0, sizeof(cmd_opt));
	opt.source  = argv[1];
	opt.logfile = "/var/log/partclone.log";
	dfr = open(opt.source, O_RDONLY);
	if (dfr == -1)
	    info_options(argc, argv);
    } else
	info_options(argc, argv);
    open_log(opt.logfile);

    /// print partclone info
    print_partclone_info(opt);

    /**
     * open Image file
     */
    dfr = open(opt.source, O_RDONLY);
    if (dfr == -1)
	printf("Can't open file(%s)\n", opt.source);

    /// get image information from image file
    restore_image_hdr(&dfr, &opt, &image_hdr);
    if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0)
	log_mesg(0, 1, 1, opt.debug, "The Image magic error. This file is NOT partclone Image\n");

    /// alloc a memory to restore bitmap
    bitmap = pc_alloc_bitmap(image_hdr.totalblock);
    if (bitmap == NULL)
	log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    log_mesg(0, 0, 0, opt.debug, "initial main bitmap pointer %p\n", bitmap);
    log_mesg(0, 0, 0, opt.debug, "Initial image hdr: read bitmap table\n");

    /// read and check bitmap from image file
    get_image_bitmap(&dfr, opt, image_hdr, bitmap);

    log_mesg(0, 0, 0, opt.debug, "check main bitmap pointer %p\n", bitmap);
    log_mesg(0, 0, 0, opt.debug, "print image_head\n");

    /// print image_head
    print_image_hdr_info(image_hdr, opt);

    close(dfr);     /// close source
    free(bitmap);   /// free bitmp
    close_log();
    return 0;       /// finish
}
