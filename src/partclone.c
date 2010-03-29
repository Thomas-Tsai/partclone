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

#include <config.h>
#define _LARGEFILE64_SOURCE
#include <features.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <locale.h>
#include <mntent.h>
#include <limits.h>
#include <stdlib.h>
#include <errno.h>
#include "gettext.h"
#include <linux/fs.h>
#define _(STRING) gettext(STRING)
//#define PACKAGE "partclone"
#include "version.h"
#include "partclone.h"

#if defined(linux) && defined(_IO) && !defined(BLKGETSIZE)
#define BLKGETSIZE      _IO(0x12,96)  /* Get device size in 512-byte blocks. */
#endif
#if defined(linux) && defined(_IOR) && !defined(BLKGETSIZE64)
#define BLKGETSIZE64    _IOR(0x12,114,size_t)   /* Get device size in bytes. */
#endif


FILE* msg = NULL;
#ifdef HAVE_LIBNCURSESW
#include <ncurses.h>
WINDOW *log_win;
WINDOW *p_win;
WINDOW *box_win;
WINDOW *bar_win;
int log_y_line = 0;
#endif

extern void print_readable_size_str(unsigned long long size_byte, char *new_size_str){

    float new_size = 1.0;
    memset(new_size_str, 0, 11);
    uint64_t tbyte=1000000000000.0;
    uint64_t gbyte=1000000000;
    uint64_t mbyte=1000000;
    uint64_t kbyte=1000;
    
    if (size_byte == 0)
	snprintf(new_size_str, 11, "%lli", size_byte);

    if (size_byte >= tbyte){
	new_size = (float)size_byte / (float)tbyte;
	snprintf(new_size_str, 11, "%5.1f TB", new_size);
    }else if (size_byte >= gbyte){
	new_size = (float)size_byte / (float)gbyte;
	snprintf(new_size_str, 11, "%5.1f GB", new_size);
    }else if (size_byte >= mbyte){
	new_size = (float)size_byte / (float)mbyte;
	snprintf(new_size_str, 11, "%5.1f MB", new_size);
    }else if (size_byte >= kbyte){
	new_size = (float)size_byte / (float)kbyte;
	snprintf(new_size_str, 11, "%5.1f KB", new_size);
    }else{
	snprintf(new_size_str, 11, "%3i Byte", (int)size_byte);
    }

}

/**
 * options - 
 * usage		    - print message "how to use this"
 * print_version
 * parse_options	    - get parameter from agrc, argv
 */
extern void usage(void)
{
    fprintf(stderr, "%s v%s http://partclone.org\nUsage: %s [OPTIONS]\n"
            "    Efficiently clone to a image, device or standard output.\n"
            "\n"
            "    -o,  --output FILE      Output FILE\n"
            "    -O   --overwrite FILE   Output FILE, overwriting if exists\n"
            "    -s,  --source FILE      Source FILE\n"
            "    -L,  --logfile FILE     Log FILE\n"
#ifndef	RESTORE
            "    -c,  --clone            Save to the special image format\n"
            "    -r,  --restore          Restore from the special image format\n"
            "    -b,  --dev-to-dev       Local device to device copy mode\n"
            "    -R,  --rescue           Continue after disk read errors\n"
#endif
            "    -dX, --debug=X          Set the debug level to X = [0|1|2]\n"
            "    -C,  --no_check         Don't check device size and free space\n"
#ifdef HAVE_LIBNCURSESW
            "    -N,  --ncurses          Using Ncurses User Interface\n"
#endif
            "    -X,  --dialog           Output message as Dialog Format\n"
            "    -I,  --ignore_fschk     Ignore filesystem check\n"
	    "         --ignore_crc       Ignore crc check error\n"
            "    -F,  --force            Force progress\n"
            "    -f,  --UI-fresh         Fresh times of progress\n"
            "    -v,  --version          Display partclone version\n"
            "    -h,  --help             Display this help\n"
            , EXECNAME, VERSION, EXECNAME);
    exit(0);
}

extern void print_version(void){
    printf("Partclone : v%s (%s) \n", VERSION, git_version);
    exit(0);
}

