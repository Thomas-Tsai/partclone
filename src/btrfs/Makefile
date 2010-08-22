CC=gcc
AM_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2
CFLAGS = -g -Werror -Os
objects = ctree.o disk-io.o radix-tree.o extent-tree.o print-tree.o \
	  root-tree.o dir-item.o file-item.o inode-item.o \
	  inode-map.o crc32c.o rbtree.o extent-cache.o extent_io.o \
	  volumes.o utils.o

#
CHECKFLAGS=-D__linux__ -Dlinux -D__STDC__ -Dunix -D__unix__ -Wbitwise \
		-Wuninitialized -Wshadow -Wundef
DEPFLAGS = -Wp,-MMD,$(@D)/.$(@F).d,-MT,$@

INSTALL= install
prefix ?= /usr/local
bindir = $(prefix)/bin
LIBS=-luuid

# make C=1 to enable sparse
ifdef C
	check=sparse $(CHECKFLAGS)
else
	check=ls
endif

.c.o:
	$(check) $<
	$(CC) $(DEPFLAGS) $(AM_CFLAGS) $(CFLAGS) -c $<
progs = dump_block

all: version $(progs)

version:
	bash version.sh

dump_block: $(objects) dump-block.o
	gcc $(CFLAGS) -o dump_block $(objects) dump-block.o $(LDFLAGS) $(LIBS)

clean :
	rm -f $(progs) cscope.out *.o .*.d

-include .*.d
