# Partclone Security Testing

This document describes how to test the security validation checks for the partclone image header. These tests are designed to be run after building the `partclone` binaries.

## 1. The `create_poc.c` program

This program is used to generate malicious partclone images with invalid header fields for both `v1` and `v2` image formats.

```c
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
            header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size; // Match calculated for consistency
        } else if (strcmp(poc_type, "invalid_usedblocks") == 0) {
            header.fs_info.usedblocks = header.fs_info.totalblock + 1;
        } else if (strcmp(poc_type, "inconsistent_device_size") == 0) {
            header.fs_info.device_size = 12345;
        } else if (strcmp(poc_type, "small_file") == 0) {
            header.fs_info.totalblock = 1024 * 1024 * 8; // 1MB bitmap
            header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
        } else if (strcmp(poc_type, "large_totalblock") == 0) {
            header.fs_info.totalblock = ULLONG_MAX;
            header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size; // Match calculated
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
             header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
        } else if (strcmp(poc_type, "invalid_usedblocks") == 0) {
            header.fs_info.usedblocks = header.fs_info.totalblock + 1;
        } else if (strcmp(poc_type, "inconsistent_device_size") == 0) {
            header.fs_info.device_size = 12345;
        } else if (strcmp(poc_type, "small_file") == 0) {
             header.fs_info.totalblock = 1024 * 1024 * 8;
             header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
        } else if (strcmp(poc_type, "large_totalblock") == 0) {
            header.fs_info.totalblock = ULLONG_MAX;
            header.fs_info.device_size = header.fs_info.totalblock * header.fs_info.block_size;
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
```

## 2. Compilation

To compile `create_poc.c`, run the following command from the root of the partclone project:

```bash
gcc -o tests/create_poc tests/create_poc.c -I. -Isrc -Wall -g
```

## 3. Advanced Testing with AddressSanitizer (ASan)

To detect subtle memory errors like heap overflows, it is highly recommended to build `partclone` with AddressSanitizer (ASan).

### 3.1 What is ASan?
ASan is a compiler feature that adds extra instrumentation to the code to detect memory errors at runtime. When a memory error occurs, it prints a detailed report and stops the program, making it an incredibly powerful debugging tool.

### 3.2 Building with ASan
To enable ASan, you need to pass the `-fsanitize=address` flag to both the compiler and the linker. The easiest way to do this is by setting `CFLAGS` and `LDFLAGS` when running the `configure` script.

1.  **Clean the build** (if you have built before):
    ```bash
    make clean
    ```

2.  **Configure with ASan flags**:
    ```bash
    ./configure --enable-all CFLAGS="-fsanitize=address -g" CXXFLAGS="-fsanitize=address -g" LDFLAGS="-fsanitize=address"
    ```
    *   `-g` is included to ensure ASan reports have precise file and line number information.

3.  **Build the project**:
    ```bash
    make
    ```

### 3.3 Running Tests with ASan
After building with ASan, run the test procedures as described below. If a memory error (like a heap overflow) occurs, the program will halt and print a detailed report to stderr. If there are no memory errors, the program will behave as expected (e.g., exiting gracefully with an error message for invalid images).

## 4. Testing Procedures

The following sections detail how to test each of the implemented security checks for both `v1` and `v2` image formats.

A temporary log file is used in the commands below. You can change the path `/tmp/partclone.log` to any other writable location.

### 4.1 v2 Image Format Tests

#### 4.1.1 Invalid `block_size`

1.  **Generate:** `./tests/create_poc invalid_block_size v2`
2.  **Run:** `./src/partclone.restore -s poc_invalid_block_size_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image: block_size (...) is invalid or too large.`

#### 4.1.2 Invalid `usedblocks`

1.  **Generate:** `./tests/create_poc invalid_usedblocks v2`
2.  **Run:** `./src/partclone.restore -s poc_invalid_usedblocks_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image: usedblocks (...) is larger than totalblock (...).`

#### 4.1.3 Inconsistent `device_size`

1.  **Generate:** `./tests/create_poc inconsistent_device_size v2`
2.  **Run:** `./src/partclone.restore -s poc_inconsistent_device_size_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image: calculated filesystem size is larger than device size.`

#### 4.1.4 Bitmap Size vs. File Size

1.  **Generate:** `./tests/create_poc small_file v2`
2.  **Run:** `./src/partclone.restore -s poc_small_file_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image: declared bitmap size (...) is larger than remaining file size (...).`

#### 4.1.5 Large `totalblock` (DoS)

1.  **Generate:** `./tests/create_poc large_totalblock v2`
2.  **Run:** `./src/partclone.restore -s poc_large_totalblock_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image: totalblock (...) exceeds signed long long max value.` (or `excessively large for bitmap allocation` if that check is hit first).

#### 4.1.6 Large `totalblock` (Integer Overflow)

1.  **Generate:** `./tests/create_poc large_totalblock_overflow v2`
2.  **Run:** `./src/partclone.restore -s poc_large_totalblock_overflow_v2.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails gracefully with an error like `Invalid image: usedblocks (...) is larger than totalblock (...)`. It must not crash.

### 4.2 v1 Image Format Tests (Backward Compatibility)

#### 4.2.1 Invalid `block_size`

1.  **Generate:** `./tests/create_poc invalid_block_size v1`
2.  **Run:** `./src/partclone.restore -s poc_invalid_block_size_v1.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image v1: block_size (...) is invalid or too large.`

#### 4.2.2 Invalid `usedblocks`

1.  **Generate:** `./tests/create_poc invalid_usedblocks v1`
2.  **Run:** `./src/partclone.restore -s poc_invalid_usedblocks_v1.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image v1: usedblocks (...) is larger than totalblock (...).`

#### 4.2.3 Inconsistent `device_size`

1.  **Generate:** `./tests/create_poc inconsistent_device_size v1`
2.  **Run:** `./src/partclone.restore -s poc_inconsistent_device_size_v1.img -O /dev/null -L /tmp/partclone.log -C`
3.  **Expected:** Fails with `Invalid image v1: calculated filesystem size is larger than device size.`

#### 4.2.4 Large `totalblock` (DoS)



1.  **Generate:** `./tests/create_poc large_totalblock v1`

2.  **Run:** `./src/partclone.restore -s poc_large_totalblock_v1.img -O /dev/null -L /tmp/partclone.log -C`

3.  **Expected:** Fails with `Invalid image v1: totalblock (...) exceeds signed long long max value.` (or `excessively large for bitmap allocation` if that check is hit first).



#### 4.2.5 Large `totalblock` (Integer Overflow)



1.  **Generate:** `./tests/create_poc large_totalblock_overflow v1`

2.  **Run:** `./src/partclone.restore -s poc_large_totalblock_overflow_v1.img -O /dev/null -L /tmp/partclone.log -C`

3.  **Expected:** Fails gracefully with an error like `Invalid image v1: usedblocks (...) is larger than totalblock (...)`. It must not crash.
