#include <config.h>
#include "checksum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "partclone.h" // for log_mesg() & cmd_opt
#ifdef HAVE_XXHASH
#include "xxhash.h"
#endif

#ifdef HAVE_ISAL
#include <isa-l.h>
#endif

#define CRC32_SEED 0xFFFFFFFF

static uint32_t crc_tab32[256] = { 0 };
static int cs_mode = CSM_NONE;
#ifdef HAVE_XXHASH
static XXH64_state_t* xxh64_state = NULL;
#endif

/**
 * ISA-L compatible CRC32 wrapper
 * This function ensures bit-for-bit compatibility with the existing CRC32
 * by properly handling the seed inversion as per IEEE 802.3 specification
 */
static inline uint32_t crc32_isa_compatible(uint32_t seed, void* buffer, long size) {
#ifdef HAVE_ISAL
    // ISA-L crc32_gzip_refl expects the seed to be bit-inverted
    uint32_t ext_seed = ~seed;
    uint32_t ext_crc = crc32_gzip_refl(ext_seed, (const unsigned char*)buffer, (uint64_t)size);
    // Return bit-inverted result to match expected output
    return ~ext_crc;
#else
    // Fallback to original implementation
    unsigned char * buf = (unsigned char *)buffer;
    const unsigned char * end = buf + size;
    uint32_t tmp, long_c, crc = seed;

    while (buf != end) {
        /// update crc
        long_c = *(buf++);
        tmp = crc ^ long_c;
        crc = (crc >> 8) ^ crc_tab32[tmp & 0xff];
    };

    return crc;
#endif
}

unsigned get_checksum_size(int checksum_mode, int debug) {

	switch(checksum_mode) {

	case CSM_NONE:
		return 0;

	case CSM_CRC32:
	case CSM_CRC32_0001:
		return 4;

#ifdef HAVE_XXHASH
	case CSM_XXH64:
		return sizeof(XXH64_hash_t);
#endif

	default:
		log_mesg(0, 1, 1, debug, "Unknown checksum mode [%d]\n", checksum_mode);
		return UINT_LEAST32_MAX;
		break;
	}
}

const char *get_checksum_str(int checksum_mode) {

	switch(checksum_mode) {

	case CSM_NONE:
		return "NONE";

	case CSM_CRC32:
		return "CRC32";

#ifdef HAVE_XXHASH
	case CSM_XXH64:
		return "XXH64";
#endif

	case CSM_CRC32_0001:
		return "CRC32_0001";

	default:
		return "UNKNOWN";
	}
}

/**
 * Initialise crc32 lookup table if it is not already done and initialise seed
 * the the default implementation seed value
 */
void init_crc32(uint32_t* seed) {

	if (crc_tab32[0] == 0) {
		/// initial crc table
		uint32_t init_crc, init_p;
		uint32_t i, j;
		init_p = 0xEDB88320L;

		for (i = 0; i < 256; i++) {
			init_crc = i;
			for (j = 0; j < 8; j++) {
				if (init_crc & 0x00000001L)
					init_crc = ( init_crc >> 1 ) ^ init_p;
				else
					init_crc = init_crc >> 1;
			}

			crc_tab32[i] = init_crc;
		}
	}

	*seed = CRC32_SEED;
}

/**
 * Initiate the checksum library and seed based on checksum_mode.
 *
 * The main goal if this function is keep the code independent of the algorithm used.
 * To accomplish this, the caller is responsible to allocate enough room to store the
 * checksum.
 */
void init_checksum(int checksum_mode, unsigned char* seed, int debug) {

	cs_mode = checksum_mode;
	switch(checksum_mode) {

	case CSM_CRC32:
		init_crc32((uint32_t*)seed);
		break;

	case CSM_CRC32_0001:
		init_crc32((uint32_t*)seed);
		break;

#ifdef HAVE_XXHASH
	case CSM_XXH64:
		if (xxh64_state == NULL) {
			xxh64_state = XXH64_createState();
		}
		XXH64_reset(xxh64_state, 0); // Using 0 as seed
		break;
#endif

	case CSM_NONE:
		// Nothing to do
		// Leave seed alone as it may be NULL or point to a zero-sized array
		break;

	default:
		log_mesg(0, 1, 1, debug, "Unknown checksum mode [%d]\n", checksum_mode);
		break;
	}
}

/// the crc32 function, reference from libcrc.
/// Author is Lammert Bies  1999-2007
/// Mail: info@lammertbies.nl
/// http://www.lammertbies.nl/comm/info/nl_crc-calculation.html
/// generate crc32 code
uint32_t crc32(uint32_t seed, void* buffer, long size) {
    return crc32_isa_compatible(seed, buffer, size);
}

/**
 * Note
 * This version is only used to detect the x64 bug existing in old image version 0001.
 * Once we know if the image as the bug or not, we disable the checksum check. The x64
 * bug caused the crc32 to be recorded with 8 bytes instead of 4. So the data stream
 * is like this: <block><crc><bug><block><crc><bug>...
 */
static uint32_t crc32_0001(uint32_t seed, void* buffer, int size) {

	unsigned char * buf = (unsigned char *)buffer;
	uint32_t tmp, long_c, crc = seed;
	int s = 0;

	while (s++ < size) {
		/// update crc
		long_c = *buf; // note: yes, 0001 is missing a "++" here
		tmp = crc ^ long_c;
		crc = (crc >> 8) ^ crc_tab32[ tmp & 0xff ];
	};

	return crc;
}

/**
 * Update the checksum by using the algorithm set when init_checksum() was call.
 *
 * The main goal if this function is keep the code independent of the algorithm used.
 * To accomplish this, the caller is responsible to allocate enough room to store the
 * checksum.
 */
void update_checksum(unsigned char* checksum, char* buf, int size) {

	switch(cs_mode)
	{
	case CSM_CRC32:
		*(uint32_t*)checksum = crc32(*(uint32_t*)checksum, (unsigned char*)buf, size);
		break;

	case CSM_CRC32_0001:
		*(uint32_t*)checksum = crc32_0001(*(uint32_t*)checksum, (unsigned char*)buf, size);
		break;

#ifdef HAVE_XXHASH
	case CSM_XXH64:
		XXH64_update(xxh64_state, buf, size);
		break;
#endif

	case CSM_NONE:
		// Nothing to do
		// Leave checksum alone as it may be NULL or point to a zero-sized array.
		break;
	}

}

void finalize_checksum(unsigned char* checksum) {

	switch(cs_mode)
	{
#ifdef HAVE_XXHASH
	case CSM_XXH64:
		*(XXH64_hash_t*)checksum = XXH64_digest(xxh64_state);
		break;
#endif

	case CSM_CRC32:
	case CSM_CRC32_0001:
	case CSM_NONE:
		// Nothing to do
		break;
	}

}

void release_checksum() {
#ifdef HAVE_XXHASH
    if (xxh64_state != NULL) {
        XXH64_freeState(xxh64_state);
        xxh64_state = NULL;
    }
#endif
}

char* format_checksum(const unsigned char* data, unsigned int size) {
    if (data == NULL || size == 0) {
        return NULL;
    }

    char* hex_string = (char*)malloc(size * 2 + 1);
    if (hex_string == NULL) {
        return NULL;
    }

    for (unsigned int i = 0; i < size; i++) {
        sprintf(hex_string + (i * 2), "%02x", data[i]);
    }

    hex_string[size * 2] = '\0';
    return hex_string;
}
