# Image formats

This file describes Partclone's image formats. 2 versions exist: 0001 and 0002.

The generic structure of the images is like this:

|   section   |
|-------------|
| Image header|
|   Bitmap    |
|   Blocks    |

Only the blocks present in the bitmap are recorded in the blocks section.


# Format 0001

This is the original format. It is used by all partclone versions lower than 0.3.0.

## Image header

|Byte Offset | Size | Description
|------------|------|-------------
|          0 |   15 | Partclone image signature
|         15 |   15 | File system's type
|         30 |    4 | **_Image version, in text: "0001"_**
|         34 |    2 | Padding (no meaning)
|         36 |    4 | File system's block size
|         40 |    8 | File system's total size
|         48 |    8 | File system's total block count
|         56 |    8 | File system's used block count
|         64 | 4096 | Unused buffer
|       4160 |      | Total size of image header

## Bitmap

|Byte Offset| Size | Description
|-----------|------|-------------
|         0 | File system's total block count | 1 byte per block: 0 = Not present; 1 = Present; other = ??
| File system's total block count | 8 | Bitmap's signature: "BiTmAgIc"

**Note:** It seems that some bitmaps contain values other than 0 or 1. The meaning of these values are not know and were probably caused by a bug manipulating the block's presence.

## Blocks

|Byte Offset | Size | Description
|------------|------|-------------
|          N | File system's block size | 1 block of data
|          N |    4 | CRC32 for the block (see https://sourceforge.net/p/partclone/bugs/6/)


# Format 0002

This new format is used since partclone 0.3.0. Partclone only write in this format, but it can read the previous format and restore the data.

The main reason why this version has been created was to fix the issue with the checksum present after each block in 0001.

The new format also support a few new features and has some flexibility for future extensions.

## Image header

|Byte Offset | Size | Description
|------------|------|-------------
|          0 |   16 | Partclone image signature
|         16 |   14 | Partclone version used to create the image, ex: 0.3.1
|         30 |    4 | **_Image version, in text: "0002"_**
|         34 |    2 | Endianess marker: 0xC0DE = little-endian, 0xDEC0 = big-endian
|         36 |   16 | File system's type
|         52 |    8 | File system's total size
|         60 |    8 | File system's total block count
|         68 |    8 | File system's used block count based on super-block
|         76 |    8 | File system's used block count based on bitmap
|         84 |    4 | File system's block size
|         88 |    4 | Size of feature section
|         92 |    2 | Image version, in binary: 0x0002
|         94 |    2 | Number of bits for CPU data: 32, 64, other?
|         96 |    2 | Checksum mode for the block strip: 0 = None, 1 = CRC32
|         98 |    2 | Checksum size, in bytes (4 for CRC32)
|        100 |    4 | Blocks per checksum, default is 256.
|        104 |    1 | Reseed checksum: 1 = yes, 0 = no
|        105 |    1 | Bitmap mode, see bitmap_mode_enum
|        106 |    4 | CRC32 of the previous bytes
|        110 |      | Total size of image header

## Bitmap

|Byte Offset| Size | Description
|-----------|------|-------------
|         0 | ceil(File system's total block count / 8) | 1 **bit** per block: 0 = Not present; 1 = Present
| ceil(File system's total block count / 8) | 4 | Bitmap's CRC32

**Note:** If the bitmap's CRC does not match, the image data cannot be restored.

## Blocks

|Byte Offset | Size | Description
|------------|------|-------------
|          N | File system's block size * Blocks per checksum | 1 strip of data
|          N | Checksum size | Checksum for the strip, if checksum mode

**Note:** If "Checksum mode" is None, "Blocks per checksum" is 0. In this case, the blocks are simply
written one after the other.

## Special notes

  - Image 0002 stores a few details about the version used to create the image and the platform used to
    help to handle issues with the image if some bugs are found later. By example, earlier versions of
    partclone did not write the right data size when it was run on a 64 bits platform. Since image 0001
    does not has any detail about the tool used, the code has to detect the presence of that issue.
    Another issue was related to the endianess of the plateform.
  - Image 0002 allows to disable the checksum because the data is usually piped in an archiver.
    If the archive gets corrupted, the archiver will abort before sending the data base into partclone.
    Disabling the checksum allows partclone to run a little bit faster.
  - By default, Image 0002 reset the checksum after writting it. This allows to verify a "checksum strip"
    without reading all the blocks since the beginning of the image.
