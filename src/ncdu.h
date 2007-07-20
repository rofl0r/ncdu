/* ncdu - NCurses Disk Usage 
    
  Copyright (c) 2007 Yoran Heling

  Permission is hereby granted, free of charge, to any person obtaining
  a copy of this software and associated documentation files (the
  "Software"), to deal in the Software without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of the Software, and to
  permit persons to whom the Software is furnished to do so, subject to
  the following conditions:
  
  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <curses.h>
#include <form.h>
#include <menu.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_TIME_H) && defined(HAVE_GETTIMEOFDAY)
# include <sys/time.h>
#else
# include <time.h>
#endif
#include <dirent.h>

/* PATH_MAX 260 on Cygwin is too small for /proc/registry */
#ifdef __CYGWIN__
# if PATH_MAX < 1024
#  undef PATH_MAX
#  define PATH_MAX 1024
# endif
#endif

/* get PATH_MAX */
#ifndef PATH_MAX
# ifdef _POSIX_PATH_MAX
#  define PATH_MAX _POSIX_PATH_MAX
# else
#  define PATH_MAX 4096
# endif
#endif
/* and LINK_MAX */
#ifndef LINK_MAX
# ifdef _POSIX_LINK_MAX
#  define LINK_MAX _POSIX_LINK_MAX
# else
#  define LINK_MAX 32
# endif
#endif
/* check for S_ISLNK */
#ifndef S_ISLNK
# ifndef S_IFLNK
#  define S_IFLNK 0120000
# endif
# define S_ISLNK(x) (x & S_IFLNK)
#endif

/*
 *    G L O B A L   F L A G S
 */
/* File Flags (struct dir -> flags) */
#define FF_DIR    1
#define FF_FILE   2
#define FF_OTHER  4
#define FF_ERR    8
#define FF_OTHFS 16
#define FF_SERR  32 /* error in subdirectory */
#define FF_BSEL  64 /* selected */

/* Settings Flags (int sflags) */
#define SF_SMFS   1 /* same filesystem */
#define SF_AS     2 /* apparent sizes */
#define SF_SI     4 /* use powers of 1000 instead of 1024 */
#define SF_IGNS   8 /* ignore too small terminal sizes */
#define SF_NOCFM 16 /* don't confirm file deletion */
#define SF_IGNE  32 /* ignore errors when deleting */

/* Browse Flags (int bflags) */
#define BF_NAME   1
#define BF_SIZE   2
#define BF_FILES  4
#define BF_NDIRF 32 /* Normally, dirs before files, setting this disables it */
#define BF_DESC  64
#define BF_HIDE 128 /* don't show hidden files... doesn't have anything to do with sorting though */


/*
 *    S T R U C T U R E S
 */
struct dir {
  struct dir *parent, *next, *prev, *sub;
  char *name;
  off_t size;
  unsigned int files, dirs;
  unsigned char flags;
};


/*
 *    G L O B A L   V A R I A B L E S
 *
 * (all defined in main.c)
 */
/* main directory data */
extern struct dir dat;
/* updated when window is resized */
extern int winrows, wincols;
/* global settings */
extern char sdir[PATH_MAX];
extern int sflags, bflags, sdelay, bgraph;


/*
 *    G L O B A L   F U N C T I O N S
 */
/* util.c */
extern char *cropdir(const char *, int);
extern char *cropsize(const off_t);
extern void ncresize(void);
extern struct dir * freedir(struct dir *);
extern char *getpath(struct dir *, char *);
/* settings.c */
extern int settingsCli(int, char **);
extern int settingsWin(void);
/* calc.c */
extern int calcUsage();
/* browser.c */
extern void showBrowser(void);

