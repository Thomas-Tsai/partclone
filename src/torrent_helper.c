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

void torrent_init(torrent_generator *torrent, FILE *tinfo)
{
	torrent->PIECE_SIZE = DEFAULT_PIECE_SIZE;
	torrent->length = 0;
	torrent->tinfo = tinfo;
#if !defined(HAVE_EVP_MD_CTX_methods)
	SHA1_Init(&torrent->ctx);
#elif defined(HAVE_EVP_MD_CTX_new)
	torrent->ctx = EVP_MD_CTX_new();
	EVP_DigestInit(torrent->ctx, EVP_sha1());
#elif defined(HAVE_EVP_MD_CTX_create)
	torrent->ctx = EVP_MD_CTX_create();
	EVP_DigestInit(torrent->ctx, EVP_sha1());
#endif
}

void torrent_update(torrent_generator *torrent, void *buffer, size_t length)
{
	unsigned long long sha_length = torrent->length;
	unsigned long long BT_PIECE_SIZE = torrent->PIECE_SIZE;
	unsigned long long sha_remain_length = BT_PIECE_SIZE - sha_length;
	unsigned long long buffer_remain_length = length;
	unsigned long long buffer_offset = 0;

	FILE *tinfo = torrent->tinfo;
	int x = 0;

	while (buffer_remain_length > 0) {
		sha_remain_length = BT_PIECE_SIZE - sha_length;
		if (sha_remain_length <= 0) {
			// finish a piece
#if defined(HAVE_EVP_MD_CTX_methods)
			EVP_DigestFinal(torrent->ctx, torrent->hash, NULL);
#else
			SHA1_Final(torrent->hash, &torrent->ctx);
#endif
			fprintf(tinfo, "sha1: ");
			for (x = 0; x < 20 /* SHA_DIGEST_LENGTH */; x++) {
				fprintf(tinfo, "%02x", torrent->hash[x]);
			}
			fprintf(tinfo, "\n");
			// start for next piece;
#if defined(HAVE_EVP_MD_CTX_methods)
			EVP_MD_CTX_reset(torrent->ctx);
			EVP_DigestInit(torrent->ctx, EVP_sha1());
#else
			SHA1_Init(&torrent->ctx);
#endif
			sha_length = 0;
			sha_remain_length = BT_PIECE_SIZE;
		}
		if (buffer_remain_length <= 0) {
			break;
		}
		else if (sha_remain_length > buffer_remain_length) {
#if defined(HAVE_EVP_MD_CTX_methods)
			EVP_DigestUpdate(torrent->ctx, buffer + buffer_offset, buffer_remain_length);
#else
			SHA1_Update(&torrent->ctx, buffer + buffer_offset, buffer_remain_length);
#endif
			sha_length += buffer_remain_length;
			break;
		}
		else {
#if defined(HAVE_EVP_MD_CTX_methods)
			EVP_DigestUpdate(torrent->ctx, buffer + buffer_offset, sha_remain_length);
#else
			SHA1_Update(&torrent->ctx, buffer + buffer_offset, sha_remain_length);
#endif
			buffer_offset += sha_remain_length;
			buffer_remain_length -= sha_remain_length;
			sha_length += sha_remain_length;
		}
	}

	torrent->length = sha_length;
}

void torrent_final(torrent_generator *torrent)
{
	int x = 0;

	if (torrent->length) {
#if !defined(HAVE_EVP_MD_CTX_methods)
		SHA1_Final(torrent->hash, &torrent->ctx);
#elif defined(HAVE_EVP_MD_CTX_new)
		EVP_DigestFinal(torrent->ctx, torrent->hash, NULL);
		EVP_MD_CTX_free(torrent->ctx);
#elif defined(HAVE_EVP_MD_CTX_create)
		EVP_DigestFinal(torrent->ctx, torrent->hash, NULL);
		EVP_MD_CTX_destroy(torrent->ctx);
#endif
		fprintf(torrent->tinfo, "sha1: ");
		for (x = 0; x < 20 /* SHA_DIGEST_LENGTH */; x++) {
			fprintf(torrent->tinfo, "%02x", torrent->hash[x]);
		}
		fprintf(torrent->tinfo, "\n");
	}
}

void torrent_start_offset(torrent_generator *torrent, unsigned long long offset)
{
	fprintf(torrent->tinfo, "offset: %032llx\n", offset);
}

void torrent_end_length(torrent_generator *torrent, unsigned long long length)
{
	fprintf(torrent->tinfo, "length: %032llx\n", length);
}
