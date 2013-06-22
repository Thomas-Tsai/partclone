/**
 * jfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read reiserfs super block and bitmap
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */


#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <sys/types.h>
#include <linux/types.h>

#include <jfs/jfs_types.h>
#include <jfs/jfs_superblock.h>
#include <jfs/jfs_dmap.h>
#include <jfs/jfs_imap.h>
#include <jfs/jfs_filsys.h>
#include <jfs/jfs_logmgr.h>
#include <jfs/jfs_dinode.h>
#include <jfs/jfs_xtree.h>
#include <jfs/jfs_byteorder.h>
#include <jfs/devices.h>
#include <jfs/super.h>

#include "partclone.h"
#include "jfsclone.h"
#include "progress.h"
#include "fs_common.h"

/* DMAP BLOCK TYPES	*/
#define DMAP	-1
#define LEVEL0	0
#define LEVEL1	1
#define LEVEL2	2

/* DMAP BLOCK CALCULATIONS */
#define L0FACTOR	(LPERCTL + 1) //LPERCTL=1024
#define L1FACTOR	((L0FACTOR * LPERCTL) + 1)
#define L1PAGE(l1)	(2 + ((l1) * L1FACTOR))
#define L0PAGE(l1, l0)	(L1PAGE(l1) + 1 + ((l0) * L0FACTOR))
#define DMAPPAGE(l1, l0, d)	(L0PAGE(l1, l0) + 1 + (d))

#define XT_CMP(CMP, K, X) \
{ \
    int64_t offset64 = offsetXAD(X); \
    (CMP) = ((K) >= offset64 + lengthXAD(X)) ? 1 : \
    ((K) < offset64) ? -1 : 0 ; \
}


int ujfs_rwdaddr(FILE *, int64_t *, struct dinode *, int64_t, int32_t, int32_t);
static int decode_pagenum(int64_t page, int *l1, int *l0, int *dmap);
static int find_iag(unsigned iagnum, unsigned which_table, int64_t * address);
static int find_inode(unsigned inum, unsigned which_table, int64_t * address);
static int xRead(int64_t, unsigned, char *);
static int jfs_bit_inuse(unsigned *bitmap, uint64_t block);
static void get_all_used_blocks(uint64_t *total_blocks, uint64_t *used_blocks);

struct superblock sb;
FILE *fp;
int bsize;
short l2bsize;
unsigned jfs_type;

char *EXECNAME = "partclone.jfs";
extern fs_cmd_opt fs_opt;

/// libfs/jfs_endian.h
#define GET 0

/// libfs/devices.h

/// xpeek.h
#define AGGREGATE_2ND_I -1
/// open device
static void fs_open(char* device){
    fp = fopen(device, "r+");
    if (fp == NULL) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: Cannot open device %s.\n", __FILE__, device);
    }

    if (ujfs_get_superblk(fp, &sb, 1)) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s: error reading primary superblock\n", __FILE__);
	if (ujfs_get_superblk(fp, &sb, 0)) {
	    log_mesg(0, 1, 1, fs_opt.debug, "%s: error reading secondary superblock\n", __FILE__);
	} else
	    log_mesg(1, 0, 0, fs_opt.debug, "%s: using secondary superblock\n", __FILE__);
    }
    jfs_type = sb.s_flag;
    l2bsize = sb.s_l2bsize;
    bsize = sb.s_bsize;

    log_mesg(1, 0, 0, fs_opt.debug, "%s: type = %u\n", __FILE__, sb.s_flag);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: block size = %i\n", __FILE__, sb.s_bsize);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: magic = %s\n", __FILE__, sb.s_magic);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: version = %u\n", __FILE__, sb.s_version);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: blocks = %lli\n", __FILE__, sb.s_size);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: l2bsize = %i\n", __FILE__, sb.s_l2bsize);
}

/// close device
static void fs_close(){
    fclose(fp);
}

