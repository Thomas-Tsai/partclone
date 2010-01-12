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
    time_t now;
    time(&now);
    memset(prog, 0, sizeof(progress_bar));
    prog->start = start;
    prog->stop = stop;
    prog->unit = 100.0 / (stop - start);
    prog->time = now;
    prog->block_size = size;
    if (RES){
        prog->resolution = RES*100;
    } else {
        if (stop <= 5000){
            prog->resolution = 100;
        } else {
            prog->resolution = 1000;
        }
    }
    prog->rate = 0.0;
    prog->pui = PUI;
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
    if (done != 1){
        if (((current - prog->start) % prog->resolution) && ((current != prog->stop)))
            return;
    }
    if (prog->pui == NCURSES)
        Ncurses_progress_update(prog, current, done);
    else if (prog->pui == DIALOG)
        Dialog_progress_update(prog, current, done);
    else if (prog->pui == TEXT)
        progress_update(prog, current, done);
}

/// update information at progress bar
extern void progress_update(struct progress_bar *prog, unsigned long long current, int done)
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

    if (done != 1){
        if (((current - prog->start) % prog->resolution) && ((current != prog->stop)))
            return;
        percent  = prog->unit * current;
        if (percent <= 0)
            percent = 1;
        elapsed  = (time(0) - prog->time);
        if (elapsed <= 0)
            elapsed = 1;
        speedps  = (float)prog->block_size * (float)current / (float)(elapsed);
        //remained = (time_t)(prog->block_size * (prog->stoprog- current)/(int)speedps);
        remained = (time_t)((elapsed/percent*100) - elapsed);
        speed = (float)(speedps / 1000000.0 * 60.0);
        prog->rate = prog->rate+speed;

        /// format time string
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);

        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);

        fprintf(stderr, _("\r%81c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2fMB/min, "), clear_buf, Eformated, Rformated, percent, (float)(speed));
        /*
           fprintf(stderr, ("\r%81c\r"), clear_buf);
           fprintf(stderr, _("Elapsed: %s, "), Eformated);
           fprintf(stderr, _("Remaining: %s, "), Rformated);
           fprintf(stderr, _("Completed:%6.2f%%, "), percent);
           fprintf(stderr, _("Rate:%6.1fMB/min, "), (float)(prog->rate));
         */
    } else {
        elapsed  = (time(0) - prog->time);
        if (elapsed <= 0)
            elapsed = 1;
        speedps  = (float)prog->block_size * (float)current / (float)(elapsed);
        speed = (float)(speedps / 1000000.0 * 60.0);
        percent=100;
        /// format time string
        remained = (time_t)0;
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);
        total = elapsed;
        Ttm = gmtime(&total);
        strftime(Tformated, sizeof(Tformated), format, Ttm);
        fprintf(stderr, _("\r%81c\rElapsed: %s, Remaining: %s, Completed:%6.2f%%, Rate: %6.2fMB/min, "), clear_buf, Tformated, Rformated, percent, (float)(speed));
        fprintf(stderr, _("\nTotal Time: %s, "), Tformated);
        fprintf(stderr, _("Ave. Rate: %6.1fMB/min, "), (float)(speed));
        fprintf(stderr, _("%s"), "100.00%% completed!\n");
    }
}

/// update information at ncurses mode
extern void Ncurses_progress_update(struct progress_bar *prog, unsigned long long current, int done)
{
#ifdef HAVE_LIBNCURSESW

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

    if (done != 1){
        if (((current - prog->start) % prog->resolution) && ((current != prog->stop)))
            return;

        percent  = prog->unit * current;
        elapsed  = (time(0) - prog->time);
        if (elapsed <= 0)
            elapsed = 1;
        speedps  = (float)prog->block_size * (float)current / (float)(elapsed);
        speed = (float)(speedps / 1000000.0 * 60.0);
        remained = (time_t)((elapsed/percent*100) - elapsed);
        prog->rate = prog->rate+speed;

        /// format time string
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);

        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);

        /// set bar color
        init_pair(4, COLOR_RED, COLOR_RED);
        init_pair(5, COLOR_WHITE, COLOR_BLUE);
        init_pair(6, COLOR_WHITE, COLOR_RED);
        werase(p_win);
        werase(bar_win);


        mvwprintw(p_win, 0, 0, _(" "));
        mvwprintw(p_win, 1, 0, _("Elapsed: %s") , Eformated);
        mvwprintw(p_win, 2, 0, _("Remaining: %s"), Rformated);
        mvwprintw(p_win, 3, 0, _("Rate: %6.2fMB/min"), (float)(speed));
        //mvwprintw(p_win, 3, 0, _("Completed:%6.2f%%"), percent);
        p_block = malloc(50);
        memset(p_block, 0, 50);
        memset(p_block, ' ', (size_t)(percent*0.5));
        wattrset(bar_win, COLOR_PAIR(4));
        mvwprintw(bar_win, 0, 0, "%s", p_block);
        wattroff(bar_win, COLOR_PAIR(4));
        if(percent <= 50){
            wattrset(p_win, COLOR_PAIR(5));
            mvwprintw(p_win, 5, 25, "%3.0f%%", percent);
            wattroff(p_win, COLOR_PAIR(5));
        }else{
            wattrset(p_win, COLOR_PAIR(6));
            mvwprintw(p_win, 5, 25, "%3.0f%%", percent);
            wattroff(p_win, COLOR_PAIR(6));
        }
        mvwprintw(p_win, 5, 52, "%6.2f%%", percent);
        wrefresh(p_win);
        wrefresh(bar_win);
        free(p_block);
    } else {
        percent=100;
        total = (time(0) - prog->time);
        Ttm = gmtime(&total);
        speedps  = (float)prog->block_size * (float)current / (float)(total);
        speed = (float)(speedps / 1000000.0 * 60.0);
        strftime(Tformated, sizeof(Tformated), format, Ttm);
        mvwprintw(p_win, 1, 0, _("Total Time: %s"), Tformated);
        mvwprintw(p_win, 2, 0, _("Remaining: 0"));
        mvwprintw(p_win, 3, 0, _("Ave. Rate: %6.2fMB/min"), (float)speed);
        wattrset(bar_win, COLOR_PAIR(4));
        mvwprintw(bar_win, 0, 0, "%50s", " ");
        wattroff(bar_win, COLOR_PAIR(4));
        wattrset(p_win, COLOR_PAIR(6));
        mvwprintw(p_win, 5, 22, "%6.2f%%", percent);
        wattroff(p_win, COLOR_PAIR(6));
        mvwprintw(p_win, 5, 52, "%6.2f%%", percent);
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

    if (done != 1){
        if (((current - prog->start) % prog->resolution) && ((current != prog->stop)))
            return;

        percent  = prog->unit * current;
        elapsed  = (time(0) - prog->time);
        if (elapsed <= 0)
            elapsed = 1;
        speedps  = (float)prog->block_size * (float)current / (float)(elapsed);
        //remained = (time_t)(prog->block_size * (prog->stoprog- current)/(int)speedps);
        remained = (time_t)((elapsed/percent*100) - elapsed);
        speed = (float)(speedps / 1000000.0 * 60.0);
        prog->rate = prog->rate+speed;

        /// format time string
        Rtm = gmtime(&remained);
        strftime(Rformated, sizeof(Rformated), format, Rtm);

        Etm = gmtime(&elapsed);
        strftime(Eformated, sizeof(Eformated), format, Etm);

        m_dialog.percent = (int)percent;
        sprintf(tmp_str, _("  Elapsed: %s\n  Remaining: %s\n  Completed:%6.2f%%\n  Rate: %6.2fMB/min, "), Eformated, Rformated, percent, (float)(speed));
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    } else {
        total = (time(0) - prog->time);
        Ttm = gmtime(&total);
        speedps  = (float)prog->block_size * (float)current / (float)(total);
        speed = (float)(speedps / 1000000.0 * 60.0);
        strftime(Tformated, sizeof(Tformated), format, Ttm);
        m_dialog.percent = 100;
        sprintf(tmp_str, _("  Total Time: %s\n  Ave. Rate: %6.1fMB/min\n 100.00%%  completed!\n"), Tformated, (float)speed);
        fprintf(stderr, "XXX\n%i\n%s\n%s\nXXX\n", m_dialog.percent, m_dialog.data, tmp_str);
    }

}
