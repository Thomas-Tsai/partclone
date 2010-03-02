/*
ntfsfixboot - deals with braindeadness with moving NTFS filesystems.
version 0.9

Copyright (C) 2009  Orgad Shaneh
Loosely based on the work of Daniel J. Grace (2006)

This program modifies the geometry settings on an NTFS partition
as described in <http://thestarman.pcministry.com/asm/mbr/NTFSBR.htm>.
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
#include <stdint.h>

int flip(void *p, int size) {
  uint16_t test;
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
    "\nUsage: %s [-h # -t #] [-s start] [-b] [-w] [-f] [-p] device"
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
    "\nExit status:"
	"\n* 0 - success (values are correct, or changed successfully)"
	"\n* 1 - a change is needed, but -w was not specified"
	"\n* 2 - an error occured"
    "\n", progname
    );
  return 0;
}

struct ntfs_geometry {
  uint16_t sectors;
  uint16_t heads;
  uint32_t start;
} __attribute__((packed));

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
  uint16_t sector_size = 0;
  uint64_t total_sectors = 0;
  off64_t last_sector = 0;
  struct hd_geometry part_geom = {0, 0, 0, 0};
  struct ntfs_geometry fs_geom = {0, 0, 0};
  struct ntfs_geometry bak_geom = {0, 0, 0};
  struct ntfs_geometry set_geom = {0, 0, 0};
  char br_sector[512];
  char bak_sector[512];
  const int geomsize = sizeof(struct ntfs_geometry);

  puts("ntfsfixboot version 0.9");

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

  if (read(device, br_sector, 512) != 512) {
    fprintf(stderr, "Error reading device!");
    perror("read");
  }

  // verify that it is an NTFS partition
  // read "NTFS" magic, or at least what should be
  if (memcmp(br_sector + 0x03, "NTFS", 4)) {
    if (!optForce) {
      fprintf(stderr, "This device does not appear to be a real NTFS volume.\n");
      if (!optWrite) {
        return 2;
      }
    }
  }

  // filesystem geometry
  memcpy(&fs_geom, br_sector + 0x18, geomsize);

  flip_all(&fs_geom);

  // backup sector
  memcpy(&sector_size, br_sector + 0x0b, 2);
  memcpy(&total_sectors, br_sector + 0x28, 8);

  flip(&sector_size, 2);
  flip(&total_sectors, 8);
  last_sector = total_sectors * sector_size;

  // very unlikely to happen...
  if (sector_size != 512) {
    fprintf(stderr, "Sector size is not 512. this mode is not supported!");
    return 2;
  }

  if (last_sector == 0) {
    fprintf(stderr, "Unable to determine last (backup) sector!\n");
    return 2;
  }

  if (lseek64(device, last_sector, SEEK_SET) < 0) {
    perror("lseek64");
    return 2;
  }

  if (read(device, &bak_sector, 512) != 512) {
    fprintf(stderr, "Unable to read backup sector.\n");
    return 2;
  }
  memcpy(&bak_geom, bak_sector + 0x18, geomsize);
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
    && !optForce
    && !memcmp(br_sector, bak_sector, 512)) {
      puts("No changes neccessary.");
      return 0;
  }

  if (!optWrite) {
    puts("Changes will be written to disk only with -w flag");
    return 1;
  }

  flip_all(&set_geom);

  memcpy(br_sector + 0x18, &set_geom, geomsize);
  // write back changes
  if (lseek(device, 0L, SEEK_SET) < 0) {
    perror("lseek");
    return 2;
  }
  if (write(device, br_sector, 512) != 512) {
    perror("write");
    return 2;
  }

  // write to backup sector
  if (lseek64(device, last_sector, SEEK_SET) < 0) {
    perror("lseek64");
    return 2;
  }
  if (write(device, br_sector, 512) != 512) {
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

  return 0;
}
