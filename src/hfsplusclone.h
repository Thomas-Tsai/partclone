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


typedef unsigned char      UInt8;
typedef unsigned short int UInt16;
typedef unsigned long int       UInt32;
typedef unsigned long long int  UInt64;

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

static short reverseShort(short s);

static int reverseInt(int i);

static int IsAllocationBlockUsed(UInt32 thisAllocationBlock, UInt8* allocationFileContents);

static void print_fork_data(HFSPlusForkData* fork);

/// open device
static void fs_open(char* device);

/// close device
static void fs_close();

/// read super block and write to image head
extern void initial_image_hdr(char* device, image_head* image_hdr);

/// readbitmap - cread and heck bitmap, reference dumpe2fs
extern void readbitmap(char* device, image_head image_hdr, char* bitmap, int pui);