void read_bitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui) {

    int64_t lblock = 0;
    int64_t address;
    int64_t cntl_addr;
    int ret = 1;
    struct dinode bmap_inode;
    struct dbmap cntl_page;
    int dmap_l2bpp;
    int64_t d_address;
    struct dmap d_map;
    int dmap_i, l0, l1;
    int next = 1;
    uint64_t tub=0;
    int64_t tb=0;
    int64_t pb = 1;
    int64_t block_used = 0;
    int64_t block_free = 0;
    uint64_t logloc = 0;
    int logsize = 0;
    int64_t dn_mapsize = 0;

    int start = 0;
    int bit_size = 1;

    fs_open(device);
    /// Read Blocal Allocation Map  Inode 
    ret = find_inode(BMAP_I, AGGREGATE_I, &address);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):find_node error.\n", __FILE__, __LINE__);
    }

    ret = xRead(address, sizeof(struct dinode), (char *) &bmap_inode);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error.\n", __FILE__, __LINE__);
    }

    /// Read overall control page for the map 
    ret = ujfs_rwdaddr(fp, &cntl_addr, &bmap_inode, (int64_t) 0, GET, bsize);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):ujfs_rwdaddr error.\n", __FILE__, __LINE__);
    }

    ret = xRead(cntl_addr, sizeof(struct dbmap), (char *) &cntl_page);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error %i\n", __FILE__, __LINE__);
    }
    dn_mapsize = cntl_page.dn_mapsize;
    dmap_l2bpp = cntl_page.dn_l2nbperpage;

    /// display leaf 

    lblock = 0; //control page
    decode_pagenum(lblock, &l1, &l0, &dmap_i);
    ret = ujfs_rwdaddr(fp, &d_address, &bmap_inode, (lblock) << dmap_l2bpp, GET, bsize);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):ujfs_rwdaddr error.\n", __FILE__, __LINE__);
    }

    ret = xRead(d_address, sizeof(struct dmap), (char *)&d_map);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error.\n", __FILE__, __LINE__);
    }

    /// ujfs_swap_dmap(&d_map); fixme

    log_mesg(2, 0, 0, fs_opt.debug, "%s: Dmap page at block %lld\n", __FILE__, (long long) (d_address >> sb.s_l2bsize));
    log_mesg(2, 0, 0, fs_opt.debug, "%s: control nblocks %d\n", __FILE__, d_map.nblocks);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: control nfree %d\n", __FILE__, d_map.nfree);
    lblock = 4; //First map page

    logloc  = addressPXD(&(sb.s_logpxd));
    logsize = lengthPXD(&(sb.s_logpxd));
    log_mesg(1, 0, 0, fs_opt.debug, "%s: logloc %lli, logsize %i\n", __FILE__, logloc, logsize);

    /// init progress
    progress_bar        prog;           /// progress_bar structure defined in progress.h
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);
    pc_init_bitmap(bitmap, 0xFF, image_hdr.totalblock);

    while(next){
	block_used = 0;
	block_free = 0;
	log_mesg(2, 0, 0, fs_opt.debug, "%s:lblock = %lli\n", __FILE__, lblock);
	decode_pagenum(lblock, &l1, &l0, &dmap_i);
	ret = ujfs_rwdaddr(fp, &d_address, &bmap_inode, (lblock) << dmap_l2bpp, GET, bsize);
	if (ret){
	    //log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):ujfs_rwdaddr error(%s).\n", __FILE__, __LINE__, strerror(ret));
	    log_mesg(2, 0, 0, fs_opt.debug, "%s(%i):ujfs_rwdaddr error(%s).\n", __FILE__, __LINE__, strerror(ret));
	}

	ret = xRead(d_address, sizeof(struct dmap), (char *)&d_map);
	if (ret){
	    log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error.\n", __FILE__, __LINE__);
	}

	///ujfs_swap_dmap(&d_map);

	log_mesg(2, 0, 0, fs_opt.debug, "%s: Dmap page at block %lld\n", __FILE__, (long long) (d_address >> sb.s_l2bsize));
	log_mesg(2, 0, 0, fs_opt.debug, "%s: nblocks %d\n", __FILE__, d_map.nblocks);
	log_mesg(2, 0, 0, fs_opt.debug, "%s: nfree %d\n", __FILE__, d_map.nfree);

	/// bitmap information not cover all partition

	/// display bitmap  

	block_used = 0;
	for (pb = 0; (pb < d_map.nblocks) && (tb < dn_mapsize); pb++){

	    if (tb > dn_mapsize)
		break;

	    if (jfs_bit_inuse(d_map.wmap, pb) == 1){
		block_used++;
		log_mesg(3, 0, 0, fs_opt.debug, "%s: used pb = %lli tb = %lli\n", __FILE__, pb, tb);
		pc_set_bit(tb, bitmap);
	    } else {
		block_free++;
		log_mesg(3, 0, 0, fs_opt.debug, "%s: free pb = %lli tb = %lli\n", __FILE__, pb, tb);
		pc_clear_bit(tb, bitmap);
	    }
	    tb++;
	    update_pui(&prog, tb, tb, 0);//keep update

	}

	log_mesg(2, 0, 0, fs_opt.debug, "%s:block_used %lli block_free %lli\n", __FILE__, block_used, block_free);
	tub += block_used;
	next = 0;
	if (((lblock-4)*d_map.nblocks)<image_hdr.totalblock){
	    lblock++;
	    next = 1;
	}
        if (tb >= dn_mapsize)
            next = 0;
    }

    log_mesg(2, 0, 0, fs_opt.debug, "%s:data_used = %llu\n", __FILE__, tub);
    block_used = 0;
    /// log

    log_mesg(2, 0, 0, fs_opt.debug, "%s:%llu log %llu\n", __FILE__, logloc, (logloc+logsize));
    for (;tb <= image_hdr.totalblock; tb++){

	if ((tb >= logloc) && (tb < (logloc+logsize))){
	    pc_set_bit(tb, bitmap);
	    block_used++;
	    log_mesg(3, 0, 0, fs_opt.debug, "%s: log used tb = %llu\n", __FILE__, pb, tb);
	}
	update_pui(&prog, tb, tb, 0);//keep update

    }

    log_mesg(2, 0, 0, fs_opt.debug, "%s:log_used = %llu\n", __FILE__, block_used);
    log_mesg(1, 0, 0, fs_opt.debug, "%s:total_used = %llu\n", __FILE__, tub+block_used);
    fs_close();
    update_pui(&prog, 1, 1, 1);//finish

}


