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
#include "config.h"
#include "progress.h"
#include "gettext.h"
#define _(STRING) gettext(STRING)

#ifdef HAVE_LIBNCURSESW
    #include <ncursesw/ncurses.h>
    WINDOW *progress_win;
    WINDOW *progress_box_win;
    int window_f = 0;
#endif

#include <partclone.h>
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
extern void Ncurses_progress_update(struct progress_bar *p, int current, int done)
{
#ifdef HAVE_LIBNCURSESW
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

	if(window_f == 0){
	    window_f = open_p_ncurses();
	}

	init_pair(1, COLOR_RED, COLOR_GREEN);
	init_pair(2, COLOR_GREEN, COLOR_RED);

        if (done != 1){
                if (((current - p->start) % p->resolution) && ((current != p->stop)))
                        return;
                mvwprintw(progress_win, 0, 0, _("Elapsed: %s") , Eformated);
                mvwprintw(progress_win, 1, 0, _("Remaining: %s"), Rformated);
                mvwprintw(progress_win, 2, 0, _("Rate: %6.2fMB/min"), (float)(speed));
                mvwprintw(progress_win, 3, 0, _("Completed:%6.2f%%"), percent);
		wattrset(progress_win, COLOR_PAIR(1));
		mvwprintw(progress_win, 5, 0, "%60s", " ");
		wattroff(progress_win, COLOR_PAIR(1));
		p_block = malloc(60);
		memset(p_block, 0, 60);
		memset(p_block, ' ', (size_t)(percent*0.6));
		wattrset(progress_win, COLOR_PAIR(2));
		mvwprintw(progress_win, 5, 0, "%s", p_block);
		wattroff(progress_win, COLOR_PAIR(2));
		wrefresh(progress_win);
		free(p_block);
        } else {
		total = elapsed;
		Ttm = gmtime(&total);
		strftime(Tformated, sizeof(Tformated), format, Ttm);
                mvwprintw(progress_win, 0, 0, _("Total Time: %s"), Tformated);
                mvwprintw(progress_win, 1, 0, _("Remaining: 0"));
                mvwprintw(progress_win, 2, 0, _("Ave. Rate: %6.1fMB/min"), (float)(p->rate/p->stop));
                mvwprintw(progress_win, 3, 0, _("100.00%% completed!"));
		wattrset(progress_win, COLOR_PAIR(1));
		mvwprintw(progress_win, 5, 0, "%60s", " ");
		wattroff(progress_win, COLOR_PAIR(1));
		wattrset(progress_win, COLOR_PAIR(2));
		mvwprintw(progress_win, 5, 0, "%60s", " ");
		wattroff(progress_win, COLOR_PAIR(2));
		wrefresh(progress_win);
		refresh();
		sleep(1);
	}

	if(done == 1){
	    window_f = close_p_ncurses();
	}

#endif
}

static int open_p_ncurses(){

#ifdef HAVE_LIBNCURSESW
    int p_line = 10;
    int p_row = 60;
    int p_y_pos = 15;
    int p_x_pos = 5;

    progress_box_win = subwin(stdscr, p_line+2, p_row+2, p_y_pos-1, p_x_pos-1);
    box(progress_box_win, ACS_VLINE, ACS_HLINE);
    progress_win = subwin(stdscr, p_line, p_row, p_y_pos, p_x_pos);

    touchwin(stdscr);
    refresh();
#endif

    return 1;
}

static int close_p_ncurses(){
#ifdef HAVE_LIBNCURSESW
    delwin(progress_win);
    delwin(progress_box_win);
    touchwin(stdscr);
#endif

    return 1;
}
extern void Dialog_progress_update(struct progress_bar *p, int current, int done){
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
	extern p_dialog_mesg m_dialog;
	
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
	char tmp_str[128];
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
		m_dialog.percent = (int)percent;
                sprintf(tmp_str, _("Elapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2fMB/min, "), Eformated, Rformated, percent, (float)(speed));
		fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
        } else {
		total = elapsed;
		Ttm = gmtime(&total);
		strftime(Tformated, sizeof(Tformated), format, Ttm);
		m_dialog.percent = 100;
                sprintf(tmp_str, _("\nTotal Time: %s, Ave. Rate: %6.1fMB/min, 100.00%% completed!\n"), Tformated, (float)(p->rate/p->stop));
		fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
	}

}
