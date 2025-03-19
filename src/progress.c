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
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "progress.h"
#include "gettext.h"
#define _(STRING) gettext(STRING)
#include "partclone.h"

#ifdef HAVE_LIBNCURSESW
#include <ncurses.h>
extern WINDOW *p_win;
extern WINDOW *bar_win;
extern WINDOW *tbar_win;
int color_support = 1;
int BUFSIZE = 50;
#endif

#define PUI_DEBUG 1

int PUI;
unsigned long RES=0;

/// initial progress bar
extern void progress_init(struct progress_bar *prog, int start, unsigned long long stop, unsigned long long total, int flag, int size)
{
    memset(prog, 0, sizeof(progress_bar));
    prog->start = start;
    prog->stop = stop;
    prog->total = total;
    prog->binary_prefix = 0;
    prog->prog_second = 0;
    strncpy(prog->time_unit, "min", 4);

    prog->unit = 100.0 / (stop - start);
    prog->total_unit = 100.0 / (total - start);

    if (!isnormal(prog->unit) || !isnormal(prog->total_unit)){
        prog->unit = 0;
        prog->total_unit = 0;
    }

    prog->initial_time = time(0);
    prog->resolution_time = time(0);
    prog->interval_time = 1;
    prog->block_size = size;
    if (RES){
        prog->interval_time = RES;
    }
    prog->rate = 0.0;
    prog->pui = PUI;
    prog->flag = flag;
}

/// open progress interface
extern int open_pui(int pui, unsigned long res){
    int tui = 0;
    log_mesg(1, 0, 0, PUI_DEBUG, "Opening User Interface mode.\n");
    if (pui == NCURSES){
        tui = open_ncurses();
        if (tui == 0){
            close_ncurses();
	    PUI = TEXT;
	    pui = TEXT;
        }
    } else if (pui == DIALOG){
        tui = 1;
    }
    PUI = pui;
    RES = res;
    return tui;
}

/// close progress interface
extern void close_pui(int pui){
    if (pui == NCURSES){
        close_ncurses();
    }
}

extern void update_pui(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done){


    if (done != 1) {
	if ((difftime(time(0), prog->resolution_time) < prog->interval_time) && copied != 0)
	    return;
    }
    if (prog->pui == NCURSES)
        Ncurses_progress_update(prog, copied, current, done);
    else if (prog->pui == TEXT)
        progress_update(prog, copied, current, done);
}

