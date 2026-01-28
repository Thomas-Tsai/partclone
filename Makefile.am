AUTOMAKE_OPTIONS = subdir-objects 
SUBDIRS= po src docs tests

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4/ChangeLog  config.rpath toolbox src/deplib_version.c src/version.h src/ufs debian

ChangeLog: FORCE
	srcdir=. $(SHELL) ./toolbox --update-log

MAINTAINERCLEANFILES = \
	Makefile.in \
	aclocal.m4 \
	configure \
	config.h.in \
	config.log \
	config.status \
	config.cache \
	compile \
	missing \
	install-sh \
	ltmain.sh \
	depcomp \
	config.guess \
	config.sub \
	config.rpath \
	ar-lib \
	config.h.in~ \
	configure~ \
	po/Makefile.in.in \
	m4/libtool.m4 \
	m4/ltoptions.m4 \
	m4/ltsugar.m4 \
	m4/ltversion.m4 \
	m4/lt~obsolete.m4

distclean-local:
	-rm -rf autom4te.cache/
	-rm -f po/*.gmo
	-rm -f po/stamp-po

maintainer-clean-local:
	-rm -rf m4/

FORCE:
