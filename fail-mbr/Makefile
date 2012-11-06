PREFIX=         /usr/local
INSTPREFIX=     $(BUILD_ROOT)$(PREFIX)
INSTLIBDIR=     $(INSTPREFIX)/lib/partclone
INSTALL=        install


all: fail-mbr.bin

clean:
	rm -f *~ *.o *.image *.bin *.hex*

compare: fail-mbr.bin
	hd fail-mbr.bin > fail-mbr.hex
	hd fail-mbr.bin.orig > fail-mbr.hex.orig
	diff fail-mbr.hex.orig fail-mbr.hex || true

%.o: %.S
	$(CC) -Wall -Werror -m32 -nostdlib -o $@ $<

fail-mbr.image: fail-mbr.o
	$(CC) -Os -Wall -W -Wshadow -Wpointer-arith -Wmissing-prototypes -Wundef -Wstrict-prototypes -g -falign-jumps=1 -falign-loops=1 -falign-functions=1 -mno-mmx -mno-sse -mno-sse2 -mno-3dnow -fno-dwarf2-cfi-asm -fno-asynchronous-unwind-tables -m32 -fno-stack-protector -mno-stack-arg-probe -Werror -Wno-trampolines -DUSE_ASCII_FAILBACK=1 -DHAVE_UNIFONT_WIDTHSPEC=1  -mrtd -mregparm=3       -fno-builtin   -m32 -Wl,--build-id=none   -nostdlib -Wl,-N,-S -Wl,-N -Wl,-Ttext,0x7C00   -o $@ $<

fail-mbr.bin: fail-mbr.image
	objcopy  -O binary  --strip-unneeded -R .note -R .comment -R .note.gnu.build-id -R .reginfo -R .rel.dyn $< $@

install: fail-mbr.bin
	-mkdir -p $(INSTLIBDIR)
	$(INSTALL) fail-mbr.bin $(INSTLIBDIR)
	

check: compare

.PHONY: all compare install
