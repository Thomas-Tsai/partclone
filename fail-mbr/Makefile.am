failmbrdir=$(datadir)/@PACKAGE@/
nodist_pkgdata_DATA = fail-mbr.bin
CLEANFILES = $(nodist_pkglib_DATA)

AM_CCASFLAGS = -fno-builtin -nostdinc
failmbr_exec_LDFLAGS = -nostdlib -Wl,-N,-Ttext,7C00

noinst_PROGRAMS = failmbr.exec
failmbr_exec_SOURCES = fail-mbr.S

fail-mbr.bin: failmbr.exec
	$(OBJCOPY) -O binary -R .note -R .comment -R .note.gnu.build-id -R .reginfo -R .rel.dyn  $< $@

