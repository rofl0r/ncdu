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

#ifndef _calc_h
#define _calc_h

#include "ncdu.h"

struct state_calc {
  char err;                /* 1/0, error or not */
  char cur[PATH_MAX];      /* current dir/item */
  char lasterr[PATH_MAX];  /* last unreadable dir/item */
  char errmsg[128];        /* error message, when err=1 */
  struct dir *parent;      /* parent directory for the calculation */
  dev_t curdev;            /* current device we're calculating on */
  suseconds_t lastupdate;  /* time of the last screen update */
  int anpos;               /* position of the animation string */
  int sterr;               /* state to go to on error (ST_BROWSE/ST_QUIT) */
};
extern struct state_calc stcalc;


void calc_process(void);
int  calc_key(int);
int  calc_draw(void);


#endif
