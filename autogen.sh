# autogen.sh - creates configure scripts and more
# requires autoconf

aclocal
autoheader
automake -a
autoconf
