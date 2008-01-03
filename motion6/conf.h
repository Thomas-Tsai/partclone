/*
  **
  ** conf.h - function prototypes for the config handling routines
  **
  ** Originally written for the dproxy package by Matthew Pratt.
  **
  ** Copyright 2000 Jeroen Vreeken (pe1rxq@chello.nl)
  **
  ** This software is licensed under the terms of the GNU General
  ** Public License (GPL). Please see the file COPYING for details.
  **
  **
*/

#ifndef _INCLUDE_CONF_H
#define _INCLUDE_CONF_H

/* 
	more parameters may be added later.
 */
struct config {
	int setup_mode;
	int width;
	int height;
	int quality;
	int rotate_deg;
	int max_changes;
	int threshold_tune;
	char *output_normal;
	int motion_img;
	int output_all;
	int gap;
	int maxmpegtime;
	int snapshot_interval;
	char *locate;
	int input;
	int norm;
	int frame_limit;
	int quiet;
	int ppm;
	int noise;
	int noise_tune;
	int mingap;
	int lightswitch;
	int nightcomp;
	int low_cpu;
	int nochild;
	int autobright;
	int brightness;
	int contrast;
	int saturation;
	int hue;
	int roundrobin_frames;
	int roundrobin_skip;
	int pre_capture;
	int post_capture;
	int switchfilter;
	int ffmpeg_cap_new;
	int ffmpeg_cap_motion;
	int ffmpeg_bps;
	int ffmpeg_vbr;
	char *ffmpeg_video_codec;
	int webcam_port;
	int webcam_quality;
	int webcam_motion;
	int webcam_maxrate;
	int webcam_localhost;
	int webcam_limit;
	int control_port;
	int control_localhost;
	int control_html_output;
	char *control_authentication;
	int frequency;
	int tuner_number;
	int timelapse;
	char *timelapse_mode; 
#ifdef __freebsd__
	char *tuner_device;
#endif
	char *video_device;
	char *vidpipe;
	char *filepath;
	char *jpegpath;
	char *mpegpath;
	char *snappath;
	char *timepath;
	char *on_event_start;
	char *on_event_end;
	char *mask_file;
	int smart_mask_speed;
	int sql_log_image;
	int sql_log_snapshot;
	int sql_log_mpeg;
	int sql_log_timelapse;
	char *mysql_db;
	char *mysql_host;
	char *mysql_user;
	char *mysql_password;
	char *on_picture_save;
	char *on_motion_detected;
	char *on_movie_start;
	char *on_movie_end;
	char *motionvidpipe;
	char *netcam_url;
	char *netcam_userpass;
	char *netcam_proxy;
	char *pgsql_db;
	char *pgsql_host;
	char *pgsql_user;
	char *pgsql_password;
	int pgsql_port;
	int text_changes;
	char *text_left;
	char *text_right;
	int text_double;
	char *despeckle;
	int minimum_motion_frames;
	// int debug_parameter;
	int argc;
	char **argv;
};

extern struct config conf;

/** 
 * typedef for a param copy function. 
 */
typedef struct context ** (* conf_copy_func)(struct context **, char *, int);
typedef char *(* conf_print_func)(struct context **, char **, int, int);

/**
 * description for parameters in the config file
 */
typedef struct {
	char * param_name;	/* name for this parameter             */
	char * param_help;	/* short explanation for parameter */
	int conf_value;	/* pointer to a field in struct context */
	conf_copy_func  copy;	/* a function to set the value in 'config' */
	conf_print_func print;	/* a function to output the value to a file */
} config_param; 


extern config_param config_params[];

struct context **conf_load (struct context **);
struct context **conf_cmdparse(struct context **, char *, char *);
char *config_type(config_param *);
void conf_print (struct context **);
void malloc_strings (struct context *);
char *mystrdup(char *);
char *mystrcpy(char *, char *);
struct context **copy_string(struct context **, char *, int);

#endif /* _INCLUDE_CONF_H */
