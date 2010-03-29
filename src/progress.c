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
int window_f = 0;
int color_support = 1;
#endif

int PUI;
unsigned long RES=0;

/// initial progress bar
extern void progress_init(struct progress_bar *prog, int start, unsigned long long stop, int size)
{
    memset(prog, 0, sizeof(progress_bar));
    prog->start = start;
    prog->stop = stop;
    prog->unit = 100.0 / (stop - start);
    prog->initial_time = time(0);
    prog->resolution_time = time(0);
    prog->interval_time = 1;
    prog->block_size = size;
    if (RES){
        prog->interval_time = RES;
    }
    prog->rate = 0.0;
    prog->pui = PUI;

    /*
    if (RES){
        prog->resolution = RES*100;
    } else {
        if (stop <= 5000){
            prog->resolution = 100;
	} else if ((stop > 5000) && (stop <= 50000)){
	    prog->resolution = 300;
        } else {
            prog->resolution = 1000;
        }
    }
    */
}

/// open progress interface
extern int open_pui(int pui, unsigned long res){
    int tui = 0;
    if (pui == NCURSES){
        tui = open_ncurses();
        if (tui == 0){
            close_ncurses();
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

extern void update_pui(struct progress_bar *prog, unsigned long long current, int done){
    if (prog->pui == NCURSES)
        Ncurses_progress_update(prog, current, done);
    else if (prog->pui == DIALOG)
        Dialog_progress_update(prog, current, done);
    else if (prog->pui == TEXT)
        progress_update(prog, current, done);
}

static void calculate_speed(struct progress_bar *prog, unsigned long long current, int done, prog_stat_t *prog_stat){
    char *format = "%H:%M:%S";
    double speedps = 1.0;
    float speed = 1.0;
    float percent = 1.0;
    time_t remained;
    time_t elapsed;
    char Rformated[10], Eformated[10];
    char speed_unit[] = "    ";
    struct tm *Rtm, *Etm;
    uint64_t gbyte=1000000000;
    uint64_t mbyte=1000000;
    uint64_t kbyte=1000;


    percent  = prog->unit * current;
    if (percent <= 0)
	percent = 1;

    elapsed  = (time(0) - prog->initial_time);
    if (elapsed <= 0)
	elapsed = 1;

    speedps  = (float)prog->block_size * (float)current / (float)(elapsed);
    speed = (float)(speedps * 60.0);

    if(prog->block_size == 1){ // don't show bitmap rate, bit_size = 1
	speed = 0;	
    }

    if (speed >= gbyte){
	speed = speed / gbyte;
	strncpy(speed_unit, "GB", 3);
    }else if (speed >= mbyte){
	speed = speed / mbyte;
	strncpy(speed_unit, "MB", 3);
    }else if (speed >= kbyte){
	speed = speed / kbyte;
	strncpy(speed_unit, "KB", 3);
    }else{
	strncpy(speed_unit, "byte", 5);
    }

    if (done != 1){
        remained = (time_t)((elapsed/percent*100) - elapsed);

        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);

        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);

    } else {
        percent=100;
        remained = (time_t)0;
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);
        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);
    }

    strncpy(prog_stat->Eformated, Eformated, 10);
    strncpy(prog_stat->Rformated, Rformated, 10);
    prog_stat->percent   = percent;
    prog_stat->speed     = speed;
    strncpy(prog_stat->speed_unit, speed_unit, 3);
}

/// update information at progress bar
extern void progress_update(struct progress_bar *prog, unsigned long long current, int done)
{
    char clear_buf = ' ';
    prog_stat_t prog_stat;

    if (done != 1)
	if ((difftime(time(0), prog->resolution_time) < prog->interval_time) && current != 0)
	    return;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    if (done != 1){
	prog->resolution_time = time(0);
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

        if ((current+1) == prog->stop){
	    fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Seeking...,"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent, (float)(prog_stat.speed));
	} else {
	    if((int)prog_stat.speed > 0)
	    fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2f%s/min,"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent, (float)(prog_stat.speed), prog_stat.speed_unit);
	    else
		fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%,"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);
	}
    } else {
	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	if((int)prog_stat.speed > 0)
	    fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2f%s/min,"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent, (float)(prog_stat.speed), prog_stat.speed_unit);
	else
	    fprintf(stderr, _("\r%80c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%,"), clear_buf, prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);

        fprintf(stderr, _("\nTotal Time: %s, "), prog_stat.Eformated);
	if((int)prog_stat.speed > 0)
	    fprintf(stderr, _("Ave. Rate: %6.1f%s/min, "), (float)(prog_stat.speed), prog_stat.speed_unit);
        fprintf(stderr, _("%s"), "100.00%% completed!\n");
    }
}