static void calculate_speed(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done, prog_stat_t *prog_stat){
    char *format = "%H:%M:%S";
    uint64_t speedps = 1;
    uint64_t speed = 1;
    double dspeed = 1.0;
    float percent = 1.0;
    time_t remained;
    time_t elapsed;
    char Rformated[12], Eformated[12];
    struct tm *Rtm, *Etm;
    uint64_t gibyte = UINT64_C(1) << 30;
    uint64_t mibyte = UINT64_C(1) << 20;
    uint64_t kibyte = UINT64_C(1) << 10;
    uint64_t gbyte = UINT64_C(1000000000);
    uint64_t mbyte = UINT64_C(1000000);
    uint64_t kbyte = UINT64_C(1000);
    int prefered_bits_size = prog->binary_prefix;

    percent  = prog->unit * copied;
    if (percent <= 0)
	percent = 1;
    else if (percent >= 100)
	percent = 99.99;

    elapsed  = (time(0) - prog->initial_time);
    if (elapsed <= 0)
	elapsed = 1;

    speedps  = prog->block_size * copied / elapsed;
    if (prog->prog_second)
        speed = speedps;
    else
        speed = speedps * 60.0;

    prog_stat->percent = percent;
    if ( prefered_bits_size ){

        if (speed >= gibyte){
            dspeed = (double)speed / (double)gibyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "GiB");
        }else if (speed >= mibyte){
            dspeed = (double)speed / (double)mibyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "MiB");
        }else if (speed >= kbyte){
            dspeed = (double)speed / (double)kibyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "KiB");
        }else{
            dspeed = speed;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "byte");
        }

    } else {

        if (speed >= gbyte){
            dspeed = (double)speed / (double)gbyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "GB");
        }else if (speed >= mbyte){
            dspeed = (double)speed / (double)mbyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "MB");
        }else if (speed >= kbyte){
            dspeed = (double)speed / (double)kbyte;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "KB");
        }else{
            dspeed = speed;
            snprintf(prog_stat->speed_unit, sizeof(prog_stat->speed_unit), "%s", "byte");
        }
    }

    prog_stat->total_percent = prog->total_unit * current;

    prog_stat->speed     = dspeed;

    if (done != 1){
        remained = (time_t)((elapsed/percent*100) - elapsed);
	if ((unsigned int)remained > 86400){
	    //spflen = snprintf(NULL, 0, " > %3i hrs ", ((int)remained/3600));
	    //snprintf(Rformated, spflen, " > %3i hrs ", ((int)remained/3600));
	    snprintf(Rformated, sizeof(Rformated), " > %3d hrs", ((int)remained/3600%1000));
	}else{
	    Rtm = gmtime(&remained);
	    strftime(Rformated, sizeof(Rformated), format, Rtm);
	}

	if ((unsigned int)elapsed > 86400){
	    //spflen = snprintf(NULL, 0, " > %3i hrs ", ((int)elapsed/3600));
	    //snprintf(Eformated, spflen, " > %3i hrs ", ((int)elapsed/3600));
	    snprintf(Eformated, sizeof(Eformated), " > %3d hrs", ((int)elapsed/3600%1000));
	}else{
	    Etm = gmtime(&elapsed);
	    strftime(Eformated, sizeof(Eformated), format, Etm);
	}

    } else {
        prog_stat->percent=100;
        remained = (time_t)0;
        Rtm = gmtime(&remained);
	strftime(Rformated, sizeof(Rformated), format, Rtm);

	if ((unsigned int)elapsed > 86400){
	    //spflen = snprintf(NULL, 0, " > %3i hrs ", ((int)elapsed/3600));
	    //snprintf(Eformated, spflen, " > %3i hrs ", ((int)elapsed/3600));
	    snprintf(Eformated, sizeof(Eformated), " > %3d hrs", ((int)elapsed/3600%1000));
	}else{
	    Etm = gmtime(&elapsed);
	    strftime(Eformated, sizeof(Eformated), format, Etm);
	}
    }

    strncpy(prog_stat->Eformated, Eformated, sizeof(Eformated)+1);
    strncpy(prog_stat->Rformated, Rformated, sizeof(Rformated)+1);
}

/// update information at progress bar
extern void progress_update(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done)
{
    char clear_buf = ' ';
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, copied, current, done, &prog_stat);

    if (done != 1){
	prog->resolution_time = time(0);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed: %6.2f%%"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);

	if((prog->flag == IO) || (prog->flag == NO_BLOCK_DETAIL))
	    fprintf(stderr, _(", %6.2f%s/%s,"), prog_stat.speed, prog_stat.speed_unit, prog->time_unit);
	if(prog->flag == IO)
	    fprintf(stderr, _("\n\r%80c\rCurrent block: %10Lu, Total block: %10Lu, Complete: %6.2f%%%s\r"), clear_buf, current, prog->total, prog_stat.total_percent, "\x1b[A");
    } else {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed: 100.00%%"), clear_buf, prog_stat.Eformated, prog_stat.Rformated);
	if((prog->flag == IO) || (prog->flag == NO_BLOCK_DETAIL))
	    fprintf(stderr, _(", Rate: %6.2f%s/%s,"), prog_stat.speed, prog_stat.speed_unit, prog->time_unit);
	if(prog->flag == IO)
	    fprintf(stderr, _("\n\r%80c\rCurrent block: %10Lu, Total block: %10Lu, Complete: 100.00%%\r"), clear_buf, current, prog->total);

        fprintf(stderr, _("\nTotal Time: %s, "), prog_stat.Eformated);
	if(prog->flag == IO)
	    fprintf(stderr, _("Ave. Rate: %6.2f%s/%s, "), prog_stat.speed, prog_stat.speed_unit, prog->time_unit);
        fprintf(stderr, _("%s"), "100.00% completed!\n");

        log_mesg(1, 0, 0, PUI_DEBUG, "Total Time: %s, Ave. Rate: %6.2f%s/%s, %s\n", prog_stat.Eformated, prog_stat.speed, prog_stat.speed_unit, prog->time_unit, "100.00% completed!");
    }
}

