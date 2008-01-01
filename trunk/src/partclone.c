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

//##define _FILE_OFFSET_BITS 64
#include <config.h>
#define _LARGEFILE64_SOURCE
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
#include <getopt.h>
#include <locale.h>
#include "gettext.h"
#define _(STRING) gettext(STRING)
//#define PACKAGE "partclone"

#include "partclone.h"


FILE* msg = NULL;

/**
 * options - 
 * usage	    - print message "how to use this"
 * parse_options    - get parameter from agrc, argv
 */
extern void usage(void)
{
    fprintf(stderr, "\nUsage: %s [OPTIONS]\n"
        "    Efficiently clone to a image, device or standard output.\n"
        "\n"
        "    -o, --output FILE      Output FILE\n"
	"    -s, --source FILE      Source FILE\n"
        "    -c, --clone            Save to the special image format\n"
        "    -r, --restore          Restore from the special image format\n"
	"    -b, --dd-mode          Save to sector-to-sector format\n"
        "    -d, --debug            Show debug information\n"
        "    -h, --help             Display this help\n"
    , EXECNAME);
    exit(0);
}

extern void parse_options(int argc, char **argv, cmd_opt* opt)
{
    static const char *sopt = "-hdrco:s:";
    static const struct option lopt[] = {
        { "help",		no_argument,	    NULL,   'h' },
        { "output",		required_argument,  NULL,   'o' },
        { "source",		required_argument,  NULL,   's' },
        { "restore-image",	no_argument,	    NULL,   'r' },
        { "clone-image",	no_argument,	    NULL,   'c' },
        { "dd-mode",		no_argument,	    NULL,   'b' },
        { "debug",		no_argument,	    NULL,   'd' },
        { NULL,			0,		    NULL,    0  }
    };

    char c;
    memset(opt, 0, sizeof(cmd_opt));

    while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != (char)-1) {
            switch (c) {
            case 's': 
                    if (opt->source)
                            usage();
                    opt->source = optarg;
                    break;
            case 'h':
                    usage();
                    break;
            case '?':
                    usage();
                    break;
            case 'o':
                    if (opt->target)
                            usage();
                    opt->target = optarg;
                    break;
            case 'r':
                    opt->restore++;
                    break;
            case 'c':
                    opt->clone++;
                    break;
/*            case 'b':
                    opt->dd++;
                    break;*/
            case 'd':
                    opt->debug++;
                    break;
            default:
                    fprintf(stderr, "Unknown option '%s'.\n", argv[optind-1]);
                    usage();
            }
    }

    if (opt->target == NULL) {
        fprintf(stderr, "You must specify an output file or device.\n");
        usage();
    }


    if (opt->source == NULL) {
        fprintf(stderr, "You must specify an input file or device.\n");
		usage();
    }

}


/**
 * debug message
 * open_log	- to open file /var/log/partclone.log
 * log_mesg	- write log to the file
 *		- write log and exit 
 *		- write to stderr...
 * close_log	- to close file /var/log/partclone.log
 */
extern void open_log(){
    msg = fopen("/var/log/partclone.log","w");
	if(msg == NULL){
		fprintf(stderr, "open /var/log/partclone.log error\n");
		exit(0);
	}
}

extern void log_mesg(int log_errno, int log_exit, int log_stderr, int debug, const char *fmt, ...){

    va_list args;
    va_start(args, fmt);
	
    /// write log to stderr if log_stderr true
    if(log_stderr)
        vfprintf(stderr, fmt, args);

    /// write log to log file if debug true
    if(debug)
	vfprintf(msg, fmt, args);
    va_end(args);

    /// clear message
    fflush(msg);

    /// exit if lexit true
    if (log_exit)
	exit(1);
}
 
extern void close_log(){
    fclose(msg);
}

/**
 * for restore used functions
 * restore_image_hdr	- get image_head from image file 
 * get_image_bitmap	- read bitmap data from image file
 */
extern void restore_image_hdr(int* ret, cmd_opt* opt, image_head* image_hdr){
    int r_size;
    char* buffer;
    int debug = opt->debug;

    buffer = (char*)malloc(sizeof(image_head));
    r_size = read(*ret, buffer, sizeof(image_head));
    if (r_size == -1)
        log_mesg(0, 1, 1, debug, "read image_hdr error\n");
    memcpy(image_hdr, buffer, sizeof(image_head));
    free(buffer);
}