extern void parse_options(int argc, char **argv, cmd_opt* opt)
{
    static const char *sopt = "-hvd::L:cbro:O:s:f:RCXFINi";
    static const struct option lopt[] = {
        { "help",		no_argument,	    NULL,   'h' },
        { "print_version",	no_argument,	    NULL,   'v' },
        { "output",		required_argument,  NULL,   'o' },
        { "overwrite",	required_argument,  NULL,   'O' },
        { "source",		required_argument,  NULL,   's' },
        { "restore-image",	no_argument,	    NULL,   'r' },
        { "clone-image",	no_argument,	    NULL,   'c' },
        { "dev-to-dev",		no_argument,	    NULL,   'b' },
        { "debug",		optional_argument,  NULL,   'd' },
        { "logfile",	required_argument,  NULL,   'L' },
        { "rescue",		no_argument,	    NULL,   'R' },
        { "UI-fresh",	required_argument,  NULL,   'u' },
        { "check",		no_argument,	    NULL,   'C' },
        { "dialog",		no_argument,	    NULL,   'X' },
        { "ignore_fschk",   no_argument,    NULL,   'I' },
	{ "ignore_crc",     no_argument,    NULL,   'i' },
        { "force",		no_argument,	    NULL,   'F' },
#ifdef HAVE_LIBNCURSESW
        { "ncurses",		no_argument,	    NULL,   'N' },
#endif
        { NULL,			0,		    NULL,    0  }
    };

    char c;
    int mode = 0;
    memset(opt, 0, sizeof(cmd_opt));
    opt->debug = 0;
    opt->rescue = 0;
    opt->check = 1;
    opt->ignore_crc = 0;
    opt->logfile = "/var/log/partclone.log";

#ifdef RESTORE
    opt->restore++;
    mode++;
#endif
    while ((c = getopt_long(argc, argv, sopt, lopt, NULL)) != (char)-1) {
        switch (c) {
            case 's': 
                opt->source = optarg;
                break;
            case 'h':
                usage();
                break;
            case '?':
                usage();
                break;
            case 'v':
                print_version();
                break;
            case 'O':
                opt->overwrite++;
            case 'o':
                opt->target = optarg;
                break;
            case 'r':
                opt->restore++;
                mode++;
                break;
            case 'c':
                opt->clone++;
                mode++;
                break;
            case 'b':
                opt->dd++;
                mode++;
                break;
            case 'd':
                if (optarg)
                    opt->debug = atol(optarg);
                else
                    opt->debug = 1;
                break;
            case 'L': 
                opt->logfile = optarg;
                break;
            case 'f':
                opt->fresh = atol(optarg);
                break;
            case 'F':
                opt->force++;
                break;
            case 'I':
                opt->ignore_fschk++;
                break;
            case 'i':
                opt->ignore_crc = 1;
                break;

            case 'R':
                opt->rescue++;
                break;
            case 'X':
                /// output message as dialog format, reference
                /// dialog --guage is text height width percent
                ///    A guage box displays a meter along the bottom of the box. The meter indicates the percentage. New percentages are read from standard input, one integer per line. The meter is updated to reflect each new percentage. If stdin is XXX, then the first line following is taken as an integer percentage, then subsequent lines up to another XXX are used for a new prompt. The guage exits when EOF is reached on stdin. 
                opt->dialog = 1;
                break;
#ifdef HAVE_LIBNCURSESW
            case 'N':
                opt->ncurses = 1;
                break;
#endif
            case 'C':
                opt->check = 0;
                break;
            default:
                fprintf(stderr, "Unknown option '%s'.\n", argv[optind-1]);
                usage();
        }
    }

    if(mode != 1) {
        //fprintf(stderr, ".\n")
        usage();
    }

    if (!opt->debug){
        opt->debug = 0;
    }
    //printf("debug %i\n", opt->debug);

    if ((opt->target == NULL) && (opt->source == NULL)){
        fprintf(stderr, "There is no image name. or --help get more info.\n");
        exit(0);
    }

    if (opt->target == NULL) {
        //fprintf(stderr, "You use specify output file like stdout. or --help get more info.\n");
        opt->target = "-";
    }

    if (opt->source == NULL) {
        //fprintf(stderr, "You use specify output file like stdout. or --help get more info.\n");
        opt->source = "-";
    }

    if (opt->clone){

        if ((strcmp(opt->source, "-") == 0) || (opt->source == NULL)) {
            fprintf(stderr, "Partclone can't clone from stdin.\nFor help, type: %s -h\n", EXECNAME);
            //usage();
            exit(0);
        }

        if ((strcmp(opt->target, "-") == 0) || (opt->target == NULL)) {
            if (opt->ncurses){
                fprintf(stderr, "Warning: Partclone can't save output to stdout with ncurses interface.\n");
                opt->ncurses = 0;
            }

        }

    }

    if (opt->restore){

        if ((strcmp(opt->target, "-") == 0) || (opt->target == NULL)) {
            fprintf(stderr, "Partclone can't restore to stdout.\nFor help,type: %s -h\n", EXECNAME);
            //usage();
            exit(0);
        }

    }

}


