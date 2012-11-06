/**
 * xfsclone.c - part of Partclone project
 *
 * Copyright (c) 2007~ Thomas Tsai <thomas at nchc org tw>
 *
 * read xfs super block and bitmap, reference xfs_copy.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <sys/param.h>
#include <xfs/libxfs.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <malloc.h>
#include <stdarg.h>
#include <getopt.h>
#include "partclone.h"
#include "xfsclone.h"
#include "progress.h"
#include "fs_common.h"

char	*EXECNAME = "partclone.xfs";
extern  fs_cmd_opt fs_opt;
int	source_fd = -1;
int     first_residue;

xfs_mount_t     *mp;
xfs_mount_t     mbuf;
libxfs_init_t   xargs;
unsigned int    source_blocksize;       /* source filesystem blocksize */
unsigned int    source_sectorsize;      /* source disk sectorsize */

#define rounddown(x, y) (((x)/(y))*(y))

static void set_bitmap(unsigned long* bitmap, uint64_t pos, int length)
{
    uint64_t pos_block;
    uint64_t block_count;
    uint64_t block;

    pos_block   = pos/source_blocksize;
    block_count = length/source_blocksize;

    for (block = pos_block; block < pos_block+block_count; block++){
	pc_set_bit(block, bitmap);
	log_mesg(3, 0, 0, fs_opt.debug, "block %i is used\n", block);
    }

}

static void fs_open(char* device)
{

    int		    open_flags;
    struct stat     statbuf;
    int		    source_is_file = 0;
    xfs_buf_t       *sbp;
    xfs_sb_t        *sb;
    int             tmp_residue;

    /* open up source -- is it a file? */
    open_flags = O_RDONLY;

    if ((source_fd = open(device, open_flags)) < 0)  {
	log_mesg(0, 1, 1, fs_opt.debug, "Couldn't open source partition %s\n", device);
    }else{
	log_mesg(0, 0, 0, fs_opt.debug, "Open %s successfully", device);
	}

    if (fstat(source_fd, &statbuf) < 0)  {
	log_mesg(0, 1, 1, fs_opt.debug, "Couldn't stat source partition\n");
    }

    if (S_ISREG(statbuf.st_mode)){
	log_mesg(1, 0, 0, fs_opt.debug, "source is file\n");
	source_is_file = 1;
    }

    /* prepare the libxfs_init structure */

    memset(&xargs, 0, sizeof(xargs));
    xargs.isdirect = LIBXFS_DIRECT;
    xargs.isreadonly = LIBXFS_ISREADONLY;

    if (source_is_file)  {
	xargs.dname = device;
	xargs.disfile = 1;
    } else
	xargs.volname = device;

    if (libxfs_init(&xargs) == 0)
    {
	log_mesg(0, 1, 1, fs_opt.debug, "libxfs_init error. Please repaire %s partition.\n", device);
    }

    /* prepare the mount structure */
    sbp = libxfs_readbuf(xargs.ddev, XFS_SB_DADDR, 1, 0);
    memset(&mbuf, 0, sizeof(xfs_mount_t));
    sb = &mbuf.m_sb;
    libxfs_sb_from_disk(sb, XFS_BUF_TO_SBP(sbp));

    mp = libxfs_mount(&mbuf, sb, xargs.ddev, xargs.logdev, xargs.rtdev, 1);
    if (mp == NULL) {
	log_mesg(0, 1, 1, fs_opt.debug, "%s filesystem failed to initialize\nAborting.\n", device);
    } else if (mp->m_sb.sb_inprogress)  {
	log_mesg(0, 1, 1, fs_opt.debug, "%s filesystem failed to initialize\nAborting(inprogress).\n", device);
    } else if (mp->m_sb.sb_logstart == 0)  {
	log_mesg(0, 1, 1, fs_opt.debug, "%s has an external log.\nAborting.\n", device);
    } else if (mp->m_sb.sb_rextents != 0)  {
	log_mesg(0, 1, 1, fs_opt.debug, "%s has a real-time section.\nAborting.\n", device);
    }

    source_blocksize = mp->m_sb.sb_blocksize;
    source_sectorsize = mp->m_sb.sb_sectsize;
    log_mesg(2, 0, 0, fs_opt.debug, "source_blocksize %i source_sectorsize = %i\n", source_blocksize, source_sectorsize);

    if (source_blocksize > source_sectorsize)  {
	/* get number of leftover sectors in last block of ag header */

	tmp_residue = ((XFS_AGFL_DADDR(mp) + 1) * source_sectorsize) % source_blocksize;
	first_residue = (tmp_residue == 0) ? 0 :  source_blocksize - tmp_residue;
	log_mesg(2, 0, 0, fs_opt.debug, "first_residue %i tmp_residue %i\n", first_residue, tmp_residue);
    } else if (source_blocksize == source_sectorsize)  {
	first_residue = 0;
    } else  {
	log_mesg(0, 1, 1, fs_opt.debug, "Error:  filesystem block size is smaller than the disk sectorsize.\nAborting XFS copy now.\n");
    }

    /* end of xfs open */
    log_mesg(3, 0, 0, fs_opt.debug, "%s: blcos size= %i\n", __FILE__, mp->m_sb.sb_blocksize);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: total b= %lli\n", __FILE__, mp->m_sb.sb_dblocks);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: free block= %lli\n", __FILE__, mp->m_sb.sb_fdblocks);
    log_mesg(3, 0, 0, fs_opt.debug, "%s: used block= %lli\n", __FILE__, (mp->m_sb.sb_dblocks - mp->m_sb.sb_fdblocks));
    log_mesg(3, 0, 0, fs_opt.debug, "%s: device size= %lli\n", __FILE__, (mp->m_sb.sb_blocksize * mp->m_sb.sb_dblocks));

}

