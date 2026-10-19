/**
 * json_status.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Periodically write clone/restore status to a JSON file so that
 * external tools (web dashboards, curl+jq, monitoring wrappers)
 * can poll partclone's progress in a structured way.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef PARTCLONE_JSON_STATUS_H
#define PARTCLONE_JSON_STATUS_H

/* NOTE: partclone.h and progress.h have no include guards; they must be
 * included by the .c file before this header (see json_status.c). */

/// state strings used in the "state" field
#define JSON_STATE_BITMAP  "bitmap"
#define JSON_STATE_RUNNING "running"
#define JSON_STATE_DONE    "done"

/**
 * Enable JSON status output. Path may be NULL (feature disabled).
 * fs_info and opt are referenced, not copied; both must outlive the
 * JSON writer. Returns 0 when enabled, -1 otherwise.
 */
extern int json_status_init(const char *path, const file_system_info *fs_info, const cmd_opt *opt);

/**
 * Write one JSON snapshot unconditionally (state given by caller).
 */
extern void json_status_emit(progress_bar *prog, unsigned long long copied, unsigned long long current, const char *state);

/**
 * Throttled update: writes at most once per opt->json_fresh seconds.
 */
extern void json_status_update(progress_bar *prog, unsigned long long copied, unsigned long long current, const char *state);

/**
 * Write the final snapshot with state "done".
 */
extern void json_status_finish(progress_bar *prog, unsigned long long copied, unsigned long long current);

#endif /* PARTCLONE_JSON_STATUS_H */
