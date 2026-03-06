# Partclone XFS Maintenance Guide

The files in `src/xfs/` are imported from `xfsprogs-6.13.0`. To facilitate future updates, this document records all manual modifications and structural changes made to the original library files.

## Manual Modifications to Source Files

1.  **`src/xfs/include/platform_defs.h`**
    - **Change**: Added `#include "config.h"` at the top.
    - **Reason**: To ensure that configuration macros (like `HAVE_MEMFD_CREATE`) are visible to the XFS library files.

2.  **`src/xfs/libxfs/cache.c`**
    - **Change**: Commented out the `#define CACHE_DEBUG 1` lines.
    - **Reason**: To silence verbose debug output (`cache_purge`, `cache_zero_check`) that is unnecessary for Partclone.

## Extra / Generated Files

1.  **`src/xfs/libfrog/crc32table.h`**
    - **Description**: This file is generated via `gen_crc32table.c`.
    - **Regeneration**: `gcc -Isrc/xfs/libfrog -o gen_crc32table xfsprogs-6.13.0/libfrog/gen_crc32table.c && ./gen_crc32table > src/xfs/libfrog/crc32table.h`.

2.  **`src/xfs/include/xfs/`**
    - **Description**: A directory containing symlinks to headers in `src/xfs/include/`.
    - **Reason**: To satisfy dependencies in some XFS source files that expect headers to be in an `xfs/` subfolder.

## Dependencies

The updated library introduces dependencies on:
- **`liburcu`** (Userspace RCU): Used for synchronization in the new libxfs.
- **`libblkid`**: Used for topology detection.

These should be linked when building `partclone.xfs`.

## Update Procedure (Reference)

When updating to a newer `xfsprogs` version:
1.  Replace `src/xfs/libxfs/` and `src/xfs/include/` with new files.
2.  Identify and copy required `libfrog/` files (as listed in `src/Makefile.am`).
3.  Re-apply the modifications to `platform_defs.h` and `cache.c`.
4.  Re-generate `crc32table.h`.
5.  Verify symlinks in `src/xfs/include/xfs/`.
6.  Adapt `src/xfsclone.c` to any new API changes.
