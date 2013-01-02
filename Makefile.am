SUBDIRS= po src docs tests fail-mbr

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog  config.rpath toolbox src/deplib_version.c src/version.h src/ufs debian

ChangeLog: FORCE
	srcdir=. $(SHELL) ./toolbox --update-log

FORCE:
