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
#include <getopt.h>
#include "version.h"

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
char *EXECNAME="partclone.chkimg";

static void usage_chkimg(void)
{
    fprintf(stderr, "%s v%s http://partclone.org\nUsage: %s [OPTIONS]\n"
            "    Efficiently clone to a image, device or standard output.\n"
            "\n"
            "    -s,  --source FILE      Source FILE\n"
            "    -L,  --logfile FILE     Log FILE\n"
            "    -dX, --debug=X          Set the debug level to X = [0|1|2]\n"
            "    -C,  --no_check         Don't check device size and free space\n"
#ifdef HAVE_LIBNCURSESW
            "    -N,  --ncurses          Using Ncurses User Interface\n"
#endif
            "    -X,  --dialog           output message as Dialog Format\n"
            "    -F,  --force            force progress\n"
            "    -f,  --UI-fresh         fresh times of progress\n"
            "    -h,  --help             Display this help\n"
            , EXECNAME, VERSION, EXECNAME);
    exit(0);
}

static void parse_option_chkimg(int argc, char** argv, cmd_opt* option){
    static const char *sopt = "-hd::L:s:f:CXFN";
    static const struct option lopt[] = {
        { "help",		no_argument,	    NULL,   'h' },
        { "source",		required_argument,  NULL,   's' },
        { "debug",		optional_argument,  NULL,   'd' },
        { "UI-fresh",	required_argument,  NULL,   'u' },
        { "check",		no_argument,	    NULL,   'C' },
        { "dialog",		no_argument,	    NULL,   'X' },
        { "logfile",	required_argument,  NULL,   'L' },
        { "force",		no_argument,	    NULL,   'F' },
#ifdef HAVE_LIBNCURSESW
        { "ncurses",		no_argument,	    NULL,   'N' },
#endif
        { NULL,			0,		    NULL,    0  }
    };

    char c;
    memset(option, 0, sizeof(cmd_opt));
    option->debug = 0;
    option->check = 1;
    option->restore = 1;
    option->chkimg = 1;
    option->logfile = "/var/log/partclone.log";
    while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != (char)-1) {
        switch (c) {
            case 's': 
                option->source = optarg;
                break;
            case 'h':
                usage_chkimg();
                break;
            case '?':
                usage_chkimg();
                break;
            case 'd':
                if (optarg)
                    option->debug = atol(optarg);
                else
                    option->debug = 1;
                break;
            case 'L': 
                option->logfile = optarg;
                break;
            case 'f':
                option->fresh = atol(optarg);
                break;
            case 'F':
                option->force++;
                break;
            case 'X':
                /// output message as dialog format, reference
                /// dialog --guage is text height width percent
                ///    A guage box displays a meter along the bottom of the box. The meter indicates the percentage. New percentages are read from standard input, one integer per line. The meter is updated to reflect each new percentage. If stdin is XXX, then the first line following is taken as an integer percentage, then subsequent lines up to another XXX are used for a new prompt. The guage exits when EOF is reached on stdin. 
                option->dialog = 1;
                break;
#ifdef HAVE_LIBNCURSESW
            case 'N':
                option->ncurses = 1;
                break;
#endif
            case 'C':
                option->check = 0;
                break;
            default:
                fprintf(stderr, "Unknown option '%s'.\n", argv[optind-1]);
                usage_chkimg();
        }
    }

    if (!option->debug){
        option->debug = 0;
    }

    if (option->source == NULL){
        fprintf(stderr, "There is no image name. or --help get more info.\n");
        exit(0);
    }

}

/**
 * main functiom - for colne or restore data
 */
