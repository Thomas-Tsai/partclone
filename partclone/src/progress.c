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
#include "config.h"
#include "progress.h"
#include "gettext.h"
#define _(STRING) gettext(STRING)

/// initial progress bar
extern void progress_init(struct progress_bar *p, int start, int stop, int res)
{
        p->start = start;
        p->stop = stop;
        p->unit = 100.0 / (stop - start);
        p->resolution = res;
}

/// update number
extern void progress_update(struct progress_bar *p, int current)
{
        setlocale(LC_ALL, "");
        bindtextdomain(PACKAGE, LOCALEDIR);
        textdomain(PACKAGE);
			
        float percent = p->unit * current;

        if (current != p->stop) {
                if ((current - p->start) % p->resolution)
                        return;
                fprintf(stderr, _("%6.2f percent completed"), percent);
                fprintf(stderr, ("\r"), percent);
        } else
                fprintf(stderr, _("100.00 percent completed\n"));

}

