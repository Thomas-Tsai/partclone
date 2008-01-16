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
#include "version.c"
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
    fprintf(stderr, "%s v%s (%s) http://partclone.sourceforge.net\nUsage: %s [OPTIONS]\n"
        "    Efficiently clone to a image, device or standard output.\n"
        "\n"
        "    -o, --output FILE      Output FILE\n"
	"    -O  --overwrite FILE   Output FILE, overwriting if exists\n"
	"    -s, --source FILE      Source FILE\n"
        "    -c, --clone            Save to the special image format\n"
        "    -r, --restore          Restore from the special image format\n"
	"    -b, --dd-mode          Save to sector-to-sector format\n"
        "    -d, --debug            Show debug information\n"
        "    -R, --rescue	    Continue after disk read errors\n"
        "    -h, --help             Display this help\n"
    , EXECNAME, VERSION, svn_version, EXECNAME);
    exit(0);
}

extern void parse_options(int argc, char **argv, cmd_opt* opt)
{
    static const char *sopt = "-hdcbro:O:s:R";
    static const struct option lopt[] = {
        { "help",		no_argument,	    NULL,   'h' },
        { "output",		required_argument,  NULL,   'o' },
	{ "overwrite",		required_argument,  NULL,   'O' },
        { "source",		required_argument,  NULL,   's' },
        { "restore-image",	no_argument,	    NULL,   'r' },
        { "clone-image",	no_argument,	    NULL,   'c' },
        { "dd-mode",		no_argument,	    NULL,   'b' },
        { "debug",		no_argument,	    NULL,   'd' },
        { "rescue",		no_argument,	    NULL,   'R' },
        { NULL,			0,		    NULL,    0  }
    };

    char c;
    int mode = 0;
    memset(opt, 0, sizeof(cmd_opt));

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
                    opt->debug++;
                    break;
	    case 'R':
		    opt->rescue++;
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
    //r_size = read(*ret, buffer, sizeof(image_head));
    r_size = read_all(ret, buffer, sizeof(image_head), opt);
    if (r_size == -1)
        log_mesg(0, 1, 1, debug, "read image_hdr error\n");
    memcpy(image_hdr, buffer, sizeof(image_head));
    free(buffer);
}

extern void check_size(int* ret, unsigned long long size){

    unsigned long long dest_size;
    long dest_block;
    int debug = 1;

    if (ioctl(*ret, BLKGETSIZE, &dest_block) >= 0)
	dest_size = (unsigned long long)(512*dest_block);

    if (dest_size < size)
	log_mesg(0, 1, 1, debug, "The dest partition size is small than original partition.(%lli<%lli)\n", dest_size, size);
}

extern void get_image_bitmap(int* ret, cmd_opt opt, image_head image_hdr, char* bitmap){
    int size, r_size;
    int do_write = 0;
    char* buffer;
    unsigned long long block_id;
    unsigned long bused = 0, bfree = 0;
    int debug = opt.debug;

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

extern int check_mount(const char* device, char* mount_p){

    char *real_file = NULL, *real_fsname = NULL;
    FILE * f;
    struct mntent * mnt;
    int isMounted = 0;
    int err = 0;

    real_file = malloc(PATH_MAX + 1);
    if (!real_file)
            return -1;

    real_fsname = malloc(PATH_MAX + 1);
    if (!real_fsname)
            err = errno;

    if (!realpath(device, real_file))
            err = errno;

    if ((f = setmntent (MOUNTED, "r")) == 0)
        return -1;

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

    return isMounted;
}

extern int open_source(char* source, cmd_opt* opt){
    int ret;
    int debug = opt->debug;
    char *mp = malloc(PATH_MAX + 1);
    int flags = O_RDONLY | O_LARGEFILE;

    if((opt->clone) || (opt->dd)){ /// always is device, clone from device=source

	if (check_mount(source, mp) == 1)
	    log_mesg(0, 1, 1, debug, "device (%s) is mounted at %s\n", source, mp);

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
    char *mp = malloc(PATH_MAX + 1);
    int flags = O_WRONLY | O_LARGEFILE;

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
		log_mesg(0, 0, 1, debug, "%s: open %s error(%i)\n", __func__, target, errno);
	    }
    	}
    } else if((opt->restore) || (opt->dd)){		    /// always is device, restore to device=target
	
	if (check_mount(target, mp) == 1)
	    log_mesg(0, 1, 1, debug, "device (%s) is mounted at %s\n", target, mp);

	ret = open (target, flags);
	if (ret == -1)
	    log_mesg(0, 1, 1, debug, "restore: open %s error\n", target);
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
	    if (errno != EAGAIN && errno != EINTR){
		log_mesg(0, 1, 1, debug, "%s: errno = %i\n",__func__, errno);
                return -1;
	    }
	    log_mesg(0, 1, 1, debug, "%s: errno = %i\n",__func__, errno);
        } else {
	    count -= i;
	    buf = i + (char *) buf;
	    log_mesg(0, 0, 0, debug, "%s: read %li, %li left.\n",__func__, i, count);
        }
    }
    return size;
}

extern void sync_data(int fd, cmd_opt* opt)
{
    log_mesg(0, 0, 1, opt->debug, "Syncing ...");
	if (fsync(fd) && errno != EINVAL)
	log_mesg(0, 1, 1, opt->debug, "fsync error: errno = %i\n", errno);
    log_mesg(0, 0, 1, opt->debug, "OK!\n");
}

extern void rescue_sector(int *fd, char *buff, cmd_opt *opt)
{
    const char *badsector_magic = "BADSECTOR\0";
    off_t pos = 0;

    pos = lseek(*fd, 0, SEEK_CUR);

    if (lseek(*fd, SECTOR_SIZE, SEEK_CUR) == (off_t)-1)
        log_mesg(0, 1, 1, opt->debug, "lseek error\n");

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
	log_mesg(0, 0, 1, debug, _("Partclone v%s (%s) http://partclone.sourceforge.net\n"), VERSION, svn_version);
    if (opt.clone)
		log_mesg(0, 0, 1, debug, _("Starting clone device (%s) to image (%s)\n"), opt.source, opt.target);	
	else if(opt.restore)
		log_mesg(0, 0, 1, debug, _("Starting restore image (%s) to device (%s)\n"), opt.source, opt.target);
	else if(opt.dd)
		log_mesg(0, 0, 1, debug, _("Starting back up device(%s) to device(%s)\n"), opt.source, opt.target);
	else
		log_mesg(0, 0, 1, debug, "unknow mode\n");
	log_mesg(0, 0, 1, debug, _("File system: %s\n"), image_hdr.fs);
	log_mesg(0, 0, 1, debug, _("Device size: %lli MB\n"), print_size((total*block_s), MBYTE));
	log_mesg(0, 0, 1, debug, _("Space in use: %lli MB\n"), print_size((used*block_s), MBYTE));
	log_mesg(0, 0, 1, debug, _("Block size: %i Byte\n"), block_s);
	log_mesg(0, 0, 1, debug, _("Used block count: %lli\n"), used);
}

/// print finish message
extern void print_finish_info(cmd_opt opt){
 
    int debug = opt.debug;

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    if (opt.clone)
		log_mesg(0, 0, 1, debug, _("Partclone successful clone device (%s) to image (%s)\n"), opt.source, opt.target);	
	else if(opt.restore)
		log_mesg(0, 0, 1, debug, _("Partclone successful restore image (%s) to device (%s)\n"), opt.source, opt.target);
}

