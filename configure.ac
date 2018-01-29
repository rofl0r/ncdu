
AC_INIT(ncdu, 1.13, projects@yorhel.nl)
AC_CONFIG_SRCDIR([src/global.h])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# Check for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_RANLIB
PKG_PROG_PKG_CONFIG

# Check for header files.
AC_CHECK_HEADERS(
  [limits.h sys/time.h sys/types.h sys/stat.h dirent.h unistd.h fnmatch.h ncurses.h],[],
  AC_MSG_ERROR([required header file not found]))

AC_CHECK_HEADERS(locale.h)

# Check for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_SYS_LARGEFILE
AC_STRUCT_ST_BLOCKS

# Check for library functions.
AC_CHECK_FUNCS(
  [getcwd gettimeofday fnmatch chdir rmdir unlink lstat system getenv],[],
  AC_MSG_ERROR([required function missing]))



# Look for ncurses library to link to
ncurses=auto
AC_ARG_WITH([ncurses],
            AC_HELP_STRING([--with-ncurses], [compile/link with ncurses library] ),
            [ncurses=ncurses])
AC_ARG_WITH([ncursesw],
            AC_HELP_STRING([--with-ncursesw], [compile/link with wide-char ncurses library @<:@default@:>@]),
            [ncurses=ncursesw])
if test "$ncurses" = "auto" -o "$ncurses" = "ncursesw"; then
  PKG_CHECK_MODULES([NCURSES], [ncursesw], [LIBS="$LIBS $NCURSES_LIBS"; ncurses=ncursesw],
    [AC_CHECK_LIB([ncursesw],
                 [initscr],
                 [LIBS="$LIBS -lncursesw"; ncurses=ncursesw],
                 [ncurses=ncurses])
    ])
fi
if test "$ncurses" = "ncurses"; then
  PKG_CHECK_MODULES([NCURSES], [ncurses], [LIBS="$LIBS $NCURSES_LIBS"],
    [AC_CHECK_LIB([ncurses],
                 [initscr],
                 [LIBS="$LIBS -lncurses"],
                 [AC_MSG_ERROR(ncurses library is required)])
    ])
fi

# Configure default shell for spawning shell when $SHELL is not set
AC_ARG_WITH([shell],
  [AS_HELP_STRING([--with-shell],
                  [used interpreter as default shell (default is /bin/sh)])],
                  [DEFAULT_SHELL=$withval],
                  [DEFAULT_SHELL=/bin/sh])
AC_MSG_NOTICE([Using $DEFAULT_SHELL as the default shell if \$SHELL is not set])
AC_DEFINE_UNQUOTED(DEFAULT_SHELL, "$DEFAULT_SHELL", [Used default shell interpreter])


AC_OUTPUT([Makefile])
