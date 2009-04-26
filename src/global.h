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

#ifndef _global_h
#define _global_h

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <limits.h>

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


/* File Flags (struct dir -> flags) */
#define FF_DIR    0x01
#define FF_FILE   0x02
#define FF_ERR    0x04 /* error while reading this item */
#define FF_OTHFS  0x08 /* excluded because it was an other filesystem */
#define FF_EXL    0x10 /* excluded using exlude patterns */
#define FF_SERR   0x20 /* error in subdirectory */
#define FF_BSEL   0x40 /* selected */

/* Program states */
#define ST_CALC   0
#define ST_BROWSE 1
#define ST_DEL    2
#define ST_HELP   3
#define ST_QUIT   4


/* structure representing a file or directory */
struct dir {
  struct dir *parent, *next, *sub;
  char *name;
  off_t size, asize;
  unsigned long items;
  unsigned char flags;
}; 

/* program state */
extern int pstate;
/* minimum screen update interval when calculating, in ms */
extern long update_delay;

/* handle input from keyboard and update display */
int input_handle(int);


/* import all other global functions and variables */
#include "exclude.h"
#include "util.h"
#include "calc.h"
#include "delete.h"
#include "browser.h"
#include "help.h"
#include "path.h"

#endif