/// update information at ncurses mode
extern void Ncurses_progress_update(struct progress_bar *prog, unsigned long long copied, unsigned long long current, int done)
{
#ifdef HAVE_LIBNCURSESW

    char *block = "                                                                    ";
    int x = 0;
    char blockbuf[BUFSIZE];
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, copied, current, done, &prog_stat);

    /// set bar color
    init_pair(4, COLOR_RED, COLOR_RED);
    init_pair(5, COLOR_WHITE, COLOR_BLUE);
    init_pair(6, COLOR_WHITE, COLOR_RED);
    werase(p_win);
    werase(bar_win);

    if (done != 1){
	prog->resolution_time = time(0);

        mvwprintw(p_win, 0, 0, _("Elapsed: %s Remaining: %s ") , prog_stat.Eformated, prog_stat.Rformated);
	if((prog->flag == IO) || (prog->flag == NO_BLOCK_DETAIL))
	    mvwprintw(p_win, 0, 40, _("Rate: %6.2f%s/%s"), prog_stat.speed, prog_stat.speed_unit, prog->time_unit);
	if (prog->flag == IO)
	    mvwprintw(p_win, 1, 0, _("Current Block: %llu  Total Block: %llu ") , current, prog->total);

	if (prog->flag == IO)
	    mvwprintw(p_win, 3, 0, _("Data Block Process:"));
	else if (prog->flag == BITMAP)
	    mvwprintw(p_win, 3, 0, _("Calculating Bitmap Process:"));
	wattrset(bar_win, COLOR_PAIR(4));
	x = snprintf(blockbuf, BUFSIZE, "%.*s",  (unsigned int)(prog_stat.percent*0.5), block);
	if ( x < 0 )
	    fprintf(stderr, _("ncurses update error\n"));
        mvwprintw(bar_win, 0, 0, "%s", blockbuf);
        wattroff(bar_win, COLOR_PAIR(4));
        mvwprintw(p_win, 4, 52, "%6.2f%%", prog_stat.percent);

	if (prog->flag == IO) {
	    werase(tbar_win);
	    mvwprintw(p_win, 6, 0, _("Total Block Process:"));
	    wattrset(tbar_win, COLOR_PAIR(4));
	    x = snprintf(blockbuf, BUFSIZE, "%.*s",  (unsigned int)(prog_stat.total_percent*0.5), block);
	    if ( x < 0 )
		fprintf(stderr, _("ncurses update error\n"));
	    mvwprintw(tbar_win, 0, 0, "%s", blockbuf);
	    wattroff(tbar_win, COLOR_PAIR(4));
	    mvwprintw(p_win, 7, 52, "%6.2f%%", prog_stat.total_percent);
	}


	wrefresh(p_win);
        wrefresh(bar_win);
        wrefresh(tbar_win);
    } else {
        mvwprintw(p_win, 0, 0, _("Total Time: %s Remaining: %s "), prog_stat.Eformated, prog_stat.Rformated);
	if((prog->flag == IO) || (prog->flag == NO_BLOCK_DETAIL))
	    mvwprintw(p_win, 1, 0, _("Ave. Rate: %6.2f%s/%s"), prog_stat.speed, prog_stat.speed_unit, prog->time_unit);

	if (prog->flag == IO)
	    mvwprintw(p_win, 3, 0, _("Data Block Process:"));
	else if (prog->flag == BITMAP)
	    mvwprintw(p_win, 3, 0, _("Calculating Bitmap Process:"));
        wattrset(bar_win, COLOR_PAIR(4));
	mvwprintw(bar_win, 0, 0, "%50s", " ");
        wattroff(bar_win, COLOR_PAIR(4));
        mvwprintw(p_win, 4, 52, "100.00%%");

	if (prog->flag == IO) {
	    werase(tbar_win);
	    mvwprintw(p_win, 6, 0, _("Total Block Process:"));
	    wattrset(tbar_win, COLOR_PAIR(4));
	    mvwprintw(tbar_win, 0, 0, "%50s", " ");
	    wattroff(tbar_win, COLOR_PAIR(4));
	    mvwprintw(p_win, 7, 52, "100.00%%");
	}

        wrefresh(p_win);
        wrefresh(bar_win);
        wrefresh(tbar_win);
        refresh();
        log_mesg(1, 0, 0, PUI_DEBUG, "Total Time: %s, Ave. Rate: %6.2f%s/%s, %s\n", prog_stat.Eformated, prog_stat.speed, prog_stat.speed_unit, prog->time_unit, "100.00% completed!");
	sleep(1);
    }

#endif
}
