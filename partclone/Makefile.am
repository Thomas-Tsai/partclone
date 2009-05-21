SUBDIRS= po src docs

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog  config.rpath toolbox src/deplib_version.c

ChangeLog: FORCE
	srcdir=. $(SHELL) ./toolbox --update-log

FORCE:
