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

#ifndef _browser_h
#define _browser_h

#include "ncdu.h"

/* Browse Flags */
#define BF_NAME   0x01
#define BF_SIZE   0x02
#define BF_NDIRF  0x04 /* Normally, dirs before files, setting this disables it */
#define BF_DESC   0x08
#define BF_HIDE   0x10 /* don't show hidden files... */
#define BF_SORT   0x20 /* no need to resort, list is already in correct order */
#define BF_AS     0x40 /* show apparent sizes instead of disk usage */
#define BF_INFO   0x80 /* show file information window */

struct state_browser {
  struct dir *cur;    /* head of current directory */
  char graph;
  unsigned char flags;
};
extern struct state_browser stbrowse;


int  browse_key(int);
int  browse_draw(void);


#endif

