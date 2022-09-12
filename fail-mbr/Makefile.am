failmbrdir=$(datadir)/@PACKAGE@/
failmbr_DATA = fail-mbr.bin

clean-local:
	rm -f *~ *.o *.image *.bin *.obj

distclean-local: clean-local

fail-mbr.bin:
	sh $(srcdir)/compile-mbr.sh