static void fs_close()
{
    libxfs_device_close(xargs.ddev);
    log_mesg(0, 0, 0, fs_opt.debug, "fs_close\n");
}

extern void initial_image_hdr(char* device, image_head* image_hdr)
{
    fs_open(device);
    strncpy(image_hdr->magic, IMAGE_MAGIC, IMAGE_MAGIC_SIZE);
    strncpy(image_hdr->fs, xfs_MAGIC, FS_MAGIC_SIZE);
    image_hdr->block_size = mp->m_sb.sb_blocksize;
    image_hdr->totalblock = mp->m_sb.sb_dblocks;
    image_hdr->usedblocks = mp->m_sb.sb_dblocks - mp->m_sb.sb_fdblocks;
    image_hdr->device_size = (image_hdr->totalblock * image_hdr->block_size);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: blcos size= %i\n", __FILE__, mp->m_sb.sb_blocksize);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: total b= %lli\n", __FILE__, mp->m_sb.sb_dblocks);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: free block= %lli\n", __FILE__, mp->m_sb.sb_fdblocks);
    log_mesg(1, 0, 0, fs_opt.debug, "%s: used block= %lli\n", __FILE__, (mp->m_sb.sb_dblocks - mp->m_sb.sb_fdblocks));
    log_mesg(1, 0, 0, fs_opt.debug, "%s: device size= %lli\n", __FILE__, (mp->m_sb.sb_blocksize*mp->m_sb.sb_dblocks));
    fs_close();

}

extern void readbitmap(char* device, image_head image_hdr, unsigned long* bitmap, int pui)
{

    xfs_agnumber_t  agno = 0;
    xfs_agblock_t   first_agbno;
    xfs_agnumber_t  num_ags;
    ag_header_t     ag_hdr;
    xfs_daddr_t     read_ag_off;
    int             read_ag_length;
    void            *read_ag_buf = NULL;
    xfs_off_t	    read_ag_position;            /* xfs_types.h: typedef __s64 */
    uint64_t	    sk, res, s_pos = 0;
    void            *btree_buf_data = NULL;
    int		    btree_buf_length;
    xfs_off_t	    btree_buf_position;
    xfs_agblock_t   bno;
    uint	    current_level;
    uint	    btree_levels;
    xfs_daddr_t     begin, next_begin, ag_begin, new_begin, ag_end;
						/* xfs_types.h: typedef __s64*/
    xfs_off_t       pos;
    xfs_alloc_ptr_t *ptr;
    xfs_alloc_rec_t *rec_ptr;
    int		    length;
    int		    i;
    uint64_t        size, sizeb;
    xfs_off_t	    w_position;
    int		    w_length;
    int		    wblocks;
    int		    w_size = 1 * 1024 * 1024;
    uint64_t        numblocks = 0;

    xfs_off_t	    logstart, logend;
    xfs_off_t	    logstart_pos, logend_pos;
    int		    log_length;

    struct xfs_btree_block *block;
    uint64_t current_block, block_count, prog_cur_block = 0;

    int start = 0;
    int bit_size = 1;
    progress_bar        prog;

    uint64_t bused = 0;
    uint64_t bfree = 0;

    /// init progress
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    fs_open(device);

    first_agbno = (((XFS_AGFL_DADDR(mp) + 1) * source_sectorsize) + first_residue) / source_blocksize;

    num_ags = mp->m_sb.sb_agcount;

    log_mesg(1, 0, 0, fs_opt.debug, "ags = %i\n", num_ags);
    for (agno = 0; agno < num_ags ; agno++)  {
	/* read in first blocks of the ag */

	/* initial settings */
	log_mesg(2, 0, 0, fs_opt.debug, "read ag %i header\n", agno);

	read_ag_off = XFS_AG_DADDR(mp, agno, XFS_SB_DADDR);
	read_ag_length = first_agbno * source_blocksize;
	read_ag_position = (xfs_off_t) read_ag_off * (xfs_off_t) BBSIZE;
	read_ag_buf = malloc(read_ag_length);
	if(read_ag_buf == NULL){
	    log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}
	memset(read_ag_buf, 0, read_ag_length);

	log_mesg(2, 0, 0, fs_opt.debug, "seek to read_ag_position %lli\n", read_ag_position);
	sk = lseek(source_fd, read_ag_position, SEEK_SET);
	current_block = (sk/source_blocksize);
	block_count = (read_ag_length/source_blocksize);
	set_bitmap(bitmap, sk, read_ag_length);
	log_mesg(2, 0, 0, fs_opt.debug, "read ag header fd = %llu(%i), length = %i(%i)\n", sk, current_block, read_ag_length, block_count);
	if ((res = read(source_fd, read_ag_buf, read_ag_length)) < 0)  {
	    log_mesg(1, 0, 1, fs_opt.debug, "read failure at offset %lld\n", read_ag_position);
	}

	ag_hdr.xfs_sb = (xfs_dsb_t *) (read_ag_buf);
	ASSERT(be32_to_cpu(ag_hdr.xfs_sb->sb_magicnum) == XFS_SB_MAGIC);
	ag_hdr.xfs_agf = (xfs_agf_t *) (read_ag_buf + source_sectorsize);
	ASSERT(be32_to_cpu(ag_hdr.xfs_agf->agf_magicnum) == XFS_AGF_MAGIC);
	ag_hdr.xfs_agi = (xfs_agi_t *) (read_ag_buf + 2 * source_sectorsize);
	ASSERT(be32_to_cpu(ag_hdr.xfs_agi->agi_magicnum) == XFS_AGI_MAGIC);
	ag_hdr.xfs_agfl = (xfs_agfl_t *) (read_ag_buf + 3 * source_sectorsize);
	log_mesg(2, 0, 0, fs_opt.debug, "ag header read ok\n");


	/* save what we need (agf) in the btree buffer */

	btree_buf_data = malloc(source_blocksize);
	if(btree_buf_data == NULL){
	    log_mesg(0, 1, 1, fs_opt.debug, "%s, %i, ERROR:%s", __func__, __LINE__, strerror(errno));
	}
	memset(btree_buf_data, 0, source_blocksize);
	memmove(btree_buf_data, ag_hdr.xfs_agf, source_sectorsize);
	ag_hdr.xfs_agf = (xfs_agf_t *) btree_buf_data;
	btree_buf_length = source_blocksize;

	///* traverse btree until we get to the leftmost leaf node */

	bno = be32_to_cpu(ag_hdr.xfs_agf->agf_roots[XFS_BTNUM_BNOi]);
	current_level = 0;
	btree_levels = be32_to_cpu(ag_hdr.xfs_agf->agf_levels[XFS_BTNUM_BNOi]);

	ag_end = XFS_AGB_TO_DADDR(mp, agno, be32_to_cpu(ag_hdr.xfs_agf->agf_length) - 1) + source_blocksize / BBSIZE;

	for (;;) {
	    /* none of this touches the w_buf buffer */

	    current_level++;

	    btree_buf_position = pos = (xfs_off_t)XFS_AGB_TO_DADDR(mp,agno,bno) << BBSHIFT;
	    btree_buf_length = source_blocksize;

	    sk = lseek(source_fd, btree_buf_position, SEEK_SET);
	    current_block = (sk/source_blocksize);
	    block_count = (btree_buf_length/source_blocksize);
	    set_bitmap(bitmap, sk, btree_buf_length);
	    log_mesg(2, 0, 0, fs_opt.debug, "read btree sf = %llu(%i), length = %i(%i)\n", sk, current_block, btree_buf_length, block_count);
	    read(source_fd, btree_buf_data, btree_buf_length);
	    block = (struct xfs_btree_block *)((char *)btree_buf_data + pos - btree_buf_position);

	    if (be16_to_cpu(block->bb_level) == 0)
		break;

	    ptr = XFS_ALLOC_PTR_ADDR(mp, block, 1, mp->m_alloc_mxr[1]);
	    bno = be32_to_cpu(ptr[0]);
	}
	log_mesg(2, 0, 0, fs_opt.debug, "btree read done\n");

	/* align first data copy but don't overwrite ag header */

	pos = read_ag_position >> BBSHIFT;
	length = read_ag_length >> BBSHIFT;
	next_begin = pos + length;
	ag_begin = next_begin;


	///* handle the rest of the ag */

	for (;;) {
	    if (be16_to_cpu(block->bb_level) != 0)  {
		log_mesg(0, 1, 1, fs_opt.debug, "WARNING:  source filesystem inconsistent.\nA leaf btree rec isn't a leaf. Aborting now.\n");
	    }

	    rec_ptr = XFS_ALLOC_REC_ADDR(mp, block, 1);
	    for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++, rec_ptr++)  {
		/* calculate in daddr's */

		begin = next_begin;

		/*
		 * protect against pathological case of a
		 * hole right after the ag header in a
		 * mis-aligned case
		 */

		if (begin < ag_begin)
		    begin = ag_begin;

		/*
		 * round size up to ensure we copy a
		 * range bigger than required
		 */

		log_mesg(3, 0, 0, fs_opt.debug, "XFS_AGB_TO_DADDR = %llu, agno = %i, be32_to_cpu=%llu\n", XFS_AGB_TO_DADDR(mp, agno, be32_to_cpu(rec_ptr->ar_startblock)), agno, be32_to_cpu(rec_ptr->ar_startblock));
		sizeb = XFS_AGB_TO_DADDR(mp, agno, be32_to_cpu(rec_ptr->ar_startblock)) - begin;
		size = roundup(sizeb <<BBSHIFT, source_sectorsize);
		log_mesg(3, 0, 0, fs_opt.debug, "BB = %i size %i and sizeb %llu brgin = %llu\n", BBSHIFT, size, sizeb, begin);

		if (size > 0)  {
		    /* copy extent */

		    log_mesg(2, 0, 0, fs_opt.debug, "copy extent\n");
		    w_position = (xfs_off_t)begin << BBSHIFT;

		    while (size > 0)  {
			/*
			 * let lower layer do alignment
			 */
			if (size > w_size)  {
			    w_length = w_size;
			    size -= w_size;
			    sizeb -= wblocks;
			    numblocks += wblocks;
			} else  {
			    w_length = size;
			    numblocks += sizeb;
			    size = 0;
			}

			//read_wbuf(source_fd, &w_buf, mp);
			sk = lseek(source_fd, w_position, SEEK_SET);
			current_block = (sk/source_blocksize);
			block_count = (w_length/source_blocksize);
			set_bitmap(bitmap, sk, w_length);
			log_mesg(2, 0, 0, fs_opt.debug, "read ext sourcefd to w_buf source_fd=%llu(%i), length=%i(%i)\n", sk, current_block, w_length, block_count);
			sk = lseek(source_fd, w_length, SEEK_CUR);

			w_position += w_length;
		    }
		}

		/* round next starting point down */

		new_begin = XFS_AGB_TO_DADDR(mp, agno, be32_to_cpu(rec_ptr->ar_startblock) + be32_to_cpu(rec_ptr->ar_blockcount));
		next_begin = rounddown(new_begin, source_sectorsize >> BBSHIFT);
	    }

	    if (be32_to_cpu(block->bb_u.s.bb_rightsib) == NULLAGBLOCK){
		log_mesg(2, 0, 0, fs_opt.debug, "NULLAGBLOCK\n");
		break;
	    }

	    /* read in next btree record block */

	    btree_buf_position = pos = (xfs_off_t)XFS_AGB_TO_DADDR(mp, agno, be32_to_cpu(block->bb_u.s.bb_rightsib)) << BBSHIFT;
	    btree_buf_length = source_blocksize;

	    /* let read_wbuf handle alignment */

	    //read_wbuf(source_fd, &btree_buf, mp);
	    sk = lseek(source_fd, btree_buf_position, SEEK_SET);
	    current_block = (sk/source_blocksize);
	    block_count = (btree_buf_length/source_blocksize);
	    set_bitmap(bitmap, sk, btree_buf_length);
	    log_mesg(2, 0, 0, fs_opt.debug, "read btreebuf fd = %llu(%i), length = %i(%i) \n", sk, current_block, btree_buf_length, block_count);
	    read(source_fd, btree_buf_data, btree_buf_length);

	    block = (struct xfs_btree_block *)((char *) btree_buf_data + pos - btree_buf_position);
	    ASSERT(be32_to_cpu(block->bb_magic) == XFS_ABTB_MAGIC);

	}
	/*
	 * write out range of used blocks after last range
	 * of free blocks in AG
	 */
	if (next_begin < ag_end)  {
	    begin = next_begin;

	    sizeb = ag_end - begin;
	    size = roundup(sizeb << BBSHIFT, source_sectorsize);

	    if (size > 0)  {
		/* copy extent */

		w_position = (xfs_off_t) begin << BBSHIFT;

		while (size > 0)  {
		    /*
		     * let lower layer do alignment
		     */
		    if (size > w_size)  {
			w_length = w_size;
			size -= w_size;
			sizeb -= wblocks;
			numblocks += wblocks;
		    } else  {
			w_length = size;
			numblocks += sizeb;
			size = 0;
		    }

		    sk = lseek(source_fd, w_position, SEEK_SET);
		    current_block = (sk/source_blocksize);
		    block_count = (w_length/source_blocksize);
		    set_bitmap(bitmap, sk, w_length);
		    log_mesg(2, 0, 0, fs_opt.debug, "read ext fd = %llu(%i), length = %i(%i)\n", sk, current_block, w_length, block_count);
		    //read_wbuf(source_fd, &w_buf, mp);
		    lseek(source_fd, w_length, SEEK_CUR);
		    w_position += w_length;
		}
	    }
	}

	log_mesg(2, 0, 0, fs_opt.debug, "write a clean log\n");
	log_length = 1 * 1024 * 1024;
	logstart = XFS_FSB_TO_DADDR(mp, mp->m_sb.sb_logstart) << BBSHIFT;
	logstart_pos = rounddown(logstart, (xfs_off_t)log_length);
	if (logstart % log_length)  {  /* unaligned */
	    sk = lseek(source_fd, logstart_pos, SEEK_SET);
	    current_block = (sk/source_blocksize);
	    block_count = (log_length/source_blocksize);
	    set_bitmap(bitmap, sk, log_length);
	    log_mesg(2, 0, 0, fs_opt.debug, "read log start from %llu(%i) %i(%i)\n", sk, current_block, log_length, block_count);
	    sk = lseek(source_fd, log_length, SEEK_CUR);
	}

	logend = XFS_FSB_TO_DADDR(mp, mp->m_sb.sb_logstart) << BBSHIFT;
	logend += XFS_FSB_TO_B(mp, mp->m_sb.sb_logblocks);
	logend_pos = rounddown(logend, (xfs_off_t)log_length);
	if (logend % log_length)  {    /* unaligned */
	    sk = lseek(source_fd, logend_pos, SEEK_SET);
	    current_block = (sk/source_blocksize);
	    block_count = (log_length/source_blocksize);
	    set_bitmap(bitmap, sk, log_length);
	    log_mesg(2, 0, 0, fs_opt.debug, "read log end  from %llu(%i) %i(%i)\n", sk, current_block, log_length, block_count);
	    sk = lseek(source_fd, log_length, SEEK_CUR);
	}

	log_mesg(2, 0, 0, fs_opt.debug, "write a clean log done\n");

	prog_cur_block = image_hdr.totalblock/num_ags*(agno+1)-1;
	update_pui(&prog, prog_cur_block, prog_cur_block, 0);
    }

    for(current_block = 0; current_block <= image_hdr.totalblock; current_block++){
	if(pc_test_bit(current_block, bitmap))
	    bused++;
	else
	    bfree++;
    }
    log_mesg(0, 0, 0, fs_opt.debug, "bused = %lli, bfree = %lli\n", bused, bfree);

    fs_close();
    update_pui(&prog, 1, 1, 1);

}

