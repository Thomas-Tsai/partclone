/*	video_freebsd.h
 *
 *	Include file for video_freebsd.c
 *      Copyright 2004 by Angel Carpintero (ack@telefonica.net)
 *      This software is distributed under the GNU public license version 2
 *      See also the file 'COPYING'.
 *
 */

#ifndef _INCLUDE_VIDEO_FREEBSD_H
#define _INCLUDE_VIDEO_FREEBSD_H

#ifndef WITHOUT_V4L

#include <machine/ioctl_meteor.h>
#include <machine/ioctl_bt848.h>

#endif

/* bktr (video4linux) stuff FIXME more modes not only these */

/* not used yet FIXME ! only needed for tuner use */
/*
#define TV_INPUT_NTSCM    BT848_IFORM_F_NTSCM 
#define TV_INPUT_NTSCJ    BT848_IFORM_F_NTSCJ
#define TV_INPUT_PALBDGHI BT848_IFORM_F_PALBDGHI
#define TV_INPUT_PALM     BT848_IFORM_F_PALM
#define TV_INPUT_PALN     BT848_IFORM_F_PALN
#define TV_INPUT_SECAM    BT848_IFORM_F_SECAM
#define TV_INPUT_PALNCOMB BT848_IFORM_F_RSVD
*/

#define NORM_DEFAULT	0x00800 // METEOR_FMT_AUTOMODE 
#define NORM_PAL	0x00200 // METEOR_FMT_PAL 
#define NORM_NTSC	0x00100 // METEOR_FMT_NTSC 
#define NORM_SECAM	0x00400 // METEOR_FMT_SECAM
#define NORM_PAL_NC	0x00200 // METEOR_FMT_PAL /* Greyscale howto ?! FIXME */

#define IN_DEFAULT      0 
#define IN_COMPOSITE    0
#define IN_TV           1
#define IN_COMPOSITE2   2
#define IN_SVIDEO       3


#define VIDEO_DEVICE "/dev/bktr0"
#define TUNER_DEVICE "/dev/turner0"


struct video_dev {
	int fd_bktr;
	int fd_tuner;
	char *video_device;
	char *tuner_device;
	int input;
	int width;
	int height;
	int contrast;
	int chroma;
	int brightness;
	int channel;
	int channelset;
	unsigned long freq;

	pthread_mutex_t mutex;
	pthread_mutexattr_t attr;
	int owner;
	int frames;
	
	/* Device type specific stuff: */
#ifndef WITHOUT_V4L	
	/* v4l */
	int v4l_fmt;
	int v4l_read_img;
	unsigned char *v4l_buffers[2];
	int v4l_curbuffer;
	int v4l_maxbuffer;
	int v4l_bufsize;
#endif
};

/* video functions, video_freebsd.c */
int vid_start(struct context *);
int vid_next(struct context *, char *map);

#ifndef WITHOUT_V4L
void vid_init(struct context *);
void vid_close(void);
void vid_cleanup(void);
#endif

/* Network camera functions, netcam.c */
int netcam_start(struct context *);
int netcam_next(struct context *, unsigned char *);

#endif /* _INCLUDE_VIDEO_FREEBSD_H */