extern void get_image_bitmap(int* ret, cmd_opt opt, image_head image_hdr, char* bitmap){
    int r_size;
    int offset = 0;
    char* buffer;
    int i, n_used;
    unsigned long long block_id;
    unsigned long bused = 0, bfree = 0;
    int debug = opt.debug;

    buffer = (char*)malloc(sizeof(char)*image_hdr.totalblock);
    r_size = read(*ret, buffer, sizeof(char)*(image_hdr.totalblock));
    memcpy(bitmap, buffer, sizeof(char)*(image_hdr.totalblock));
    free(buffer);

    if (r_size == -1)
        log_mesg(0, 1, 1, debug, "read image_bitmap error(%i)\n", image_hdr.totalblock);
    for (block_id = 0; block_id < image_hdr.totalblock; block_id++){
        if(bitmap[block_id] == 1){
	    //printf("u = %i\n",block_id);
            bused++;
	} else {
	    //printf("n = %i\n",block_id);
            bfree++;
	}
    }
    if(image_hdr.usedblocks != bused)
        log_mesg(0, 1, 1, debug, "restore block %li, and free %li", bused, bfree);
    
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

extern int open_source(char* source, cmd_opt* opt){
    int ret;
	int debug = opt->debug;

    if(opt->clone){
        ret = open(source, O_RDONLY | O_LARGEFILE);
		if (ret == -1)
			log_mesg(0, 1, 1, debug, "clone: open %s error\n", source);
    } else if(opt->restore) {
    	if (strcmp(source, "-") == 0){ 
        	ret = fileno(stdin); 
			if (ret == -1)
				log_mesg(0, 1, 1, debug,"restore: open %s(stdin) error\n", source);
	    } else {
    	    ret = open (source, O_RDONLY | O_LARGEFILE, S_IRWXU);
			if (ret == -1)
				log_mesg(0, 1, 1, debug, "restore: open %s error\n", source);
	    }
	}

    return ret;
}

extern int open_target(char* target, cmd_opt* opt){
    int ret;
	int debug = opt->debug;

    if (opt->clone){
    	if (strcmp(target, "-") == 0){ 
        	ret = fileno(stdout);
			if (ret == -1)
				log_mesg(0, 1, 1, debug, "clone: open %s(stdout) error\n", target);
    	} else { 
        	ret = open (target, O_WRONLY | O_CREAT | O_LARGEFILE);
			if (ret == -1)
				log_mesg(0, 1, 1, debug, "clone: open %s error\n", target);
    	}
    } else if(opt->restore) {
	    	ret = open (target, O_WRONLY | O_LARGEFILE);
			if (ret == -1)
				log_mesg(0, 1, 1, debug, "restore: open %s error\n", target);
    }

    return ret;
}

/// print image head
extern void print_image_hdr_info(image_head image_hdr, cmd_opt opt){
 
    int block_s  = image_hdr.block_size;
    unsigned long long total    = image_hdr.totalblock;
    unsigned long long used     = image_hdr.usedblocks;
    unsigned long long dev_size = image_hdr.device_size;
    int debug = opt.debug;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	//log_mesg(0, 0, 0, "%s v%s \n", EXEC_NAME, VERSION);
    if (opt.clone)
		log_mesg(0, 0, 1, debug, _("Starting clone device(%s) to image(%s)\n"), opt.source, opt.target);	
	else if(opt.restore)
		log_mesg(0, 0, 1, debug, _("Starting restore image(%s) to device(%s)\n"), opt.source, opt.target);
//	else if(opt.dd)
//		log_mesg(0, 0, 1, debug, _("Starting back up device(%s) to device(%s)\n"), opt.source, opt.target);
	else
		log_mesg(0, 0, 1, debug, "unknow mode\n");
	log_mesg(0, 0, 1, debug, _("The device size is %lli\n"), (total*block_s));
	log_mesg(0, 0, 1, debug, _("The used size is %lli\n"), (used*block_s));
	log_mesg(0, 0, 1, debug, _("The block size is %i\n"), block_s);
	log_mesg(0, 0, 1, debug, _("The used block count is %lli\n"), used);
}

