bin_PROGRAMS=ncdu

ncdu_SOURCES=\
	src/browser.c\
	src/delete.c\
	src/dirlist.c\
	src/dir_common.c\
	src/dir_export.c\
	src/dir_mem.c\
	src/dir_scan.c\
	src/exclude.c\
	src/help.c\
	src/main.c\
	src/path.c\
	src/util.c

noinst_HEADERS=\
	src/browser.h\
	src/delete.h\
	src/dir.h\
	src/dirlist.h\
	src/exclude.h\
	src/global.h\
	src/help.h\
	src/khash.h\
	src/path.h\
	src/util.h


man_MANS=ncdu.1
EXTRA_DIST=ncdu.1 doc/ncdu.pod

# Don't "clean" ncdu.1, it should be in the tarball so that pod2man isn't a
# build dependency for those who use the tarball.
ncdu.1: $(srcdir)/doc/ncdu.pod Makefile
	pod2man --center "ncdu manual" --release "@PACKAGE@-@VERSION@" "$(srcdir)/doc/ncdu.pod" >ncdu.1