/**
 * Ncurses Text User Interface
 * open_ncurses	    - open text window
 * close_ncurses    - close text window
 */
extern int open_ncurses(){
    int debug = 1;

#ifdef HAVE_LIBNCURSESW
    extern cmd_opt opt;
    int terminal_x = 0;
    int terminal_y = 0;
    initscr();

    // check terminal width and height
    getmaxyx(stdscr, terminal_y, terminal_x);

    // set window position
    int log_line = 10;
    int log_row = 60;
    int log_y_pos = (terminal_y-24)/2+2;
    int log_x_pos = (terminal_x-log_row)/2;
    int gap = 2;
    int p_line = 8;
    int p_row = log_row;
    int p_y_pos = log_y_pos+log_line+gap;
    int p_x_pos = log_x_pos;

    int size_ok = 1;

    if(terminal_y < (log_line+gap+p_line+3))
        size_ok = 0;
    if(terminal_x < (log_row+2))
        size_ok = 0;

    if (size_ok == 0){
        log_mesg(0, 0, 0, debug, "Terminal width(%i) or height(%i) too small\n", terminal_x, terminal_y);
        return 0;
    }

    /// check color pair
    if(!has_colors()){
        log_mesg(0, 0, 0, debug, "Terminal color error\n");
        return 0;
    }

    if (start_color() != OK){
        log_mesg(0, 0, 0, debug, "Terminal can't start color mode\n");
        return 0;
    }

    /// define color
    init_pair(1, COLOR_WHITE, COLOR_BLUE); ///stdscr
    init_pair(2, COLOR_RED, COLOR_WHITE); ///sub window
    init_pair(3, COLOR_BLUE, COLOR_WHITE); ///sub window

    /// write background color
    bkgd(COLOR_PAIR(1));
    touchwin(stdscr);
    refresh();

    /// init main box
    attrset(COLOR_PAIR(2));
    box_win = subwin(stdscr, (log_line+gap+p_line+2), log_row+2, log_y_pos-1, log_x_pos-1);
    box(box_win, ACS_VLINE, ACS_HLINE);
    wbkgd(box_win, COLOR_PAIR(2));
    mvprintw((log_y_pos-1), ((terminal_x-9)/2), " Partclone ");
    attroff(COLOR_PAIR(2));

    attrset(COLOR_PAIR(3));

    /// init log window
    log_win = subwin(stdscr, log_line, log_row, log_y_pos, log_x_pos);
    wbkgd(log_win, COLOR_PAIR(3));
    //wprintw(log_win, "Calculating bitmap...\nPlease wait...\n");

    // init progress window
    p_win = subwin(stdscr, p_line, p_row, p_y_pos, p_x_pos);
    wbkgd(p_win, COLOR_PAIR(3));
    mvwprintw(p_win, 5, 52, "  0.00%%\n");

    // init progress window
    bar_win = subwin(stdscr, 1, p_row-10, p_y_pos+5, p_x_pos);
    wbkgd(bar_win, COLOR_PAIR(1));

    scrollok(log_win, TRUE);

    if( touchwin(stdscr) == ERR ){
        return 0;
    }

    //clear();

    refresh();

#endif
    return 1;
}

extern void close_ncurses(){
#ifdef HAVE_LIBNCURSESW
    sleep(3);
    attroff(COLOR_PAIR(3));
    delwin(log_win);
    delwin(p_win);
    delwin(bar_win);
    delwin(box_win);
    touchwin(stdscr);
    endwin();
#endif
}


/**
 * debug message
 * open_log	- to open file default is /var/log/partclone.log
 * log_mesg	- write log to the file
 *		- write log and exit 
 *		- write to stderr...
 * close_log	- to close file /var/log/partclone.log
 */
extern void open_log(char* source){
    msg = fopen(source,"w");
    if(msg == NULL){
        fprintf(stderr, "open logfile %s error\n", source);
        exit(0);
    }
}

extern void log_mesg(int log_level, int log_exit, int log_stderr, int debug, const char *fmt, ...){

    va_list args;
    va_start(args, fmt);
    extern cmd_opt opt;
    extern p_dialog_mesg m_dialog;
    char tmp_str[512];

    vsprintf(tmp_str, fmt, args);
    if (opt.ncurses) {
#ifdef HAVE_LIBNCURSESW
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
        if((log_stderr) && (log_level <= debug)){
            if(log_exit){
                wattron(log_win, A_STANDOUT);
            }

            wprintw(log_win, tmp_str);


            if(log_exit){
                wattroff(log_win, A_STANDOUT);
                sleep(3);
            }
            wrefresh(log_win);
            log_y_line++;
        }
#endif
    } else if (opt.dialog) {
        /// write log to stderr if log_stderr is true
        if((log_stderr) && (log_level <= debug)){
            //vsprintf(tmp_str, fmt, args);
            strncat(m_dialog.data, tmp_str, strlen(tmp_str));
            fprintf(stderr, "XXX\n%i\n%s\nXXX\n", m_dialog.percent, m_dialog.data);
        }
    } else {
        /// write log to stderr if log_stderr is true
        if((log_stderr == 1) && (log_level <= debug)){
            //vsprintf(tmp_str, fmt, args);
            fprintf(stderr, "%s", tmp_str);
        }
    }

    /// write log to logfile if debug is true
    if(log_level <= debug){
        //vsprintf(tmp_str, fmt, args);
        fprintf(msg, "%s", tmp_str);
        //if (errno != 0)
        //    fprintf(msg, "%s(%i), ", strerror(errno), errno);
    }
    va_end(args);

    /// clear message
    fflush(msg);

    /// exit if lexit true
    if ((!opt.force) && (log_exit)){
        close_ncurses();
        fprintf(stderr, "Partclone fail, please check %s !\n", opt.logfile);
        exit(1);
    }
}

extern void close_log(){
    fclose(msg);
}

/**
 * for restore used functions
 * restore_image_hdr	- get image_head from image file 
 * restore_image_hdr_sp	- get image_head from image file 
 * get_image_bitmap	- read bitmap data from image file
 */
extern void restore_image_hdr(int* ret, cmd_opt* opt, image_head* image_hdr){
    int r_size;
    char* buffer;
    unsigned long long dev_size;
    int debug = opt->debug;

    buffer = (char*)malloc(sizeof(image_head));
    //r_size = read(*ret, buffer, sizeof(image_head));
    r_size = read_all(ret, buffer, sizeof(image_head), opt);
    if (r_size == -1)
        log_mesg(0, 1, 1, debug, "read image_hdr error\n");
    memcpy(image_hdr, buffer, sizeof(image_head));
    free(buffer);
    dev_size = (unsigned long long)(image_hdr->totalblock * image_hdr->block_size);
    if (image_hdr->device_size != dev_size)
        image_hdr->device_size = dev_size;
}
extern void restore_image_hdr_sp(int* ret, cmd_opt* opt, image_head* image_hdr, char* first_sec){
    int r_size;
    char* buffer;
    unsigned long long dev_size;
    int debug = opt->debug;

    buffer = (char*)malloc(sizeof(image_head));
    //r_size = read(*ret, buffer, sizeof(image_head));
    memcpy(buffer, first_sec, 512);
    r_size = read_all(ret, (buffer+512), (sizeof(image_head)-512), opt);
    if (r_size == -1)
        log_mesg(0, 1, 1, debug, "read image_hdr error\n");
    memcpy(image_hdr, buffer, sizeof(image_head));
    free(buffer);
    dev_size = (unsigned long long)(image_hdr->totalblock * image_hdr->block_size);
    if (image_hdr->device_size != dev_size)
        image_hdr->device_size = dev_size;
}
/// get partition size
extern unsigned long long get_partition_size(int* ret){

    unsigned long long dest_size = 0;
    unsigned long dest_block;
    struct stat stat;
    int debug = 1;

    if (!fstat(*ret, &stat)) {
        if (S_ISFIFO(stat.st_mode)){
            dest_size = 0;
        } else if (S_ISREG(stat.st_mode)){
            dest_size = stat.st_size;
        } else {

#ifdef BLKGETSIZE64
            if (ioctl(*ret, BLKGETSIZE64, &dest_size) < 0) {
                log_mesg(0, 0, 0, debug, "get device size error, Use option -C to disable size checking(Dangerous).\n");
            }
            log_mesg(1, 0, 0, debug, "get device size %lli by ioctl BLKGETSIZE64,\n", dest_size);
            return dest_size;
#endif

#ifdef BLKGETSIZE
            if (ioctl(*ret, BLKGETSIZE, &dest_block) >= 0) {
                dest_size = (unsigned long long)(dest_block * 512);
            }
            log_mesg(1, 0, 0, debug, "get block %li and device size %lli by ioctl BLKGETSIZE,\n", dest_block, dest_size);
            return dest_size;
#endif
        }
    } else {
        log_mesg(0, 0, 0, debug, "fstat size error, Use option -C to disable size checking(Dangerous).\n");

    }

    return dest_size;

}
/// check partition size
extern int check_size(int* ret, unsigned long long size){

    unsigned long long dest_size;
    int debug = 1;

    dest_size = get_partition_size(ret);
    if (dest_size < size){
        log_mesg(0, 1, 1, debug, "Target partition size(%lli MB) is smaller than source(%lli MB). Use option -C to disable size checking(Dangerous).\n", print_size(dest_size, MBYTE), print_size(size, MBYTE));
        return 1;
    }

    return 0;

}

/// check free space 
extern void check_free_space(int* ret, unsigned long long size){

    unsigned long long dest_size;
    struct statvfs stvfs;
    struct stat stat;
    int debug = 1;

    if (fstatvfs(*ret, &stvfs) == -1) {
        printf("WARNING: Unknown free space on the destination: %s\n",
                strerror(errno));
        return;
    }
    /* if file is a FIFO there is no point in checking the size */
    if (!fstat(*ret, &stat)) {
        if (S_ISFIFO(stat.st_mode))
            return;
    } else {
        printf("WARNING: Couldn't get file info because of the following error: %s\n",
                strerror(errno));
    }

    dest_size = (unsigned long long)stvfs.f_frsize * stvfs.f_bfree;
    if (!dest_size)
        dest_size = (unsigned long long)stvfs.f_bsize * stvfs.f_bfree;

    if (dest_size < size)
        log_mesg(0, 1, 1, debug, "Destination doesn't have enough free space: %llu MB < %llu MB\n", print_size(dest_size, MBYTE), print_size(size, MBYTE));
}

/// check free memory size
extern int check_mem_size(image_head image_hdr, cmd_opt opt, unsigned long long *mem_size){
    unsigned long long image_head_size = 0;
    unsigned long long bitmap_size = 0;
    int crc_io_size = 0;
    void *test_mem;

    image_head_size = sizeof(image_head);
    bitmap_size = (sizeof(char)*image_hdr.totalblock);
    crc_io_size = sizeof(unsigned long)+image_hdr.block_size;
    *mem_size = image_head_size + bitmap_size + crc_io_size;

    test_mem = malloc(*mem_size);
    if (test_mem == NULL){
        free(test_mem);
        return -1;
    } else {
        free(test_mem);
    }
    return 1;
}

