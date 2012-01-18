/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2012 Yoran Heling

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

#ifndef _util_h
#define _util_h

#include "global.h"
#include <ncurses.h>

/* updated when window is resized */
extern int winrows, wincols;

/* used by the nc* functions and macros */
extern int subwinr, subwinc;


/* Instead of using several ncurses windows, we only draw to stdscr.
 * the functions nccreate, ncprint and the macros ncaddstr and ncaddch
 * mimic the behaviour of ncurses windows.
 * This works better than using ncurses windows when all windows are
 * created in the correct order: it paints directly on stdscr, so
 * wrefresh, wnoutrefresh and other window-specific functions are not
 * necessary.
 * Also, this method doesn't require any window objects, as you can
 * only create one window at a time.
*/

/* updates winrows, wincols, and displays a warning when the terminal
 * is smaller than the specified minimum size. */
int ncresize(int, int);

/* creates a new centered window with border */
void nccreate(int, int, char *);

/* printf something somewhere in the last created window */
void ncprint(int, int, char *, ...);

/* same as the w* functions of ncurses */
#define ncaddstr(r, c, s) mvaddstr(subwinr+(r), subwinc+(c), s)
#define  ncaddch(r, c, s)  mvaddch(subwinr+(r), subwinc+(c), s)
#define   ncmove(r, c)        move(subwinr+(r), subwinc+(c))

/* crops a string into the specified length */
char *cropstr(const char *, int);

/* formats size in the form of xxx.xXB */
char *formatsize(const off_t);

/* int2string with thousand separators */
char *fullsize(const off_t);

/* recursively free()s a directory tree */
void freedir(struct dir *);

/* generates full path from a dir item,
   returned pointer will be overwritten with a subsequent call */
char *getpath(struct dir *);

#endif

