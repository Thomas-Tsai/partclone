SUBDIRS= po src docs

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog  config.rpath toolbox

ChangeLog: FORCE
	srcdir=. $(SHELL) ./toolbox --update-log

FORCE:
