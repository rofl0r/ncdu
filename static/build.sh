#!/bin/sh

# This script is based on static/build.sh from the ncdc git repo.
# Only i486 and arm arches are supported. i486 should perform well enough, so
# x86_64 isn't really necessary. I can't test any other arches.


MUSL_VERSION=1.1.6
NCURSES_VERSION=5.9

export CFLAGS="-O3 -g -static"

# (The variables below are automatically set by the functions, they're defined
# here to make sure they have global scope and for documentation purposes.)

# This is the arch we're compiling for, e.g. arm/mipsel.
TARGET=
# This is the name of the toolchain we're using, and thus the value we should
# pass to autoconf's --host argument.
HOST=
# Installation prefix.
PREFIX=
# Path of the extracted source code of the package we're currently building.
srcdir=

mkdir -p tarballs


# "Fetch, Extract, Move"
fem() { # base-url name targerdir extractdir 
  echo "====== Fetching and extracting $1 $2"
  cd tarballs
  if [ -n "$4" ]; then
    EDIR="$4"
  else
    EDIR=$(basename $(basename $(basename $2 .tar.bz2) .tar.gz) .tar.xz)
  fi
  if [ ! -e "$2" ]; then
    wget "$1$2" || exit
  fi
  if [ ! -d "$3" ]; then
    tar -xvf "$2" || exit
    mv "$EDIR" "$3"
  fi
  cd ..
}


prebuild() { # dirname
  if [ -e "$TARGET/$1/_built" ]; then
    echo "====== Skipping build for $TARGET/$1 (assumed to be done)"
    return 1
  fi
  echo "====== Starting build for $TARGET/$1"
  rm -rf "$TARGET/$1"
  mkdir -p "$TARGET/$1"
  cd "$TARGET/$1"
  srcdir="../../tarballs/$1"
  return 0
}


postbuild() {
  touch _built
  cd ../..
}


# Pre-built cross-compilation binaries for musl. Handy. :)
getmusl() {
  # Order of $HOST is different than the tar/dir names, so we need this case.
  case $TARGET in
    arm)    DIR=arm-linux-musleabi ;;
    i486)   DIR=i486-linux-musl ;;
  esac
  fem https://googledrive.com/host/0BwnS5DMB0YQ6bDhPZkpOYVFhbk0/musl-$MUSL_VERSION/ crossx86-$DIR-$MUSL_VERSION.tar.xz "musl-$TARGET" $DIR
}


getncurses() {
  fem http://ftp.gnu.org/pub/gnu/ncurses/ ncurses-$NCURSES_VERSION.tar.gz ncurses
  prebuild ncurses || return
  $srcdir/configure --prefix=$PREFIX\
    --without-cxx --without-cxx-binding --without-ada --without-manpages --without-progs\
    --without-tests --without-curses-h --without-pkg-config --without-shared --without-debug\
    --without-gpm --without-sysmouse --enable-widec --with-default-terminfo-dir=/usr/share/terminfo\
    --with-terminfo-dirs=/usr/share/terminfo:/lib/terminfo:/usr/local/share/terminfo\
    --with-fallbacks="screen linux vt100 xterm" --host=$HOST\
    CPPFLAGS=-D_GNU_SOURCE || exit
  make || exit
  make install.libs || exit
  postbuild
}


getncdu() {
  prebuild ncdu || return
  srcdir=../../..
  $srcdir/configure --host=$HOST --with-ncursesw PKG_CONFIG=false\
    CPPFLAGS=" -D_GNU_SOURCE -I$PREFIX/include -I$PREFIX/include/ncursesw"\
    LDFLAGS="-static -L$PREFIX/lib -lncursesw" || exit
  make || exit

  VER=`cd '../../..' && git describe --abbrev=5 --dirty= | sed s/^v//`
  tar -czf ../../ncdu-linux-$TARGET-$VER-unstripped.tar.gz ncdu
  $HOST-strip ncdu
  tar -czf ../../ncdu-linux-$TARGET-$VER.tar.gz ncdu
  echo "====== ncdu-linux-$TARGET-$VER.tar.gz and -unstripped created."

  postbuild
}


buildarch() {
  TARGET=$1
  case $TARGET in
    arm)    HOST=arm-musl-linuxeabi ;;
    i486)   HOST=i486-musl-linux ;;
    *)      echo "Unknown target: $TARGET" ;;
  esac
  PREFIX="`pwd`/$TARGET/inst"
  mkdir -p $TARGET $PREFIX

  getmusl
  MBIN="`pwd`/tarballs/musl-$TARGET/bin"
  OLDPATH="$PATH"
  PATH="$PATH:$MBIN"
  getncurses
  getncdu
  PATH="$OLDPATH"
}


buildarch $1

