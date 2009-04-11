/* ncdu - NCurses Disk Usage 
    
  Copyright (c) 2007-2009 Yoran Heling

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

#ifndef _ncdu_h
#define _ncdu_h

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>

#include <ncurses.h>
#include <form.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>

#include "calc.h"

/* set S_BLKSIZE if not defined already in sys/stat.h */
#ifndef S_BLKSIZE
# define S_BLKSIZE 512
#endif

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
#define FF_DIR    0x01
#define FF_FILE   0x02
#define FF_ERR    0x04 /* error while reading this item */
#define FF_OTHFS  0x08 /* excluded because it was an other filesystem */
#define FF_EXL    0x10 /* excluded using exlude patterns */
#define FF_SERR   0x20 /* error in subdirectory */
#define FF_BSEL   0x40 /* selected */

/* Settings Flags (int sflags) */
#define SF_SMFS   0x01 /* same filesystem */
#define SF_SI     0x02 /* use powers of 1000 instead of 1024 */
#define SF_IGNS   0x04 /* ignore too small terminal sizes */
#define SF_NOCFM  0x08 /* don't confirm file deletion */
#define SF_IGNE   0x10 /* ignore errors when deleting */

/* Browse Flags (int bflags) */
#define BF_NAME   0x01
#define BF_SIZE   0x02
#define BF_NDIRF  0x04 /* Normally, dirs before files, setting this disables it */
#define BF_DESC   0x08
#define BF_HIDE   0x10 /* don't show hidden files... */
#define BF_SORT   0x20 /* no need to resort, list is already in correct order */
#define BF_AS     0x40 /* show apparent sizes instead of disk usage */
#define BF_INFO   0x80 /* show file information window */

/* States */
#define ST_CALC   0
#define ST_BROWSE 1
#define ST_DEL    2
#define ST_HELP   3
#define ST_QUIT   4



/*
 *    S T R U C T U R E S
 */
struct dir {
  struct dir *parent, *next, *sub;
  char *name;
  off_t size, asize;
  unsigned long items;
  unsigned char flags;
}; 



/*
 *    G L O B A L   V A R I A B L E S
 *
 * (all defined in main.c)
 */
/* main directory data */
extern struct dir *dat;
/* global settings */
extern int sflags, bflags, sdelay, bgraph;
/* program state */
extern int pstate;


/*
 *    G L O B A L   F U N C T I O N S
 */
/* main.c */
int input_handle(int);
/* browser.c */
void drawBrowser(int);
void showBrowser(void);
/* help.c */
void showHelp(void);
/* delete.c */
struct dir *showDelete(struct dir *);


#endif
