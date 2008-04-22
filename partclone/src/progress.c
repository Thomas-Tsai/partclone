/**
 * progress.c - part of Partclone project
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
#include <locale.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <ncurses.h>
#include "config.h"
#include "progress.h"
#include "gettext.h"
#define _(STRING) gettext(STRING)

/// initial progress bar
extern void progress_init(struct progress_bar *p, int start, int stop, int res, int size)
{
	time_t now;
	time(&now);
        p->start = start;
        p->stop = stop;
        p->unit = 100.0 / (stop - start);
	p->time = now;
	p->block_size = size;
        p->resolution = res;
	p->rate = 0.0;
}

/// update information at progress bar
extern void progress_update(struct progress_bar *p, int current, int done)
{
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
	
	float percent;
        double speedps = 1.0;
        float speed = 1.0;
        int display = 0;
        time_t remained;
	time_t elapsed;
	time_t total;
	char *format = "%H:%M:%S";
	char Rformated[10], Eformated[10], Tformated[10];
	struct tm *Rtm, *Etm, *Ttm;
	char *clear_buf = NULL;

        percent  = p->unit * current;
        elapsed  = (time(0) - p->time);
	if (elapsed <= 0)
	    elapsed = 1;
        speedps  = (float)p->block_size * (float)current / (float)(elapsed);
	//remained = (time_t)(p->block_size * (p->stop- current)/(int)speedps);
	remained = (time_t)((elapsed/percent*100) - elapsed);
	speed = (float)(speedps / 1000000.0 * 60.0);
	p->rate = p->rate+speed;

	/// format time string
	Rtm = gmtime(&remained);
	strftime(Rformated, sizeof(Rformated), format, Rtm);

	Etm = gmtime(&elapsed);
	strftime(Eformated, sizeof(Eformated), format, Etm);

        if (done != 1){
                if (((current - p->start) % p->resolution) && ((current != p->stop)))
                        return;
                fprintf(stderr, _("\r%81c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2fMB/min, "), clear_buf, Eformated, Rformated, percent, (float)(speed));
                /*
                fprintf(stderr, ("\r%81c\r"), clear_buf);
                fprintf(stderr, _("Elapsed: %s, "), Eformated);
                fprintf(stderr, _("Remaining: %s, "), Rformated);
                fprintf(stderr, _("Completed:%6.2f%%, "), percent);
                fprintf(stderr, _("Rate:%6.1fMB/min, "), (float)(p->rate));
                */
        } else {
		total = elapsed;
		Ttm = gmtime(&total);
		strftime(Tformated, sizeof(Tformated), format, Ttm);
                fprintf(stderr, _("\nTotal Time: %s, "), Tformated);
                fprintf(stderr, _("Ave. Rate: %6.1fMB/min, "), (float)(p->rate/p->stop));
                fprintf(stderr, _("100.00%% completed!\n"));
	}
}

/// update information at ncurses mode
extern void TUI_progress_update(struct progress_bar *p, int current, int done)
{
#ifdef HAVE_LIBNCURSES
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
	
	float percent;
        double speedps = 1.0;
        float speed = 1.0;
        int display = 0;
        time_t remained;
	time_t elapsed;
	time_t total;
	char *format = "%H:%M:%S";
	char Rformated[10], Eformated[10], Tformated[10];
	struct tm *Rtm, *Etm, *Ttm;
	char *clear_buf = NULL;
	char *p_block;

        percent  = p->unit * current;
        elapsed  = (time(0) - p->time);
	if (elapsed <= 0)
	    elapsed = 1;
        speedps  = (float)p->block_size * (float)current / (float)(elapsed);
	//remained = (time_t)(p->block_size * (p->stop- current)/(int)speedps);
	remained = (time_t)((elapsed/percent*100) - elapsed);
	speed = (float)(speedps / 1000000.0 * 60.0);
	p->rate = p->rate+speed;

	/// format time string
	Rtm = gmtime(&remained);
	strftime(Rformated, sizeof(Rformated), format, Rtm);

	Etm = gmtime(&elapsed);
	strftime(Eformated, sizeof(Eformated), format, Etm);

	/// check color pair
	if(!has_colors()){
	    endwin();
	    //printf("error, no colors\n");
	}

	if (start_color() != OK){
	    endwin();
	    //printf("error, no init color\n");
	}

	init_pair(1, COLOR_WHITE, COLOR_BLACK);
	init_pair(2, COLOR_WHITE, COLOR_RED);
	init_pair(3, COLOR_WHITE, COLOR_GREEN);

        if (done != 1){
                if (((current - p->start) % p->resolution) && ((current != p->stop)))
                        return;
                mvprintw(5, 10, "Elapsed: %s" , Eformated);
                mvprintw(7, 10, "Remaining: %s", Rformated);
                mvprintw(9, 10, "Rate: %6.2fMB/min", (float)(speed));
                mvprintw(11, 10, "Completed:%6.2f%%", percent);
		attrset(COLOR_PAIR(2));
		mvprintw(15, 10, "%60s", " ");
		attrset(COLOR_PAIR(3));
		p_block = malloc(60);
		memset(p_block, 0, 60);
		memset(p_block, ' ', (size_t)(percent*0.6));
		mvprintw(15, 10, "%s", p_block);
		free(p_block);
		attrset(COLOR_PAIR(1));
		refresh();
        } else {
		total = elapsed;
		Ttm = gmtime(&total);
		strftime(Tformated, sizeof(Tformated), format, Ttm);
                mvprintw(5, 10, "Total Time: %s", Tformated);
                mvprintw(7, 10, "Remaining: 0");
                mvprintw(9, 10, "Ave. Rate: %6.1fMB/min", (float)(p->rate/p->stop));
                mvprintw(11, 10, "100.00%% completed!");
		attrset(COLOR_PAIR(2));
		mvprintw(15, 10, "%60s", " ");
		attrset(COLOR_PAIR(3));
		mvprintw(15, 10, "%60s", " ");
		attrset(COLOR_PAIR(1));
		refresh();
		sleep(1);
	}

#endif
}
