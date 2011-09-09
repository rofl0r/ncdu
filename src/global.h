/* ncdu - NCurses Disk Usage 
    
  Copyright (c) 2007-2010 Yoran Heling

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
#include <sys/stat.h>

/* File Flags (struct dir -> flags) */
#define FF_DIR    0x01
#define FF_FILE   0x02
#define FF_ERR    0x04 /* error while reading this item */
#define FF_OTHFS  0x08 /* excluded because it was an other filesystem */
#define FF_EXL    0x10 /* excluded using exlude patterns */
#define FF_SERR   0x20 /* error in subdirectory */
#define FF_HLNKC  0x40 /* hard link candidate (file with st_nlink > 1) */
#define FF_BSEL   0x80 /* selected */

/* Program states */
#define ST_CALC   0
#define ST_BROWSE 1
#define ST_DEL    2
#define ST_HELP   3


/* structure representing a file or directory
 * XXX: probably a good idea to get rid of the custom _t types and use
 *      fixed-size integers instead, which are much more predictable */
struct dir {
  struct dir *parent, *next, *prev, *sub, *hlnk;
  off_t size, asize;
  ino_t ino;
  unsigned long items;
  dev_t dev;
  unsigned char flags;
  char name[3]; /* must be large enough to hold ".." */
}; 
/* sizeof(total dir) = SDIRSIZE + strlen(name) = sizeof(struct dir) - 3 + strlen(name) + 1 */
#define SDIRSIZE (sizeof(struct dir)-2)

/* Ideally, the name array should be as large as the padding added to the end of
 * the struct, but I can't figure out a portable way to calculate this. We can
 * be sure that it's at least 3 bytes, though, as the struct is aligned to at
 * least 4 bytes and the flags field is a single byte. */


/* program state */
extern int pstate;
/* read-only flag */
extern int read_only;
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
#include "dirlist.h"

#endif