void initial_image_hdr(char* device, image_head* image_hdr) {
    uint64_t used_blocks = 0;
    uint64_t total_blocks = 0;
    fs_open(device);
    get_all_used_blocks(&total_blocks, &used_blocks);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, jfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = (int)sb.s_bsize;
    image_hdr->totalblock = (unsigned long long)total_blocks;
    image_hdr->usedblocks = (unsigned long long)used_blocks;
    image_hdr->device_size =(unsigned long long)(total_blocks * sb.s_bsize);
    fs_close();
}

/// get_all_used_blocks
extern void get_all_used_blocks(uint64_t *total_blocks, uint64_t *used_blocks){

    int64_t address;
    int64_t cntl_addr;
    int ret = 1;
    struct dinode bmap_inode;
    struct dbmap cntl_page;
    int64_t bytes_on_device = 0;
    int logsize = 0;


    /// Read Blocal Allocation Map  Inode 
    ret = find_inode(BMAP_I, AGGREGATE_I, &address);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):find_node error.\n", __FILE__, __LINE__);
    }

    ret = xRead(address, sizeof(struct dinode), (char *) &bmap_inode);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error.\n", __FILE__, __LINE__);
    }

    /// Read overall control page for the map 
    ret = ujfs_rwdaddr(fp, &cntl_addr, &bmap_inode, (int64_t) 0, GET, bsize);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):ujfs_rwdaddr error.\n", __FILE__, __LINE__);
    }

    ret = xRead(cntl_addr, sizeof(struct dbmap), (char *) &cntl_page);
    if (ret){
	log_mesg(0, 1, 1, fs_opt.debug, "%s(%i):xRead error.\n", __FILE__, __LINE__);
    }
    logsize = lengthPXD(&(sb.s_logpxd));
    ret = ujfs_get_dev_size(fp, &bytes_on_device);
    *total_blocks = bytes_on_device / sb.s_bsize;
    *used_blocks = (cntl_page.dn_mapsize - cntl_page.dn_nfree + logsize);

    log_mesg(2, 0, 0, fs_opt.debug, "%s: dn.mapsize = %lli\n", __FILE__, cntl_page.dn_mapsize);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: total_blocks = %lli\n", __FILE__, *total_blocks);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: free_blocks = %lli\n", __FILE__, cntl_page.dn_nfree);
    log_mesg(2, 0, 0, fs_opt.debug, "%s: log_blocks = %i\n", __FILE__, logsize);

}



int decode_pagenum(int64_t page, int *l1, int *l0, int *dmap)
{
    int remainder;

    if (page == 0)
	return -1;

    if (page == 1)
	return LEVEL2;

    *l1 = (page - 2) / L1FACTOR;
    remainder = (page - 2) % L1FACTOR;
    if (remainder == 0)
	return LEVEL1;

    *l0 = (remainder - 1) / L0FACTOR;
    remainder = (remainder - 1) % L0FACTOR;
    if (remainder == 0)
	return LEVEL0;

    *dmap = remainder - 1;
    return DMAP;
}


int find_inode(unsigned inum, unsigned which_table, int64_t * address)
{
    int extnum;
    struct iag iag;
    int iagnum;
    int64_t map_address;
    int rc;

    iagnum = INOTOIAG(inum);
    extnum = (inum & (INOSPERIAG - 1)) >> L2INOSPEREXT;
    if (find_iag(iagnum, which_table, &map_address) == 1)
	return 1;

    rc = ujfs_rw_diskblocks(fp, map_address, sizeof (struct iag), &iag, GET);
    if (rc) {
	log_mesg(1, 1, 1, fs_opt.debug, "%s: find_inode: Error reading iag\n", __FILE__);
	return 1;
    }

    /* swap if on big endian machine */
    ///ujfs_swap_iag(&iag);

    if (iag.inoext[extnum].len == 0)
	return 1;

    *address = (addressPXD(&iag.inoext[extnum]) << l2bsize) + ((inum & (INOSPEREXT - 1)) * sizeof (struct dinode));
    return 0;
}