/// get bitmap from image file to restore data
extern void get_image_bitmap(int* ret, cmd_opt opt, image_head image_hdr, char* bitmap){
    int size, r_size;
    int do_write = 0;
    char* buffer;
    unsigned long long block_id;
    unsigned long long bused = 0, bfree = 0;
    int debug = opt.debug;
    int err_exit = 1;

    size = sizeof(char)*image_hdr.totalblock;
    buffer = (char*)malloc(size);

    r_size = read_all(ret, buffer, size, &opt);
    memcpy(bitmap, buffer, size);

    free(buffer);

    for (block_id = 0; block_id < image_hdr.totalblock; block_id++){
        if(bitmap[block_id] == 1){
            //printf("u = %i\n",block_id);
            bused++;
        } else {
            //printf("n = %i\n",block_id);
            bfree++;
        }
    }
    if (debug >= 2) {
        if(image_hdr.usedblocks != bused){
            if (opt.force)
                err_exit = 0;
            else
                err_exit = 1;
            log_mesg(0, err_exit, 1, debug, "The Used Block count is different.(bitmap %lli != image_head %lli)\nTry to use --force to skip the metadata error.\n", bused, image_hdr.usedblocks);
        }	
    } 
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

extern int check_mount(const char* device, char* mount_p){

    char *real_file = NULL, *real_fsname = NULL;
    FILE * f;
    struct mntent * mnt;
    int isMounted = 0;
    int err = 0;

    real_file = malloc(PATH_MAX + 1);
    if (!real_file){
	free(real_file);
        return -1;
    }

    real_fsname = malloc(PATH_MAX + 1);
    if (!real_fsname){
	free(real_fsname);
        return -1;
        //err = errno;
    }

    if (!realpath(device, real_file))
        err = errno;

    if ((f = setmntent (MOUNTED, "r")) == 0){
	free(real_file);
	free(real_fsname);
        return -1;
    }

    while ((mnt = getmntent (f)) != 0)
    {
        if (!realpath(mnt->mnt_fsname, real_fsname))
            continue;
        if (strcmp(real_file, real_fsname) == 0)
        {
            isMounted = 1;
            strcpy(mount_p, mnt->mnt_dir);
        }
    }
    endmntent (f);

    free(real_file);
    free(real_fsname);
    return isMounted;
}

extern int open_source(char* source, cmd_opt* opt){
    int ret;
    int debug = opt->debug;
    char *mp;
    int flags = O_RDONLY | O_LARGEFILE;

    log_mesg(1, 0, 0, debug, "open source file/device %s\n", source);
    if((opt->clone) || (opt->dd)){ /// always is device, clone from device=source

        mp = malloc(PATH_MAX + 1);
        if (check_mount(source, mp) == 1){
            log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", source, mp);
	    free(mp);
            log_mesg(0, 1, 1, debug, "error exit\n");
	}
	free(mp);

        ret = open(source, flags, S_IRUSR);
        if (ret == -1)
            log_mesg(0, 1, 1, debug, "clone: open %s error\n", source);


    } else if(opt->restore) {

        if (strcmp(source, "-") == 0){ 
            ret = fileno(stdin); 
            if (ret == -1)
                log_mesg(0, 1, 1, debug,"restore: open %s(stdin) error\n", source);
        } else {
            ret = open (source, flags, S_IRWXU);
            if (ret == -1)
                log_mesg(0, 1, 1, debug, "restore: open %s error\n", source);
        }
    }

    return ret;
}

extern int open_target(char* target, cmd_opt* opt){
    int ret;
    int debug = opt->debug;
    char *mp;
    int flags = O_WRONLY | O_LARGEFILE;
    struct stat st_dev;

    log_mesg(1, 0, 0, debug, "open target file/device\n");
    if (opt->clone){
        if (strcmp(target, "-") == 0){ 
            ret = fileno(stdout);
            if (ret == -1)
                log_mesg(0, 1, 1, debug, "clone: open %s(stdout) error\n", target);
        } else { 
            flags |= O_CREAT;		    /// new file
            if (!opt->overwrite)	    /// overwrite
                flags |= O_EXCL;
            ret = open (target, flags, S_IRUSR);

            if (ret == -1){
                if (errno == EEXIST){
                    log_mesg(0, 0, 1, debug, "Output file '%s' already exists.\nUse option --overwrite if you want to replace its content.\n", target);
                }
                log_mesg(0, 0, 1, debug, "%s,%s,%i: open %s error(%i)\n", __FILE__, __func__, __LINE__, target, errno);
            }
        }
    } else if((opt->restore) || (opt->dd)){		    /// always is device, restore to device=target

        /// check mounted
        mp = malloc(PATH_MAX + 1);
        if (check_mount(target, mp) == 1){
            log_mesg(0, 0, 1, debug, "device (%s) is mounted at %s\n", target, mp);
	    free(mp);
            log_mesg(0, 1, 1, debug, "error exit\n");
	}
	free(mp);

        /// check block device
        stat(target, &st_dev);
        if (!S_ISBLK(st_dev.st_mode)){
            log_mesg(1, 0, 1, debug, "Warning, did you restore to non-block device(%s)?\n", target);
            flags |= O_CREAT;	        /// new file
            if (!opt->overwrite)        /// overwrite
                flags |= O_EXCL;
        }
        ret = open (target, flags, S_IRUSR);
        if (ret == -1){
            if (errno == EEXIST){
                log_mesg(0, 0, 1, debug, "Output file '%s' already exists.\nUse option --overwrite if you want to replace its content.\n", target);
            }
            log_mesg(0, 0, 1, debug, "%s,%s,%i: open %s error(%i)\n", __FILE__, __func__, __LINE__, target, errno);
        } 
    }
    return ret;
}

/// the io function, reference from ntfsprogs(ntfsclone).
extern int io_all(int *fd, char *buf, int count, int do_write, cmd_opt* opt)
{
    int i;
    int debug = opt->debug;
    int size = count;

    // for sync I/O buffer, when use stdin or pipe.
    while (count > 0) {
        if (do_write)
            i = write(*fd, buf, count);
        else
            i = read(*fd, buf, count);

        if (i < 0) {
            log_mesg(0, 0, 1, debug, "%s: errno = %s(%i)\n",__func__, strerror(errno), errno);
            if (errno != EAGAIN && errno != EINTR){
                return -1;
            }
        } else if (i == 0){
            log_mesg(1, 0, 0, debug, "%s: errno = %s(%i)\n",__func__, strerror(errno), errno);
            return 0;

        } else {
            count -= i;
            buf = i + (char *) buf;
            log_mesg(2, 0, 0, debug, "%s: read %li, %li left.\n",__func__, i, count);
        }
    }
    return size;
}

extern void sync_data(int fd, cmd_opt* opt)
{
    log_mesg(0, 0, 1, opt->debug, "Syncing... ");
    if (fsync(fd) && errno != EINVAL)
        log_mesg(0, 1, 1, opt->debug, "fsync error: errno = %i\n", errno);
    log_mesg(0, 0, 1, opt->debug, "OK!\n");
}

extern void rescue_sector(int *fd, unsigned long long pos, char *buff, cmd_opt *opt)
{
    const char *badsector_magic = "BADSECTOR\0";

    if (lseek(*fd, pos, SEEK_SET) == (off_t)-1){
        log_mesg(0, 0, 1, opt->debug, "WARNING: lseek error at %lli\n", pos);
        memset(buff, '?', SECTOR_SIZE);
        memmove(buff, badsector_magic, sizeof(badsector_magic));
	return;
    }

    if (io_all(fd, buff, SECTOR_SIZE, 0, opt) == -1) { /// read_all
        log_mesg(0, 0, 1, opt->debug, "WARNING: Can't read sector at %llu, lost data.\n", (unsigned long long)pos);
        memset(buff, '?', SECTOR_SIZE);
        memmove(buff, badsector_magic, sizeof(badsector_magic));
    }
}


/// the crc32 function, reference from libcrc. 
/// Author is Lammert Bies  1999-2007
/// Mail: info@lammertbies.nl
/// http://www.lammertbies.nl/comm/info/nl_crc-calculation.html 
/// generate crc32 code
extern unsigned long crc32(unsigned long crc, char *buf, int size){

    unsigned long crc_tab32[256];
    unsigned long init_crc, init_p;
    unsigned long tmp, long_c;
    int i, j, init = 0, s = 0 ;
    char c;
    init_p = 0xEDB88320L;

    do{
        memcpy(&c, buf, sizeof(char));
        s = s + sizeof(char);
        /// initial crc table
        if (init == 0){
            for (i=0; i<256; i++) {
                init_crc = (unsigned long) i;
                for (j=0; j<8; j++) {
                    if ( init_crc & 0x00000001L ) init_crc = ( init_crc >> 1 ) ^ init_p;
                    else                     init_crc =   init_crc >> 1;
                }
                crc_tab32[i] = init_crc;
            }
            init = 1;
        }

        /// update crc
        long_c = 0x000000ffL & (unsigned long) c;
        tmp = crc ^ long_c;
        crc = (crc >> 8) ^ crc_tab32[ tmp & 0xff ];
    }while(s < size);

    return crc;
}

/// print options to log file
extern void print_opt(cmd_opt opt){
    int debug = opt.debug;

    if (opt.clone)
        log_mesg(1, 0, 0, debug, "MODE: clone\n");
    else if (opt.restore)
        log_mesg(1, 0, 0, debug, "MODE: restore\n");
    else if (opt.dd)
        log_mesg(1, 0, 0, debug, "MODE: device to device\n");

    log_mesg(1, 0, 0, debug, "DEBUG: %i\n", opt.debug);
    log_mesg(1, 0, 0, debug, "SOURCE: %s\n", opt.source);
    log_mesg(1, 0, 0, debug, "TARGET: %s\n", opt.target);
    log_mesg(1, 0, 0, debug, "OVERWRITE: %i\n", opt.overwrite);
    log_mesg(1, 0, 0, debug, "RESCUE: %i\n", opt.rescue);
    log_mesg(1, 0, 0, debug, "CHECK: %i\n", opt.check);
    log_mesg(1, 0, 0, debug, "FRESH: %i\n", opt.fresh);
    log_mesg(1, 0, 0, debug, "FORCE: %i\n", opt.force);
#ifdef HAVE_LIBNCURSESW
    log_mesg(1, 0, 0, debug, "NCURSES: %i\n", opt.ncurses);
#endif
    log_mesg(1, 0, 0, debug, "DIALOG: %i\n", opt.dialog);

}

/// print partclone info
extern void print_partclone_info(cmd_opt opt){

    int debug = opt.debug;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    //log_mesg(0, 0, 0, "%s v%s \n", EXEC_NAME, VERSION);
    log_mesg(0, 0, 1, debug, _("Partclone v%s http://partclone.org\n"), VERSION);
    if (opt.chkimg)
        log_mesg(0, 0, 1, debug, _("Starting to check image (%s)\n"), opt.source);	
    else if (opt.clone)
        log_mesg(0, 0, 1, debug, _("Starting to clone device (%s) to image (%s)\n"), opt.source, opt.target);	
    else if(opt.restore)
        log_mesg(0, 0, 1, debug, _("Starting to restore image (%s) to device (%s)\n"), opt.source, opt.target);
    else if(opt.dd)
        log_mesg(0, 0, 1, debug, _("Starting to back up device(%s) to device(%s)\n"), opt.source, opt.target);
    else
        log_mesg(0, 0, 1, debug, "unknow mode\n");
}

/// print image head
extern void print_image_hdr_info(image_head image_hdr, cmd_opt opt){

    int block_s  = image_hdr.block_size;
    unsigned long long total    = image_hdr.totalblock;
    unsigned long long used     = image_hdr.usedblocks;
    unsigned long long dev_size = image_hdr.device_size;
    int debug = opt.debug;
    char size_str[11];

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    log_mesg(0, 0, 1, debug, _("File system:  %s\n"), image_hdr.fs);

    print_readable_size_str(total*block_s, size_str);
    log_mesg(0, 0, 1, debug, _("Device size:  %s\n"), size_str);
    
    print_readable_size_str(used*block_s, size_str);
    log_mesg(0, 0, 1, debug, _("Space in use: %s\n"), size_str);
    
    print_readable_size_str((total-used)*block_s, size_str);
    log_mesg(0, 0, 1, debug, _("Free Space:   %s\n"), size_str);
    
    log_mesg(0, 0, 1, debug, _("Block size:   %i Byte\n"), block_s);
    log_mesg(0, 0, 1, debug, _("Used block :  %lli\n"), used);
}

/// print finish message
extern void print_finish_info(cmd_opt opt){

    int debug = opt.debug;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    if (opt.chkimg)
        log_mesg(0, 0, 1, debug, _("Partclone successfully checkd the image (%s)\n"), opt.source);	
    else if (opt.clone)
        log_mesg(0, 0, 1, debug, _("Partclone successfully cloned the device (%s) to the image (%s)\n"), opt.source, opt.target);	
    else if(opt.restore)
        log_mesg(0, 0, 1, debug, _("Partclone successfully restored the image (%s) to the device (%s)\n"), opt.source, opt.target);
    else if(opt.dd)
        log_mesg(0, 0, 1, debug, _("Partclone successfully cloned the device (%s) to the device (%s)\n"), opt.source, opt.target);

}

/// block_count
static unsigned long long block_count(unsigned long long partition_size, int block_size){
    unsigned long long blocks;
    int sectors;
    blocks  = partition_size / block_size;
    sectors = partition_size % block_size;
    if (sectors)
        return (blocks+1);
    else
        return blocks;

}

/// initial dd hdr
extern void initial_dd_hdr(int ret, image_head* image_hdr){

    memset(image_hdr ,(int )NULL, sizeof(image_head));
    memcpy(image_hdr->fs, raw_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size  = 512*2;
    image_hdr->device_size = get_partition_size(&ret);
    image_hdr->totalblock  = block_count(image_hdr->device_size, image_hdr->block_size);
    image_hdr->usedblocks  = image_hdr->totalblock;
}

/// initial bitmap
extern void dd_bitmap(image_head image_hdr, char* bitmap){

    int block;

    /// initial image bitmap as 1 (all block are used)
    for(block = 0; block < image_hdr.totalblock; block++)
        bitmap[block] = 1; 

}

