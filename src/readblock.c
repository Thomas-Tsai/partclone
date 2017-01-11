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

#include "partclone.h"

/// cmd_opt structure defined in partclone.h
cmd_opt opt;

void info_usage(void) {
	fprintf(stderr, "partclone v%s http://partclone.org\n"
	                "Usage: partclone.info [FILE]\n"
	                "Or: partclone.info [OPTIONS]\n"
	                "\n"
		"    -s,  --source FILE      Source FILE, or stdin(-)\n"
		"    -o,  --output FILE      Target FILE, or stdin(-)\n"
		"    -L,  --logfile FILE     Log FILE\n"
		"    -dX, --debug=X          Set the debug level to X = [0|1|2]\n"
		"    -q,  --quiet            Disable progress message\n"
		"    -v,  --version          Display partclone version\n"
		"    -h,  --help             Display this help\n"
		, VERSION);
	exit(1);
}


void info_options (int argc, char **argv){

    static const char *sopt = "-hvqd::L:s:o:k:";
    static const struct option lopt[] = {
	{ "help",	no_argument,	    NULL,   'h' },
	{ "print_version",  no_argument,	NULL,   'v' },
	{ "source",	required_argument,  NULL,   's' },
	{ "target",	required_argument,  NULL,   'o' },
	{ "debug",	optional_argument,  NULL,   'd' },
	{ "logfile",	    required_argument,	NULL,   'L' },
	{ "quiet",              no_argument,            NULL,   'q' },
	{ NULL,			0,			NULL,    0  }
    };
	int c;
	memset(&opt, 0, sizeof(cmd_opt));
	opt.debug = 0;
	opt.quiet = 0;
	opt.info  = 1;
	opt.logfile = "/var/log/partclone.log";
	opt.clone   = 0;
	opt.restore = 1;
	opt.info = 0;
	opt.overwrite = 1;
	

    while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != -1) {
	switch (c) {
	    case 'h':
	    case '?':
		info_usage();
		break;
	    case 'v':
		print_version();
		break;
	    case 'o':
		opt.target = optarg;
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

    if ( opt.source == 0 )
        info_usage();

}

/**
 * main functiom - print Image file metadata.
 */
int main(int argc, char **argv){ 

	int dfr;                  /// file descriptor for source and target
	unsigned long   *bitmap;  /// the point for bitmap data
	image_head_v2    img_head;
	file_system_info fs_info;
	image_options    img_opt;

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
    if (strcmp(opt.source, "-") == 0) {
	if ((dfr = fileno(stdin)) == -1)
	    log_mesg(0, 1, 1, opt.debug, "info: open %s(stdin) error\n", opt.source);
    } else {
	dfr = open(opt.source, O_RDONLY);
	if (dfr == -1)
	    log_mesg(0, 1, 1, opt.debug, "info: Can't open file(%s)\n", opt.source);
    }

    /// get image information from image file
    load_image_desc(&dfr, &opt, &img_head, &fs_info, &img_opt);

    /// alloc a memory to restore bitmap
    bitmap = pc_alloc_bitmap(fs_info.totalblock);
    if (bitmap == NULL)
	log_mesg(0, 1, 1, opt.debug, "%s, %i, not enough memory\n", __func__, __LINE__);

    log_mesg(0, 0, 0, opt.debug, "initial main bitmap pointer %p\n", bitmap);
    log_mesg(0, 0, 0, opt.debug, "Initial image hdr: read bitmap table\n");

    /// read and check bitmap from image file
    load_image_bitmap(&dfr, opt, fs_info, img_opt, bitmap);

    log_mesg(0, 0, 0, opt.debug, "check main bitmap pointer %p\n", bitmap);
    log_mesg(0, 0, 0, opt.debug, "print image information\n");

    print_file_system_info(fs_info, opt);
    log_mesg(0, 0, 1, opt.debug, "\n");
    print_image_info(img_head, img_opt, opt);

    int dfw = open_target(opt.target, &opt);
    if (dfw == -1) {
	log_mesg(0, 1, 1, opt.debug, "Error exit\n");
    }

    char *read_buffer, *write_buffer;
    int r_size=0;
    int w_size = 0;
    unsigned long int block = 0;

    unsigned long long used = 0;
    unsigned int i;

    for(i = 0; i < block; ++i) {
	if (pc_test_bit(i, bitmap, fs_info.totalblock))
	    ++used;
    }

    printf("used = %i\n", used);

    read_buffer = (char*)malloc(fs_info.block_size);
    write_buffer = (char*)malloc(fs_info.block_size);
    log_mesg(0, 0, 1, opt.debug, "get block data %ld\n", block);
    unsigned long int seek_crc_size = ((used-1) / img_opt.blocks_per_checksum) * img_opt.checksum_size;
    long int bseek = fs_info.block_size*(used-1)+seek_crc_size;
    int baseseek = lseek(dfr, 0, SEEK_CUR);
    printf("baseseek = %i\n", baseseek);
    if (lseek(dfr, bseek, SEEK_CUR) == (off_t)-1)
	log_mesg(0, 1, 1, opt.debug, "source seek ERROR:%s\n", strerror(errno));
    r_size = read_all(&dfr, read_buffer, fs_info.block_size, &opt);
    w_size = write_all(&dfw, read_buffer, fs_info.block_size, &opt);
    close(dfw);

    close(dfr);     /// close source
    free(bitmap);   /// free bitmap
    close_log();
    free(read_buffer);
    free(write_buffer);
    return 0;       /// finish
}
