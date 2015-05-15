failmbrdir=$(datadir)/@PACKAGE@/
failmbr_DATA = fail-mbr.bin

clean-local:
	rm -f *~ *.o *.image *.bin

distclean-local: clean-local

fail-mbr.bin:
	sh compile-mbr.sh
