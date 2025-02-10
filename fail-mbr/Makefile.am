if ENABLE_FAILMBR
failmbrdir=$(datadir)/@PACKAGE@/
failmbr_DATA = fail-mbr.bin

clean-local:
	rm -f *~ *.o *.image *.bin *.obj

distclean-local: clean-local

if X86
fail-mbr.image: fail-mbr.S
	$(CC) -c "$<" -o "$@"

fail-mbr.bin: fail-mbr.image
	$(OBJCOPY) -O binary --strip-unneeded -R .note -R .comment -R .note.gnu.build-id -R .note.gnu.property -R .reginfo -R .rel.dyn "$<" "$@"

	## Check that this file is the same as the original fail-mbr:
	@echo "Checking the file:"
	$(OBJDUMP) -D -b binary -mi386 -Maddr16,data16 "$@" > f1.obj
	$(OBJDUMP) -D -b binary -mi386 -Maddr16,data16 "$(srcdir)/$@.orig" > f2.obj

	## The regular expression is used to reject lines which contain either:
	## - the name of the file, "fail-mbr"
	## - a line separator output by diff, "---"
	## - the OP code "xor    %ax,%ax" which may begin by 0x31 or 0x33
	## - a numerical offset output by diff, ^[a-f0-9]*$
	@if diff f1.obj f2.obj | grep -qEv 'fail-mbr|---|xor    %ax,%ax|^[a-f0-9]*$$'; then \
		echo "files $@ and $@.orig differ significantly:"; \
		diff f1.obj f2.obj; \
		exit 1; \
	else \
		echo "'$@.bin' and '$@.orig' have no significant differences."; \
		rm f1.obj f2.obj; \
		exit 0; \
	fi
else
fail-mbr.bin: fail-mbr.bin.orig
	@echo "The architecture is not x86, so the file '$@' is not compiled"
	cp -vp "$<" "$@"
endif

endif
