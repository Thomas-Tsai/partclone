/**
 * progress.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * progress bar
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
//#include <time.h>

// progress display mode
#define TEXT 0
#define NCURSES 1
#define DIALOG 2

/// the progress bar structure
struct progress_bar {
        int start;
	unsigned long long stop;
	unsigned long long total;
        unsigned long long resolution;
	int block_size;
	float rate;
	time_t initial_time;
	time_t resolution_time;
	time_t interval_time;
        float unit;
        float total_unit;
	int pui;
	int flag;
};
typedef struct progress_bar progress_bar;

struct prog_stat_t{
    char Eformated[12];
    char Rformated[12];
    float percent;
    float total_percent;
    float speed;
    char speed_unit[5];
    
};
typedef struct prog_stat_t prog_stat_t;

extern int open_pui(int pui, unsigned long res);
extern void close_pui(int pui);
extern void update_pui(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done);

/// initial progress bar
extern void progress_init(struct progress_bar *prog, int start, unsigned long long stop, unsigned long long total, int flag, int size);

/// update number
extern void progress_update(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done);
extern void Ncurses_progress_update(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done);
