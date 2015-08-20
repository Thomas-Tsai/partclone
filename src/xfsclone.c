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

#define _LARGEFILE64_SOURCE
#include <xfs/libxfs.h>
#include <stdio.h>
#include "partclone.h"
#include "xfsclone.h"
#include "progress.h"
#include "fs_common.h"


#undef crc32
char	*EXECNAME = "partclone.xfs";
extern  fs_cmd_opt fs_opt;
int	source_fd = -1;
int     first_residue;

xfs_mount_t     *mp;
xfs_mount_t     mbuf;
libxfs_init_t   xargs;
unsigned int    source_blocksize;       /* source filesystem blocksize */
unsigned int    source_sectorsize;      /* source disk sectorsize */
progress_bar        prog;

unsigned long* xfs_bitmap;

#define rounddown(x, y) (((x)/(y))*(y))

void get_sb(xfs_sb_t *sbp, xfs_off_t off, int size, xfs_agnumber_t agno)
{
        xfs_dsb_t *buf;
	int rval = 0;

        buf = memalign(libxfs_device_alignment(), size);
        if (buf == NULL) {
                log_mesg(3, 1, 1, fs_opt.debug, "error reading superblock %u -- failed to memalign buffer\n", agno);
        }
        memset(buf, 0, size);
        memset(sbp, 0, sizeof(*sbp));

        /* try and read it first */

        if (lseek64(source_fd, off, SEEK_SET) != off)  {
                log_mesg(3, 0, 1, fs_opt.debug, "error reading superblock %u -- seek to offset %" PRId64 " failed\n", agno, off);
                free(buf);
        }

        if ((rval = read(source_fd, buf, size)) != size)  {
                log_mesg(3, 1, 1, fs_opt.debug, "superblock read failed, offset %" PRId64 ", size %d, ag %u, rval %d\n", off, size, agno, rval);
        }
        libxfs_sb_from_disk(sbp, buf);

        //rval = verify_sb((char *)buf, sbp, agno == 0);
        free(buf);
        //return rval;
}


static void set_bitmap(unsigned long* bitmap, uint64_t start, int count)
{
    uint64_t block;


    for (block = start; block < start+count; block++){
	pc_clear_bit(block, bitmap);
	log_mesg(3, 0, 0, fs_opt.debug, "block %i is free\n", block);
	update_pui(&prog, start, start, 0);
    }

}
// copy from xfs_db freesp ....


typedef enum typnm
{
    TYP_AGF, TYP_AGFL, TYP_AGI, TYP_ATTR, TYP_BMAPBTA,
    TYP_BMAPBTD, TYP_BNOBT, TYP_CNTBT, TYP_DATA,
    TYP_DIR2, TYP_DQBLK, TYP_INOBT, TYP_INODATA, TYP_INODE,
    TYP_LOG, TYP_RTBITMAP, TYP_RTSUMMARY, TYP_SB, TYP_SYMLINK,
    TYP_TEXT, TYP_NONE
} typnm_t;


static void
addtohist(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len)
{
	unsigned long long start_block;


	log_mesg(1, 0, 0, fs_opt.debug, "add %8d %8d %8d\n", agno, agbno, len);
	
	start_block = (agno*mp->m_sb.sb_agblocks) + agbno;
	set_bitmap(xfs_bitmap, start_block, len);

}


static void
scan_sbtree(
	xfs_agf_t	*agf,
	xfs_agblock_t	root,
	typnm_t		typ,
	int		nlevels,
	void		(*func)(struct xfs_btree_block	*block,
				typnm_t			typ,
				int			level,
				xfs_agf_t		*agf))
{
	xfs_agnumber_t	seqno = be32_to_cpu(agf->agf_seqno);
	struct xfs_buf	*bp;
	int blkbb = 1 << mp->m_blkbb_log;
	void *data;

	//push_cur();
	//set_cur(&typtab[typ], XFS_AGB_TO_DADDR(mp, seqno, root),
	//	blkbb, DB_RING_IGN, NULL);
	bp = libxfs_readbuf(mp->m_ddev_targp, XFS_AGB_TO_DADDR(mp, seqno, root), blkbb, 0, NULL);
        data = bp->b_addr;
	if (data == NULL) {
		log_mesg(0, 0, 0, fs_opt.debug, "can't read btree block %u/%u\n", seqno, root);
		return;
	}
	(*func)(data, typ, nlevels - 1, agf);
	//pop_cur();
}


static void
scanfunc_bno(
	struct xfs_btree_block	*block,
	typnm_t			typ,
	int			level,
	xfs_agf_t		*agf)
{
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (!(be32_to_cpu(block->bb_magic) == XFS_ABTB_MAGIC ||
	      be32_to_cpu(block->bb_magic) == XFS_ABTB_CRC_MAGIC)){
		log_mesg(0, 0, 0, fs_opt.debug, "bb_magic error\n");
		return;
	}

	if (level == 0) {
		rp = XFS_ALLOC_REC_ADDR(mp, block, 1);
		for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
			addtohist(be32_to_cpu(agf->agf_seqno),
					be32_to_cpu(rp[i].ar_startblock),
					be32_to_cpu(rp[i].ar_blockcount));
		return;
	}
	pp = XFS_ALLOC_PTR_ADDR(mp, block, 1, mp->m_alloc_mxr[1]);
	for (i = 0; i < be16_to_cpu(block->bb_numrecs); i++)
		scan_sbtree(agf, be32_to_cpu(pp[i]), typ, level, scanfunc_bno);
}

static void
scan_freelist(
	xfs_agf_t	*agf)
{
	xfs_agnumber_t	seqno = be32_to_cpu(agf->agf_seqno);
	xfs_agfl_t	*agfl;
	xfs_agblock_t	bno;
	int		i;
	__be32		*agfl_bno;
	struct xfs_buf	*bp;
	const struct xfs_buf_ops *ops = NULL;

	if (be32_to_cpu(agf->agf_flcount) == 0)
		return;
	//push_cur();
	//set_cur(&typtab[TYP_AGFL], XFS_AG_DADDR(mp, seqno, XFS_AGFL_DADDR(mp)),
	//			XFS_FSS_TO_BB(mp, 1), DB_RING_IGN, NULL);
	//agfl = iocur_top->data;
	bp = libxfs_readbuf(mp->m_ddev_targp, XFS_AG_DADDR(mp, seqno, XFS_AGFL_DADDR(mp)), XFS_FSS_TO_BB(mp, 1), 0, ops);
	agfl = bp->b_addr;
	i = be32_to_cpu(agf->agf_flfirst);

	/* open coded XFS_BUF_TO_AGFL_BNO */
	agfl_bno = xfs_sb_version_hascrc(&mp->m_sb) ? &agfl->agfl_bno[0]
						   : (__be32 *)agfl;

	/* verify agf values before proceeding */
	if (be32_to_cpu(agf->agf_flfirst) >= XFS_AGFL_SIZE(mp) ||
	    be32_to_cpu(agf->agf_fllast) >= XFS_AGFL_SIZE(mp)) {
		log_mesg(0, 0, 0, fs_opt.debug, "agf %d freelist blocks bad, skipping "
			  "freelist scan\n", i);
		//pop_cur();
		return;
	}

	for (;;) {
		bno = be32_to_cpu(agfl_bno[i]);
		addtohist(seqno, bno, 1);
		if (i == be32_to_cpu(agf->agf_fllast))
			break;
		if (++i == XFS_AGFL_SIZE(mp))
			i = 0;
	}
	//pop_cur();
}



static void
scan_ag(
	xfs_agnumber_t	agno)
{
	xfs_agf_t	*agf;
	struct xfs_buf	*bp;
	const struct xfs_buf_ops *ops = NULL;

	//push_cur();
	//set_cur(&typtab[TYP_AGF], XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp)),
	//			XFS_FSS_TO_BB(mp, 1), DB_RING_IGN, NULL);
	//agf = iocur_top->data;
	bp = libxfs_readbuf(mp->m_ddev_targp, XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR(mp)), XFS_FSS_TO_BB(mp, 1), 0, ops);
	agf = bp->b_addr;
	scan_freelist(agf);
	scan_sbtree(agf, be32_to_cpu(agf->agf_roots[XFS_BTNUM_BNO]),
			TYP_BNOBT, be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNO]),
			scanfunc_bno);
	//pop_cur();
}



static void fs_open(char* device)
{

    int		    open_flags;
    struct stat     statbuf;
    int		    source_is_file = 0;
    //xfs_buf_t       *sbp;
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
    memset(&mbuf, 0, sizeof(xfs_mount_t));
    sb = &mbuf.m_sb;
    get_sb(sb, 0, XFS_MAX_SECTORSIZE, 0);
    //sbp = libxfs_readbuf(xargs.ddev, XFS_SB_DADDR, 1, 0, ops);

    //memset(&mbuf, 0, sizeof(xfs_mount_t));
    //sb = &mbuf.m_sb;
    //libxfs_sb_from_disk(sb, XFS_BUF_TO_SBP(sbp));

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
    xfs_agnumber_t  num_ags;

    int start = 0;
    int bit_size = 1;

    uint64_t bused = 0;
    uint64_t bfree = 0;
    unsigned long long current_block = 0;

    xfs_bitmap = bitmap;

    for(current_block = 0; current_block <= image_hdr.totalblock; current_block++){
	pc_set_bit(current_block, bitmap);
    }
    /// init progress
    progress_init(&prog, start, image_hdr.totalblock, image_hdr.totalblock, BITMAP, bit_size);

    fs_open(device);

    num_ags = mp->m_sb.sb_agcount;

    log_mesg(1, 0, 0, fs_opt.debug, "ags = %i\n", num_ags);
    for (agno = 0; agno < num_ags ; agno++)  {
	/* read in first blocks of the ag */
	scan_ag(agno);
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

