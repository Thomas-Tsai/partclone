/* -*-Asm-*- */
/*
 *  fail-mbr. This file is part of the project DRBL
 *  The code has been strongly inspired by the file
 *  grub-core/boot/i386/pc/boot.S (package grub2) and by the file
 *  fail-mbr.bin which had been written with an hexadecimal editor by
 *  Orgad Shaneh <orgads@gmail.com>.
 *  Copyright (C) 2012 Georges Khaznadar <georgesk@ofset.org>
 *
 *  DRBL is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  DRBL is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with DRBL.  If not, see <http://www.gnu.org/licenses/>.
 */

	.file	"fail-mbr.S"

	.text

	/* Tell GAS to generate 16-bit instructions so that this code works
	   in real mode. */
	.code16 

	.globl _start

_start:
	/*
	 * _start is loaded at 0x7c00 and is jumped to with CS:IP 0:0x7c00
	 */
	.set real_start, 0x7c00
	/*
	 * Beginning of the sector is compatible with the FAT/HPFS BIOS
	 * parameter block.
	 */

	jmp	after_signature
signature:
	.ascii "OCS Dummy MBR"

	nop
after_signature:
/* general setup */
	xor	%ax, %ax	/* zero in ax */
	push	%ax
	pop	%ds		/* zero in ds */
	mov	$(failure_msg + real_start - _start), %si	/* point to the message */
start_msg1:	
	lods	%ds:(%si),%al	/* get one char and increment the pointer */
	cmp	$0x0, %al	/* is the cstring finished? */
	je	end_msg1
	mov	$0x7, %bx	/* page 0, color 7 (only in graphics mode) */
	mov	$0x1, %cx	/* probably not relevant is it a bug? */
	mov	$0xe, %ah	/* prepare a call to Teletype output */
	int	$0x10		/* BIOS call */
	jmp	start_msg1
end_msg1:
	mov	$0x2, %ah	/* prepare a call to Set cursor position */
	mov	$0x100, %dx	/* row 0, column 1 */
	int	$0x10		/* BIOS call */
wait_forever:	
	cmp	$0x0,%al
	je	wait_forever
	
failure_msg:
	.ascii "Image loading failure. Reload image!\0"
	
/* filling with zeros to match the length of another mbr file */
	. = _start + 0x1bd
	.byte 0x0
	