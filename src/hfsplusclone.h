/**
 * hfsplusclone.h - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read hfsplus super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


typedef uint8_t  UInt8;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef uint64_t UInt64;

typedef UInt32 HFSCatalogNodeID;

struct HFSPlusExtentDescriptor {
    UInt32                  startBlock;
    UInt32                  blockCount;
};
typedef struct HFSPlusExtentDescriptor HFSPlusExtentDescriptor;
//typedef struct HFSPlusExtentDescriptor HFSPlusExtentRecord;

struct HFSPlusForkData {
    UInt64                  logicalSize;
    UInt32                  clumpSize;
    UInt32                  totalBlocks;
    HFSPlusExtentDescriptor extents[8];
};
typedef struct HFSPlusForkData HFSPlusForkData;


struct HFSPlusVolumeHeader {
    UInt16              signature;
    UInt16              version;
    UInt32              attributes;
    UInt32              lastMountedVersion;
    UInt32              journalInfoBlock;
    UInt32              createDate;
    UInt32              modifyDate;
    UInt32              backupDate;
    UInt32              checkedDate;
    UInt32              fileCount;
    UInt32              folderCount;
    UInt32              blockSize;
    UInt32              totalBlocks;
    UInt32              freeBlocks;
    UInt32              nextAllocation;
    UInt32              rsrcClumpSize;
    UInt32              dataClumpSize;
    HFSCatalogNodeID    nextCatalogID;
    UInt32              writeCount;
    UInt64              encodingsBitmap;
    UInt32              finderInfo[8];
    HFSPlusForkData     allocationFile;
    HFSPlusForkData     extentsFile;
    HFSPlusForkData     catalogFile;
    HFSPlusForkData     attributesFile;
    HFSPlusForkData     startupFile;

};
typedef struct HFSPlusVolumeHeader HFSPlusVolumeHeader;

// Bytes from start of the volume to the header
#define HFSHeaderOffset (2 * PART_SECTOR_SIZE)

#define HFSPlusSignature 0x482b // H+
#define HFSXSignature 0x4858 // HX

/*
 * HFS types, for handling HFS+ wrapper volumes.
 *
 * HFS+ volumes can be embedded inside an HFS "wrapper" volume. This is necessary for
 * Mac OS 9 (or earlier) to boot from HFS+, and was common in HFS filesystems created
 * up until Mac OS X 10.4.
 * 
 * See Inside Macintosh chapter 2 for HFS structures,
 * and Apple Technote 1150 for HFS+ wrappers
 */

struct HFSExtentDescriptor {
    UInt16 startBlock;
    UInt16 blockCount;
};
typedef struct HFSExtentDescriptor HFSExtentDescriptor;

typedef HFSExtentDescriptor HFSExtentRecord[3];

struct __attribute__ ((packed)) HFSVolumeHeader {
    UInt16 signature;
    UInt32 createDate;
    UInt32 modifyDate;
    UInt16 attributes;
    UInt16 rootFileCount;
    UInt16 bitmapOffset;
    UInt16 nextAllocation;
    UInt16 allocationBlockCount;
    UInt32 allocationBlockSize; // in bytes
    UInt32 clumpSize;
    UInt16 firstAllocationBlock; // in 512-byte blocks
    UInt32 nextCatalogID;
    UInt16 freeBlocks;
    char label[28]; // pascal string
    UInt32 backupDate;
    UInt16 backupSeqNumber;
    UInt32 writeCount;
    UInt32 overflowClumpSize;
    UInt32 catalogClumpSize;
    UInt16 rootDirCount;
    UInt32 fileCount;
    UInt32 dirCount;
    UInt32 finderInfo[8];
    UInt16 embedSignature;
    HFSExtentDescriptor embedExtent;
    UInt32 overflowSize;
    HFSExtentRecord overflowExtents;
    UInt32 catalogSize;
    HFSExtentRecord catalogExtents;
};
typedef struct HFSVolumeHeader HFSVolumeHeader;

#define HFSSignature 0x4244 // BD
