/**
 * torrent_helper.c - Part of Partclone project.
 *
 * Copyright (c) 2019~ Thomas Tsai <thomas at nchc org tw>
 *
 * function and structure used by main.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "torrent_helper.h"

void torrent_init(torrent_generator *torrent, int tinfo)
{
	torrent->PIECE_SIZE = DEFAULT_PIECE_SIZE;
	torrent->length = 0;
	torrent->tinfo = tinfo;
	SHA1_Init(&torrent->ctx);
}

void torrent_update(torrent_generator *torrent, void *buffer, size_t length)
{
	unsigned long long sha_length = torrent->length;
	unsigned long long BT_PIECE_SIZE = torrent->PIECE_SIZE;
	unsigned long long sha_remain_length = BT_PIECE_SIZE - sha_length;
	unsigned long long buffer_remain_length = length;
	unsigned long long buffer_offset = 0;

	int tinfo = torrent->tinfo;

	while (buffer_remain_length > 0) {
		sha_remain_length = BT_PIECE_SIZE - sha_length;
		if (sha_remain_length <= 0) {
			// finish a piece
			SHA1_Final(torrent->hash, &torrent->ctx);
			dprintf(tinfo, "sha1: ");
			for (int x = 0; x < SHA_DIGEST_LENGTH; x++) {
				dprintf(tinfo, "%02x", torrent->hash[x]);
			}
			dprintf(tinfo, "\n");
			// start for next piece;
			SHA1_Init(&torrent->ctx);
			sha_length = 0;
			sha_remain_length = BT_PIECE_SIZE;
		}
		if (buffer_remain_length <= 0) {
			break;
		}
		else if (sha_remain_length > buffer_remain_length) {
			SHA1_Update(&torrent->ctx, buffer + buffer_offset, buffer_remain_length);
			sha_length += buffer_remain_length;
			break;
		}
		else {
			SHA1_Update(&torrent->ctx, buffer + buffer_offset, sha_remain_length);
			buffer_offset += sha_remain_length;
			buffer_remain_length -= sha_remain_length;
			sha_length += sha_remain_length;
		}
	}

	torrent->length = sha_length;
}

void torrent_final(torrent_generator *torrent)
{
	if (torrent->length) {
		SHA1_Final(torrent->hash, &torrent->ctx);
		dprintf(torrent->tinfo, "sha1: ");
		for (int x = 0; x < SHA_DIGEST_LENGTH; x++) {
			dprintf(torrent->tinfo, "%02x", torrent->hash[x]);
		}
		dprintf(torrent->tinfo, "\n");
	}
}

void torrent_start_offset(torrent_generator *torrent, unsigned long long offset)
{
	dprintf(torrent->tinfo, "offset: %032llx\n", offset);
}

void torrent_end_length(torrent_generator *torrent, unsigned long long length)
{
	dprintf(torrent->tinfo, "length: %032llx\n", length);
}
