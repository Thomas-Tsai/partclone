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

/// the progress bar structure
struct progress_bar {
        int start;
        int stop;
        int resolution;
        float unit;
};
typedef struct progress_bar progress_bar;

/// initial progress bar
extern void progress_init(struct progress_bar *p, int start, int stop, int res);

/// update number
extern void progress_update(struct progress_bar *p, int current);


