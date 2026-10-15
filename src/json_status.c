/**
 * json_status.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * Periodically write clone/restore status to a JSON file so that
 * external tools (web dashboards, curl+jq, monitoring wrappers)
 * can poll partclone's progress in a structured way.
 *
 * Writes are atomic: the document is rendered to "<path>.tmp" and
 * then rename(2)d over the real path, so concurrent readers never
 * see a partial file.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <config.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "partclone.h"
#include "progress.h"
#include "json_status.h"
#include "version.h"

#define JSON_SCHEMA_VERSION 1

static char json_path[PATH_MAX];
static char json_tmp_path[PATH_MAX];
static int  json_enabled = 0;
static const file_system_info *json_fs_info;
static const cmd_opt *json_opt;
static time_t json_last_write = 0;

static const char *json_action_string(const cmd_opt *opt)
{
	if (opt->chkimg)
		return "chkimg";
	if (opt->clone)
		return "clone";
	if (opt->restore)
		return "restore";
	if (opt->dd)
		return "dev-to-dev";
	if (opt->domain)
		return "domain";
	if (opt->ddd)
		return "dd";
	return "unknown";
}

/// Escape src into dst as a JSON string body (without surrounding quotes).
static size_t json_escape(char *dst, size_t dst_size, const char *src)
{
	size_t i = 0;

	if (dst_size == 0)
		return 0;

	for (; src && *src && i + 7 < dst_size; src++) {
		unsigned char c = (unsigned char)*src;
		const char *rep = NULL;

		switch (c) {
		case '"':  rep = "\\\""; break;
		case '\\': rep = "\\\\"; break;
		case '\b': rep = "\\b"; break;
		case '\f': rep = "\\f"; break;
		case '\n': rep = "\\n"; break;
		case '\r': rep = "\\r"; break;
		case '\t': rep = "\\t"; break;
		default:
			if (c < 0x20)
				i += snprintf(dst + i, dst_size - i, "\\u%04x", c);
			else
				dst[i++] = (char)c;
		}
		if (rep) {
			size_t l = strlen(rep);
			memcpy(dst + i, rep, l);
			i += l;
		}
	}
	dst[i] = '\0';
	return i;
}

int json_status_init(const char *path, const file_system_info *fs_info, const cmd_opt *opt)
{
	if (path == NULL)
		return -1;

	json_fs_info = fs_info;
	json_opt = opt;

	if (snprintf(json_path, sizeof(json_path), "%s", path) >= (int)sizeof(json_path))
		return -1;
	if (snprintf(json_tmp_path, sizeof(json_tmp_path), "%s.tmp", path) >= (int)sizeof(json_tmp_path))
		return -1;

	json_last_write = 0;
	json_enabled = 1;
	return 0;
}

static void json_status_render(progress_bar *prog, unsigned long long copied, unsigned long long current, const char *state, int done)
{
	prog_stat_t stat;
	char doc[2048 + 2 * PATH_MAX];
	char es_source[PATH_MAX];
	char es_target[PATH_MAX];
	char es_fs[FS_MAGIC_SIZE * 2 + 1];
	unsigned long long used_bytes, free_bytes;
	FILE *fp;

	if (!json_enabled)
		return;

	memset(&stat, 0, sizeof(prog_stat_t));
	progress_calculate_stat(prog, copied, current, done, &stat);

	json_escape(es_source, sizeof(es_source), json_opt->source ? json_opt->source : "");
	json_escape(es_target, sizeof(es_target), json_opt->target ? json_opt->target : "");
	json_escape(es_fs, sizeof(es_fs), json_fs_info->fs);

	used_bytes = json_fs_info->usedblocks * (unsigned long long)json_fs_info->block_size;
	free_bytes = (json_fs_info->totalblock - json_fs_info->usedblocks) * (unsigned long long)json_fs_info->block_size;

	snprintf(doc, sizeof(doc),
		"{\n"
		"  \"schema_version\": %d,\n"
		"  \"timestamp\": %lld,\n"
		"  \"partclone\": {\n"
		"    \"version\": \"%s\",\n"
		"    \"git_version\": \"%s\",\n"
		"    \"action\": \"%s\",\n"
		"    \"state\": \"%s\",\n"
		"    \"source\": \"%s\",\n"
		"    \"target\": \"%s\",\n"
		"    \"filesystem\": \"%s\",\n"
		"    \"block_size\": %u,\n"
		"    \"device_size\": { \"blocks\": %llu, \"bytes\": %llu },\n"
		"    \"space_in_use\": { \"blocks\": %llu, \"bytes\": %llu },\n"
		"    \"free_space\": { \"blocks\": %llu, \"bytes\": %llu },\n"
		"    \"progress\": {\n"
		"      \"current_block\": %llu,\n"
		"      \"copied_blocks\": %llu,\n"
		"      \"total_block\": %llu,\n"
		"      \"data_percent\": %.2f,\n"
		"      \"total_percent\": %.2f,\n"
		"      \"rate_bytes_per_sec\": %llu,\n"
		"      \"elapsed_sec\": %llu,\n"
		"      \"remaining_sec\": %llu\n"
		"    }\n"
		"  }\n"
		"}\n",
		JSON_SCHEMA_VERSION,
		(long long)time(NULL),
		VERSION,
		git_version,
		json_action_string(json_opt),
		state,
		es_source,
		es_target,
		es_fs,
		json_fs_info->block_size,
		json_fs_info->totalblock, json_fs_info->totalblock * (unsigned long long)json_fs_info->block_size,
		json_fs_info->usedblocks, used_bytes,
		json_fs_info->totalblock - json_fs_info->usedblocks, free_bytes,
		current,
		copied,
		prog->total,
		(double)stat.percent,
		(double)stat.total_percent,
		stat.rate_bytes_per_sec,
		stat.elapsed_sec,
		stat.remaining_sec);

	fp = fopen(json_tmp_path, "w");
	if (fp == NULL) {
		json_enabled = 0;
		log_mesg(0, 0, 1, json_opt->debug, "json_status: cannot open %s, disabling JSON status output\n", json_tmp_path);
		return;
	}
	fputs(doc, fp);
	if (fclose(fp) != 0) {
		json_enabled = 0;
		log_mesg(0, 0, 1, json_opt->debug, "json_status: write to %s failed, disabling JSON status output\n", json_tmp_path);
		return;
	}
	if (rename(json_tmp_path, json_path) != 0) {
		json_enabled = 0;
		log_mesg(0, 0, 1, json_opt->debug, "json_status: rename to %s failed, disabling JSON status output\n", json_path);
	}
}

void json_status_emit(progress_bar *prog, unsigned long long copied, unsigned long long current, const char *state)
{
	json_status_render(prog, copied, current, state, 0);
}

void json_status_update(progress_bar *prog, unsigned long long copied, unsigned long long current, const char *state)
{
	time_t now;

	if (!json_enabled)
		return;

	now = time(NULL);
	if (json_last_write != 0 && difftime(now, json_last_write) < (double)json_opt->json_fresh)
		return;

	json_last_write = now;
	json_status_render(prog, copied, current, state, 0);
}

void json_status_finish(progress_bar *prog, unsigned long long copied, unsigned long long current)
{
	if (!json_enabled)
		return;
	json_status_render(prog, copied, current, JSON_STATE_DONE, 1);
}