int xRead(int64_t address, unsigned count, char *buffer)
{
    int64_t block_address;
    char *block_buffer;
    int64_t length;
    unsigned offset;

    offset = address & (bsize - 1);
    length = (offset + count + bsize - 1) & ~(bsize - 1);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: offset %i, length %lli, count %i, address %lli\n", __FILE__, offset , length , count , address);

    if ((offset == 0) & (length == count))
	return ujfs_rw_diskblocks(fp, address, count, buffer, GET);

    block_address = address - offset;
    block_buffer = (char *) malloc(length);
    if (block_buffer == 0)
	return 1;

    if (ujfs_rw_diskblocks(fp, block_address, length, block_buffer, GET)) {
	log_mesg(3, 0, 0, fs_opt.debug, "%s: block_address %lli , length %lli , GET %i \n", __FILE__, block_address, length, GET);
	free(block_buffer);
	return 1;
    }
    memcpy(buffer, block_buffer + offset, count);
    free(block_buffer);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: done: offset %i, length %lli, count %i, address %lli\n", __FILE__, offset , length , count , address);
    return 0;
}

int find_iag(unsigned iagnum, unsigned which_table, int64_t * address)
{
    int base;
    char buffer[PSIZE];
    int cmp;
    struct dinode fileset_inode;
    int64_t fileset_inode_address;
    int64_t iagblock;
    int index;
    int lim;
    xtpage_t *page;
    int rc;

    int64_t AIT_2nd_offset = addressPXD(&(sb.s_ait2)) * sb.s_bsize;

    if (which_table != FILESYSTEM_I &&
	    which_table != AGGREGATE_I && which_table != AGGREGATE_2ND_I) {
	log_mesg(1, 1, 1, fs_opt.debug, "%s: find_iag: Invalid fileset, %d\n", __FILE__, which_table);
	return 1;
    }
    iagblock = IAGTOLBLK(iagnum, L2PSIZE - l2bsize);

    if (which_table == AGGREGATE_2ND_I) {
	fileset_inode_address = AIT_2nd_offset + sizeof (struct dinode);
    } else {
	fileset_inode_address = AGGR_INODE_TABLE_START + (which_table * sizeof (struct dinode));
    }
    rc = xRead(fileset_inode_address, sizeof (struct dinode), (char *) &fileset_inode);
    if (rc) {
	log_mesg(1, 1, 1, fs_opt.debug, "%s: find_inode: Error reading fileset inode\n", __FILE__);
	return 1;
    }

    page = (xtpage_t *) & (fileset_inode.di_btroot);

descend:
    /* Binary search */
    for (base = XTENTRYSTART,
	    lim = __le16_to_cpu(page->header.nextindex) - XTENTRYSTART; lim; lim >>= 1) {
	index = base + (lim >> 1);
	XT_CMP(cmp, iagblock, &(page->xad[index]));
	if (cmp == 0) {
	    /* HIT! */
	    if (page->header.flag & BT_LEAF) {
		*address = (addressXAD(&(page->xad[index]))
			+ (iagblock - offsetXAD(&(page->xad[index]))))
		    << l2bsize;
		return 0;
	    } else {
		rc = xRead(addressXAD(&(page->xad[index])) << l2bsize,
			PSIZE, buffer);
		if (rc) {
		    log_mesg(1, 1, 1, fs_opt.debug, "%s: find_iag: Error reading btree node\n", __FILE__);
		    return 1;
		}
		page = (xtpage_t *) buffer;

		goto descend;
	    }
	} else if (cmp > 0) {
	    base = index + 1;
	    --lim;
	}
    }

    if (page->header.flag & BT_INTERNAL) {
	/* Traverse internal page, it might hit down there
	 * If base is non-zero, decrement base by one to get the parent
	 * entry of the child page to search.
	 */
	index = base ? base - 1 : base;

	rc = xRead(addressXAD(&(page->xad[index])) << l2bsize, PSIZE, buffer);
	if (rc) {
	    log_mesg(1, 1, 1, fs_opt.debug, "%s: find_iag: Error reading btree node\n", __FILE__);
	    return 1;
	}
	page = (xtpage_t *) buffer;

	goto descend;
    }

    /* Not found! */
    log_mesg(1, 1, 1, fs_opt.debug, "%s: find_iag:  IAG %d not found!\n", __FILE__, iagnum);
    return 1;
}

int jfs_bit_inuse(unsigned *bitmap, uint64_t block){
    uint64_t byte = 0;
    int bit = 0;

    byte = block/32;
    bit  = 31 - (block%32);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: bitmap = %08x\n", __FILE__, bitmap[byte]);	

    if(bitmap[byte] & (1<<bit))
	return 1;
    else
	return 0;
}


