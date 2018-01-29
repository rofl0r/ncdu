/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2018 Yoran Heling

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

/* Note: all functions below include a 'reference to parent dir' node at the
 *       top of the list. */

#ifndef _dirlist_h
#define _dirlist_h

#include "global.h"


#define DL_NOCHANGE   -1
#define DL_COL_NAME    0
#define DL_COL_SIZE    1
#define DL_COL_ASIZE   2
#define DL_COL_ITEMS   3


void dirlist_open(struct dir *);

/* Get the next non-hidden item,
 * NULL = get first non-hidden item */
struct dir *dirlist_next(struct dir *);

/* Get the struct dir item relative to the selected item, or the item nearest to the requested item
 * i = 0 get selected item
 * hidden items aren't considered */
struct dir *dirlist_get(int i);

/* Get/set the first visible item in the list on the screen */
struct dir *dirlist_top(int hint);

/* Set selected dir (must be in the currently opened directory, obviously) */
void dirlist_select(struct dir *);

/* Change sort column (arguments should have a NO_CHANGE option) */
void dirlist_set_sort(int column, int desc, int df);

/* Set the hidden thingy */
void dirlist_set_hidden(int hidden);


/* DO NOT WRITE TO ANY OF THE BELOW VARIABLES FROM OUTSIDE OF dirlist.c! */

/* The 'reference to parent dir' */
extern struct dir *dirlist_parent;

/* The actual parent dir */
extern struct dir *dirlist_par;

/* current sorting configuration (set with dirlist_set_sort()) */
extern int dirlist_sort_desc, dirlist_sort_col, dirlist_sort_df;

/* set with dirlist_set_hidden() */
extern int dirlist_hidden;

/* maximum size of an item in the opened dir */
extern int64_t dirlist_maxs, dirlist_maxa;


#endif