/// update information at ncurses mode
extern void Ncurses_progress_update(struct progress_bar *prog, unsigned long long current, int done)
{
#ifdef HAVE_LIBNCURSESW

    char *clear_buf = NULL;
    char *p_block;
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    if (done != 1){
        if (difftime(time(0), prog->resolution_time) < prog->interval_time)
            return;
	prog->resolution_time = time(0);

        /// set bar color
        init_pair(4, COLOR_RED, COLOR_RED);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);
        init_pair(6, COLOR_WHITE, COLOR_RED);
        werase(p_win);
        werase(bar_win);


        mvwprintw(p_win, 0, 0, _(" "));
        mvwprintw(p_win, 1, 0, _("Elapsed: %s") , prog_stat.Eformated);
        mvwprintw(p_win, 2, 0, _("Remaining: %s"), prog_stat.Rformated);
	if ((int)prog_stat.speed > 0)
	    mvwprintw(p_win, 3, 0, _("Rate: %6.2f%s/min"), (float)(prog_stat.speed), prog_stat.speed_unit);
        //mvwprintw(p_win, 3, 0, _("Completed:%6.2f%%"), percent);
        p_block = malloc(50);
        memset(p_block, 0, 50);
        memset(p_block, ' ', (size_t)(prog_stat.percent*0.5));
        wattrset(bar_win, COLOR_PAIR(4));
        mvwprintw(bar_win, 0, 0, "%s", p_block);
        wattroff(bar_win, COLOR_PAIR(4));
        if(prog_stat.percent <= 50){
            wattrset(p_win, COLOR_PAIR(5));
            mvwprintw(p_win, 5, 25, "%3.0f%%", prog_stat.percent);
            wattroff(p_win, COLOR_PAIR(5));
        }else{
            wattrset(p_win, COLOR_PAIR(6));
            mvwprintw(p_win, 5, 25, "%3.0f%%", prog_stat.percent);
            wattroff(p_win, COLOR_PAIR(6));
        }
        mvwprintw(p_win, 5, 52, "%6.2f%%", prog_stat.percent);
        wrefresh(p_win);
        wrefresh(bar_win);
        free(p_block);
    } else {
        mvwprintw(p_win, 1, 0, _("Total Time: %s"), prog_stat.Eformated);
        mvwprintw(p_win, 2, 0, _("Remaining: 0"));
	if ((int)prog_stat.speed > 0)
	    mvwprintw(p_win, 3, 0, _("Ave. Rate: %6.2f%s/min"), (float)prog_stat.speed, prog_stat.speed_unit);
        wattrset(bar_win, COLOR_PAIR(4));
        mvwprintw(bar_win, 0, 0, "%50s", " ");
        wattroff(bar_win, COLOR_PAIR(4));
        wattrset(p_win, COLOR_PAIR(6));
        mvwprintw(p_win, 5, 22, "%6.2f%%", prog_stat.percent);
        wattroff(p_win, COLOR_PAIR(6));
        mvwprintw(p_win, 5, 52, "%6.2f%%", prog_stat.percent);
        wrefresh(p_win);
        wrefresh(bar_win);
        refresh();
        sleep(1);
    }

    if(done == 1){
        window_f = close_p_ncurses();
    }

#endif
}

static int open_p_ncurses(){

    return 1;
}

static int close_p_ncurses(){

    return 1;
}
/// update information as dialog format, refernece source code of dialog
/// # mkfifo pipe
/// # (./clone.extfs -d -c -X -s /dev/loop0 2>pipe | cat - > test.img) | ./gauge < pipe
/// # (cat test - |./clone.extfs -d -X -r -s - -o /dev/loop0 2>pipe) | ./gauge < pipe
extern void Dialog_progress_update(struct progress_bar *prog, unsigned long long current, int done){
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    extern p_dialog_mesg m_dialog;

    char tmp_str[128];
    char *clear_buf = NULL;
    prog_stat_t prog_stat;

    memset(&prog_stat, 0, sizeof(prog_stat_t));
    calculate_speed(prog, current, done, &prog_stat);

    if (done != 1){
        if (difftime(time(0), prog->resolution_time) < prog->interval_time)
            return;
	prog->resolution_time = time(0);

        m_dialog.percent = (int)prog_stat.percent;
	if ((int)prog_stat.speed > 0)
	    sprintf(tmp_str, _("  Elapsed: %s\n  Remaining: %s\n  Completed:%6.2f%%\n  Rate: %6.2f%s/min, "), prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent, (float)(prog_stat.speed), prog_stat.speed_unit);
	else
	    sprintf(tmp_str, _("  Elapsed: %s\n  Remaining: %s\n  Completed:%6.2f%%"), prog_stat.Eformated, prog_stat.Rformated, prog_stat.percent);
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    } else {
        m_dialog.percent = 100;
	if ((int)prog_stat.speed > 0)
	    sprintf(tmp_str, _("  Total Time: %s\n  Ave. Rate: %6.1f%s/min\n 100.00%%  completed!\n"), prog_stat.Eformated, (float)prog_stat.speed, prog_stat.speed_unit);
	else
	    sprintf(tmp_str, _("  Total Time: %s\n  100.00%%  completed!\n"), prog_stat.Eformated);
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    }

}
