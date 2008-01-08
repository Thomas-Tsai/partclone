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
}

/// update number
extern void progress_update(struct progress_bar *p, int current)
{
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
			
        float percent = p->unit * current;
        float speed = (float)p->block_size * (float)current / (float)(time(0) - p->time);
        int estime = p->block_size * (p->stop - current)/(int)speed;
        int total_time = p->block_size * (p->stop - p->start)/(int)speed;

        if (current != p->stop) {
                if ((current - p->start) % p->resolution)
                        return;
                fprintf(stderr, _("%6.2f%% completed, "), percent);
                fprintf(stderr, _("Total: %05i sec., "), total_time);
                fprintf(stderr, _("Estimated: %05i sec., "), estime);
                fprintf(stderr, _("%7.2f MB/s"), speed/1000000.00);
                fprintf(stderr, ("\r"));
        } else{
                fprintf(stderr, _("\n100.00%% completed\n"));
	}
}

