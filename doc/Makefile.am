man_MANS = ncdu.1
EXTRA_DIST = ncdu.1 ncdu.pod

# Don't "clean" ncdu.1, it should be in the tarball so that pod2man isn't a
# build dependency for those who use the tarball.
ncdu.1: ncdu.pod Makefile
	pod2man --center "ncdu manual" --release "@PACKAGE@-@VERSION@" ncdu.pod >ncdu.1
