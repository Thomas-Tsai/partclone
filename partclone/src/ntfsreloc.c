/*
ntfsreloc - deals with braindeadness with moving NTFS filesystems.
version 0.7

Copyright (C) 2006  Daniel J. Grace
Modified 2008 by Orgad Shaneh

This program modifies the geometry settings on an NTFS partition
as described in <http://thestarman.pcministry.com/asm/mbr/NTLDR.htm>.
It is needed for booting, as described in libntfs <http://www.linux-ntfs.org/doku.php?id=libntfs>

It will NOT work for Windows NT v3.5 and below.
It is designed for NT4/2000/XP and later versions.

Like any other program that tinkers with the contents of your HD, this
program may cause data corruption. USE AT YOUR OWN RISK. You have been warned.


This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <linux/hdreg.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

int flip(void *p, int size) {
	ushort test;
	int iter;
	char *c;
	char t;

	if (size % 2) return 1;

	// Determine system architecture
	test = 1;
	c = (char *)&test;
	if (*c) return 0;

	// reverse bytes
	c = p;
	for (iter = 0 ; iter < size / 2; ++iter) {
		t = c[iter];
		c[iter] = c[size-iter-1];
		c[size-iter-1] = t;
	}
	return 0;
}

int usage(char *progname) {
	fprintf(stderr, 
		"adjust filesystem geometry for a NTFS partition"
		"\nUsage: %s [-h # -t #] [-s start [-b]] [-w] [-f] [-p] device"
		"\nwhere device points to an NTFS partition"
		"\n"
		"\nOptions:"
		"\n-w:\t\tWrite new start sector to the partition."
		"\n-h # -t #:\tSpecify number of heads and number of sectors per track"
		"\n\t\tIf omitted, determined via ioctl."
		"\n-s start:\tNew start sector to write."
		"\n\t\tIf omitted, determined via ioctl."
		"\n-b:\t\tProceed even if the specified device is not a"
		"\n\t\tpartition (e.g. a regular file)"
		"\n-f:\t\tForce the operation to occur even if device does not look"
		"\n\t\tlike a valid NTFS partition or values are equal."
		"\n-p:\t\tPrint debug information (values read, values requested etc.)"
		"\n"
		"\nThis utility displays the current starting sector as defined by the"
		"\nthe filesystem.  No change will actually be made without the -w"
		"\noption."
		"\n"
		"\nExit status is 2 if an error occured, 1 if a change was made or is needed"
		"\nor 0 if the filesystem already has the correct values."
		"\n", progname
	);
	return 0;
}

off64_t last_sector(int device)
{
	unsigned short sectorSize = 0;
	unsigned long long totalSectors = 0;

	if (lseek(device, 0x0bL, SEEK_SET) < 0) {
		perror("lseek");
		exit(2);
	}

	if (read(device, &sectorSize, 2) != 2) {
		fprintf(stderr, "Unable to read filesystem info.\n");
		exit(2);
	}

	if (lseek(device, 0x28L, SEEK_SET) < 0) {
		perror("lseek");
		exit(2);
	}

	if (read(device, &totalSectors, 8) != 8) {
		fprintf(stderr, "Unable to read filesystem info.\n");
		exit(2);
	}
	flip(&sectorSize, 2);
	flip(&totalSectors, 8);
	return sectorSize * totalSectors;
}

struct ntfs_geometry {
	unsigned short sectors;
	unsigned short heads;
	unsigned long start;
};

void flip_all(struct ntfs_geometry *geo) {
	flip(&geo->heads, 2);
	flip(&geo->sectors, 2);
	flip(&geo->start, 4);
}

char optSpecifyStart = 0, optSpecifyHS = 0, optWrite = 0, optBlock = 0, optForce = 0, optPrint = 0;
char *optDeviceName = NULL;
struct ntfs_geometry opt_geom = {0, 0, 0};

void print(const char *s, ...)
{
	va_list ap;
	if (optPrint) {
		va_start(ap, s);
		vprintf(s, ap);
		va_end(ap);
	}
}

int read_options(int argc, char *argv[]) {
	int i;
	char opt;
	int readopts = 1;

	if (argc <= 1) {
		usage(argv[0]);
		return 2;
	}

	// read options
	for (i = 1 ; i < argc ; ++i) {
		if (argv[i][0] == '-' && readopts) { 
			opt = argv[i][1];
			// -h, -t and -s need a number to follow
			if ((opt == 'h') || (opt == 't') || (opt == 's')) {
				char *sizePtr, *endPtr;
				if (argv[i][2]) {
					sizePtr = &argv[i][2];
				} else if (i+1 < argc) {
					sizePtr = argv[++i];
				} else {
					fprintf(stderr, "ERROR: Size must be specified for option -%c\n", opt);
					usage(argv[0]);
					return 1;
				}

				switch (opt) {
				case 'h': optSpecifyHS = 1;    opt_geom.heads = strtoul(sizePtr, &endPtr, 10); break;
				case 't': optSpecifyHS = 1;    opt_geom.sectors = strtoul(sizePtr, &endPtr, 10); break;
				case 's': optSpecifyStart = 1; opt_geom.start = strtoul(sizePtr, &endPtr, 10); break;
				}

				// assert the value is a number
				if (endPtr == sizePtr || *endPtr) {
					fprintf(stderr, "ERROR: Invalid size specified for option -%c\n", opt);
					usage(argv[0]);
					return 1;
				}
				continue;
			}

			if (opt && argv[i][2]) {
				fprintf(stderr, "Unknown option '%s'\n", argv[i]);
				usage(argv[0]);
				return 1;
			}

			switch (opt) {
			case '-': readopts = 0; break;
			case 'b': optBlock = 1; break;
			case 'w': optWrite = 1; break;
			case 'f': optForce = 1; break;
			case 'p': optPrint = 1; break;
			default:
				fprintf(stderr, "Unknown option '%s'\n", argv[i]);
				usage(argv[0]);
				return 1;
			}
			continue;
		}

		// If we reach here, we're reading a device name
		if (optDeviceName) {
			fprintf(stderr, "Only one device may be specified\n");
			usage(argv[0]);
			return 1;
		}
		optDeviceName = argv[i];
	}

	if (!optDeviceName) {
		fprintf(stderr, "No device name specified\n");
		usage(argv[0]);
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[]) {
	int device = 0;
	int opt_res;
	char haveGeom = 0;
	off64_t lastsector = 0;
	struct hd_geometry part_geom = {0, 0, 0, 0};
	struct ntfs_geometry fs_geom = {0, 0, 0};
	struct ntfs_geometry bak_geom = {0, 0, 0};
	struct ntfs_geometry set_geom = {0, 0, 0};
	const int geomsize = sizeof(struct ntfs_geometry);

	puts("ntfsreloc version 0.7");

	// read program options (into global variables)
	opt_res = read_options(argc, argv);
	if (opt_res) return opt_res;

	// verify that we can open the device in readonly mode
	if (!(device = open(optDeviceName, (optWrite ? O_RDWR : O_RDONLY) | O_SYNC))) {
		perror("open");
		return 2;
	}

	// check to see if it's a partition, and determine geometry
	if (ioctl(device, HDIO_GETGEO, &part_geom)) {
		if (!optBlock) {
			fprintf(stderr, "Failed to read disk geometry.  Perhaps this is not a partition?\n");
			fprintf(stderr, "Verify that you are using the correct device or use the -b option.\n");
			fprintf(stderr, "The exact error was:\n");
			perror("ioctl");
			return 2;
		} else if (!optSpecifyStart && optWrite) {
			fprintf(stderr, "Failed to read disk geometry, and -s option was not specified.\n");
			fprintf(stderr, "No update can be made without this information.\n");
			fprintf(stderr, "The exact error was:\n");
			perror("ioctl");
			return 2;
		}			
	} else {
		haveGeom = 1;

		if (!optForce && !part_geom.start) {
			fprintf(stderr, "This looks like an entire disk (start=0) instead of a single partition.\n");
			fprintf(stderr, "It will not be modified without the -f (force) option.\n");
			if (optWrite) {
				return 2;
			}
		}
	}

	// verify that it is an NTFS partition
	if (lseek(device, 3L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}

	// read "NTFS" magic, or at least what should be
	char ntfsMagic[4];
	if (read(device, &ntfsMagic, 4) != 4 || memcmp(ntfsMagic, "NTFS", 4)) {
		if (!optForce) {
			fprintf(stderr, "This device does not appear to be a real NTFS volume.\n");
			if (!optWrite) {
				return 2;
			}
		}
	}

	// filesystem geometry
	if (lseek(device, 0x18L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}
	if (read(device, &fs_geom, geomsize) != geomsize) {
		fprintf(stderr, "Unable to read filesystem info.\n");
		return 2;
	}

	flip_all(&fs_geom);

	// backup sector
	lastsector = last_sector(device);
	if (lastsector == 0) {
		fprintf(stderr, "Unable to determine last (backup) sector!\n");
		return 2;
	}
	if (lseek64(device, lastsector + 0x18L, SEEK_SET) < 0) {
		perror("lseek64");
		return 2;
	}

	if (read(device, &bak_geom, geomsize) != geomsize) {
		fprintf(stderr, "Unable to read filesystem info.\n");
		return 2;
	}
	flip_all(&bak_geom);

	if (optSpecifyHS) {
		if (!opt_geom.heads || !opt_geom.sectors) {
			fprintf(stderr, "Must specify both heads and sectors per track or none!\n");
			return 2;
		}
	}

	// behavior description follows. HS and Start values indicates SpecifyHS and SpecifyStart
	// part: use partition value
	// fs  : use filesystem value (don't change anything)
	// opt : use user value

	// HS   Start  tgt HS  tgt Start
	// 0      0     part     part  (if no partition geometry, use fs)
	// 0      1      fs       opt
	// 1      0      opt      fs
	// 1      1      opt      opt
	if (!optSpecifyHS && !optSpecifyStart) {
		if (haveGeom) {
			set_geom.heads = part_geom.heads;
			set_geom.sectors = part_geom.sectors;
			set_geom.start = part_geom.start;
		} else {
			set_geom = fs_geom;
		}
	} else {
		set_geom = opt_geom;
		if (!optSpecifyStart) {
			set_geom.start = fs_geom.start;
		}
		if (!optSpecifyHS) {
			set_geom.heads = fs_geom.heads;
			set_geom.sectors = fs_geom.sectors;
		}
	}

	// print all details
	print("\t\tHeads\tSectors\tStart\n");
	if (haveGeom) {
		print("partition:\t%d\t%d\t%lu\n", part_geom.heads, part_geom.sectors, part_geom.start);
	}
	print("filesystem:\t%d\t%d\t%lu\n", fs_geom.heads, fs_geom.sectors, fs_geom.start);
	print("backup sector\t%d\t%d\t%lu\n", bak_geom.heads, bak_geom.sectors, bak_geom.start);
	print("target:\t\t%d\t%d\t%lu\n", set_geom.heads, set_geom.sectors, set_geom.start);

	if (!memcmp(&set_geom, &fs_geom, geomsize)
	&& !memcmp(&set_geom, &bak_geom, geomsize)
	&& !optForce) {
		puts("No changes neccessary.");
		return 0;
	}

	if (!optWrite) {
		puts("Changes will be written to disk only with -w flag");
		return 0;
	}

	flip_all(&set_geom);

	// write back changes
	if (lseek(device, 0x18L, SEEK_SET) < 0) {
		perror("lseek");
		return 2;
	}
	if (write(device, &set_geom, geomsize) != geomsize) {
		perror("write");
		return 2;
	}

	// write to backup sector
	if (lseek64(device, lastsector + 0x18L, SEEK_SET) < 0) {
		perror("lseek64");
		return 2;
	}
	if (write(device, &set_geom, geomsize) != geomsize) {
		perror("write");
		return 2;
	}

	if (fsync(device)) {
		perror("fsync");
		return 2;
	}
	if (close(device)) {
		perror("close");
		return 2;
	}
	puts("done!");

	return 1;
}

