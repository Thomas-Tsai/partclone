/**
 * torrent_helper.h - Part of Partclone project.
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

/*
 * This helper will help you to generate `torrent.info` for partclone_create_torrent.py
 * It will output `offset`, `length`, `sha1` for torrent
 * https://github.com/tjjh89017/ezio utils/partclone_create_torrent.py
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* SHA1 for torrent info */
#if (defined(HAVE_EVP_MD_CTX_new) || defined(HAVE_EVP_MD_CTX_create)) && defined(HAVE_EVP_MD_CTX_reset)
#define HAVE_EVP_MD_CTX_methods
#endif
#if defined(HAVE_EVP_MD_CTX_methods)
#include <openssl/evp.h>
#else
#include <openssl/sha.h>
#endif

#define DEFAULT_PIECE_SIZE (16ULL * 1024 * 1024)

typedef struct {
	unsigned long long PIECE_SIZE;
	unsigned char hash[20]; /* SHA_DIGEST_LENGTH, only present in <openssl/sha.h> */
	/* fd for torrent.info. You should close fd yourself */
	FILE *tinfo;
	/* remember the length for a piece size */
#if defined(HAVE_EVP_MD_CTX_methods)
	EVP_MD_CTX *ctx;
#else
	SHA_CTX ctx;
#endif
	size_t length;
} torrent_generator;

// init
void torrent_init(torrent_generator *torrent, FILE *tinfo);
// put or write data
void torrent_update(torrent_generator *torrent, void *buffer, size_t length);
// flush all sha1 hash for end
void torrent_final(torrent_generator *torrent);
// print offset, start a block
void torrent_start_offset(torrent_generator *torrent, unsigned long long offset);
// print length, end a block
void torrent_end_length(torrent_generator *torrent, unsigned long long length);