int main(int argc, char **argv){ 

    char*		source;			/// source data
    int			dfr;		/// file descriptor for source and target
    int			r_size;		/// read and write size
    char*		buffer;		/// buffer data
    char*		buffer2;		/// buffer data
    unsigned long long	block_id, copied = 0;	/// block_id is every block in partition
    /// copied is copied block count
    off_t		offset = 0, sf = 0;	/// seek postition, lseek result
    int			start, stop;		/// start, range, stop number for progress bar
    char		bitmagic[8] = "BiTmAgIc";// only for check postition
    char		bitmagic_r[8];		/// read magic string from image
    int			cmp;			/// compare magic string
    char		*bitmap;		/// the point for bitmap data
    int			debug = 0;		/// debug or not
    unsigned long	crc = 0xffffffffL;	/// CRC32 check code for writint to image
    unsigned long	crc_ck = 0xffffffffL;	/// CRC32 check code for checking
    unsigned long	crc_ck2 = 0xffffffffL;	/// CRC32 check code for checking
    int			c_size;			/// CRC32 code size
    char*		crc_buffer;		/// buffer data for malloc crc code
    int			done = 0;
    int			s_count = 0;
    int			rescue_num = 0;
    int			tui = 0;		/// text user interface
    int			pui = 0;		/// progress mode(default text)
    int			raw = 0;
    char		image_hdr_magic[512];

    progress_bar	prog;			/// progress_bar structure defined in progress.h
    image_head		image_hdr;		/// image_head structure defined in partclone.h

    /**
     * get option and assign to opt structure
     * check parameter and read from argv
     */
    parse_option_chkimg(argc, argv, &opt);

    /**
     * if "-d / --debug" given
     * open debug file in "/var/log/partclone.log" for log message 
     */
    debug = opt.debug;
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

    /*
    if (geteuid() != 0)
        log_mesg(0, 1, 1, debug, "You are not logged as root. You may have \"access denied\" errors when working.\n"); 
    else
        log_mesg(1, 0, 0, debug, "UID is root.\n");
    */

    /**
     * open source and target 
     * clone mode, source is device and target is image file/stdout
     * restore mode, source is image file/stdin and target is device
     * dd mode, source is device and target is device !!not complete
     */
#ifdef _FILE_OFFSET_BITS
    log_mesg(1, 0, 0, debug, "enable _FILE_OFFSET_BITS %i\n", _FILE_OFFSET_BITS);
#endif
    dfr = open_source(opt.source, &opt);
    if (dfr == -1) {
        log_mesg(0, 1, 1, debug, "Erro EXIT.\n");
    }

    /**
     * get partition information like super block, image_head, bitmap
     * from device or image file.
     */


    log_mesg(1, 0, 0, debug, "Checking image hdr - get image_head from image file\n");

    restore_image_hdr(&dfr, &opt, &image_hdr);
    /// check the image magic
    if (memcmp(image_hdr.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE) != 0)
        log_mesg(0, 1, 1, debug, "The Image magic error. This file is NOT partclone Image\n");

    /// alloc a memory to restore bitmap
    bitmap = (char*)malloc(sizeof(char)*image_hdr.totalblock);
    if(bitmap == NULL){
        log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
    }

    log_mesg(2, 0, 0, debug, "initial main bitmap pointer %lli\n", bitmap);
    log_mesg(1, 0, 0, debug, "Initial image hdr - read bitmap table\n");

    /// read and check bitmap from image file
    log_mesg(0, 0, 1, debug, "Calculating bitmap... ");
    log_mesg(0, 0, 1, debug, "Please wait... ");
    get_image_bitmap(&dfr, opt, image_hdr, bitmap);

    log_mesg(2, 0, 0, debug, "check main bitmap pointer %i\n", bitmap);
    log_mesg(0, 0, 1, debug, "done!\n");

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
    stop = image_hdr.usedblocks;	/// get the end of progress number, only used block
    log_mesg(1, 0, 0, debug, "Initial Progress bar\n");
    /// Initial progress bar
    progress_init(&prog, start, stop, image_hdr.block_size);
    copied = 1;				/// initial number is 1

    /**
     * start read and write data between device and image file
     */

    /**
     * read magic string from image file
     * and check it.
     */
    r_size = read_all(&dfr, bitmagic_r, 8, &opt); /// read a magic string
    cmp = memcmp(bitmagic, bitmagic_r, 8);
    if(cmp != 0)
        log_mesg(0, 1, 1, debug, "bitmagic error %i\n", cmp);

    /// start restore image file to partition
    for( block_id = 0; block_id < image_hdr.totalblock; block_id++ ){

        r_size = 0;

        if((block_id + 1) == image_hdr.totalblock) 
            done = 1;

#ifdef _FILE_OFFSET_BITS
        if(copied == image_hdr.usedblocks) 
            done = 1;
#endif
        if (bitmap[block_id] == 1){ 
            /// The block is used
            log_mesg(2, 0, 0, debug, "block_id=%lli, ",block_id);
            log_mesg(1, 0, 0, debug, "bitmap=%i, ",bitmap[block_id]);

            offset = (off_t)(block_id * image_hdr.block_size);
            buffer = (char*)malloc(image_hdr.block_size); ///alloc a memory to copy data
            if(buffer == NULL){
                log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
            }
            r_size = read_all(&dfr, buffer, image_hdr.block_size, &opt);
            log_mesg(1, 0, 0, debug, "bs=%i and r=%i, ",image_hdr.block_size, r_size);
            if (r_size <0)
                log_mesg(0, 1, 1, debug, "read errno = %i \n", errno);

            /// read crc32 code and check it.
            crc_ck = crc32(crc_ck, buffer, r_size);
            crc_buffer = (char*)malloc(CRC_SIZE); ///alloc a memory to copy data
            if(crc_buffer == NULL){
                log_mesg(0, 1, 1, debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
            }
            c_size = read_all(&dfr, crc_buffer, CRC_SIZE, &opt);
            if (c_size < CRC_SIZE)
                log_mesg(0, 1, 1, debug, "read CRC error, please check your image file. \n");
            memcpy(&crc, crc_buffer, CRC_SIZE);
            if (memcmp(&crc, &crc_ck, CRC_SIZE) != 0){
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
                memcpy(&crc, crc_buffer, CRC_SIZE);
                if (memcmp(&crc, &crc_ck2, CRC_SIZE) != 0) {
                    log_mesg(0, 1, 1, debug, "CRC error again at %i...\n ", sf);
                } else {
                    crc_ck = crc_ck2;
                }
                free(buffer2);

            } else {
                crc_ck2 = crc_ck;
            }


            /// free buffer
            free(buffer);
            free(crc_buffer);


            copied++;					/// count copied block

            log_mesg(1, 0, 0, debug, "end\n");
        }

	update_pui(&prog, copied, done);
	if(done = 1)
	    break;
    } // end of for
    print_finish_info(opt);

    close (dfr);    /// close source
    free(bitmap);   /// free bitmp
    close_pui(pui);
    printf("Checked successfully.\n");
    if(opt.debug)
        close_log();
    return 0;	    /// finish
}
