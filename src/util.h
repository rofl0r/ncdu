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

#ifndef _util_h
#define _util_h

#include "global.h"
#include <ncurses.h>


/* UI colors: (foreground, background, attrs)
 *  NAME         OFF                        DARK
 */
#define UI_COLORS \
  C(DEFAULT,     -1,-1,0               ,    -1,           -1,         0     )\
  C(BOX_TITLE,   -1,-1,A_BOLD          ,    COLOR_BLUE,   -1,         A_BOLD)\
  C(HD,          -1,-1,A_REVERSE       ,    COLOR_BLACK,  COLOR_CYAN, 0     )    /* header & footer */\
  C(SEL,         -1,-1,A_REVERSE       ,    COLOR_WHITE,  COLOR_GREEN,A_BOLD)\
  C(NUM,         -1,-1,0               ,    COLOR_YELLOW, -1,         A_BOLD)\
  C(NUM_HD,      -1,-1,A_REVERSE       ,    COLOR_YELLOW, COLOR_CYAN, A_BOLD)\
  C(NUM_SEL,     -1,-1,A_REVERSE       ,    COLOR_YELLOW, COLOR_GREEN,A_BOLD)\
  C(KEY,         -1,-1,A_BOLD          ,    COLOR_YELLOW, -1,         A_BOLD)\
  C(KEY_HD,      -1,-1,A_BOLD|A_REVERSE,    COLOR_YELLOW, COLOR_CYAN, A_BOLD)\
  C(DIR,         -1,-1,0               ,    COLOR_BLUE,   -1,         A_BOLD)\
  C(DIR_SEL,     -1,-1,A_REVERSE       ,    COLOR_BLUE,   COLOR_GREEN,A_BOLD)\
  C(FLAG,        -1,-1,0               ,    COLOR_RED,    -1,         0     )\
  C(FLAG_SEL,    -1,-1,A_REVERSE       ,    COLOR_RED,    COLOR_GREEN,0     )\
  C(GRAPH,       -1,-1,0               ,    COLOR_MAGENTA,-1,         0     )\
  C(GRAPH_SEL,   -1,-1,A_REVERSE       ,    COLOR_MAGENTA,COLOR_GREEN,0     )

enum ui_coltype {
#define C(name, ...) UIC_##name,
  UI_COLORS
#undef C
  UIC_NONE
};

/* Color & attribute manipulation */
extern int uic_theme;

void uic_init();
void uic_set(enum ui_coltype);


/* updated when window is resized */
extern int winrows, wincols;

/* used by the nc* functions and macros */
extern int subwinr, subwinc;

/* used by formatsize to choose between base 2 or 10 prefixes */
extern int si;


/* Macros/functions for managing struct dir and struct dir_ext */

#define dir_memsize(n)     (offsetof(struct dir, name)+1+strlen(n))
#define dir_ext_offset(n)  ((dir_memsize(n) + 7) & ~7)
#define dir_ext_memsize(n) (dir_ext_offset(n) + sizeof(struct dir_ext))

static inline struct dir_ext *dir_ext_ptr(struct dir *d) {
  return d->flags & FF_EXT
    ? (struct dir_ext *) ( ((char *)d) + dir_ext_offset(d->name) )
    : NULL;
}


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
void nccreate(int, int, const char *);

/* printf something somewhere in the last created window */
void ncprint(int, int, char *, ...);

/* Add a "tab" to a window */
void nctab(int, int, int, char *);

/* same as the w* functions of ncurses, with a color */
#define ncaddstr(r, c, s) mvaddstr(subwinr+(r), subwinc+(c), s)
#define  ncaddch(r, c, s)  mvaddch(subwinr+(r), subwinc+(c), s)
#define   ncmove(r, c)        move(subwinr+(r), subwinc+(c))

/* add stuff with a color */
#define mvaddstrc(t, r, c, s) do { uic_set(t); mvaddstr(r, c, s); } while(0)
#define  mvaddchc(t, r, c, s) do { uic_set(t);  mvaddch(r, c, s); } while(0)
#define   addstrc(t,       s) do { uic_set(t);   addstr(      s); } while(0)
#define    addchc(t,       s) do { uic_set(t);    addch(      s); } while(0)
#define ncaddstrc(t, r, c, s) do { uic_set(t); ncaddstr(r, c, s); } while(0)
#define  ncaddchc(t, r, c, s) do { uic_set(t);  ncaddch(r, c, s); } while(0)
#define  mvhlinec(t, r, c, s, n) do { uic_set(t);  mvhline(r, c, s, n); } while(0)

/* crops a string into the specified length */
char *cropstr(const char *, int);

/* Converts the given size in bytes into a float (0 <= f < 1000) and a unit string */
float formatsize(int64_t, char **);

/* print size in the form of xxx.x XB */
void printsize(enum ui_coltype, int64_t);

/* int2string with thousand separators */
char *fullsize(int64_t);

/* format's a file mode as a ls -l string */
char *fmtmode(unsigned short);

/* read locale information from the environment */
void read_locale();

/* recursively free()s a directory tree */
void freedir(struct dir *);

/* generates full path from a dir item,
   returned pointer will be overwritten with a subsequent call */
char *getpath(struct dir *);

/* returns the root element of the given dir struct */
struct dir *getroot(struct dir *);

/* Add two signed 64-bit integers. Returns INT64_MAX if the result would
 * overflow, or 0 if it would be negative. At least one of the integers must be
 * positive.
 * I use uint64_t's to detect the overflow, as (a + b < 0) relies on undefined
 * behaviour, and (INT64_MAX - b >= a) didn't work for some reason. */
#define adds64(a, b) ((a) > 0 && (b) > 0\
    ? ((uint64_t)(a) + (uint64_t)(b) > (uint64_t)INT64_MAX ? INT64_MAX : (a)+(b))\
    : (a)+(b) < 0 ? 0 : (a)+(b))

/* Adds a value to the size, asize and items fields of *d and its parents */
void addparentstats(struct dir *, int64_t, int64_t, int);


/* A simple stack implemented in macros */
#define nstack_init(_s) do {\
    (_s)->size = 10;\
    (_s)->top = 0;\
    (_s)->list = malloc(10*sizeof(*(_s)->list));\
  } while(0)

#define nstack_push(_s, _v) do {\
    if((_s)->size <= (_s)->top) {\
      (_s)->size *= 2;\
      (_s)->list = realloc((_s)->list, (_s)->size*sizeof(*(_s)->list));\
    }\
    (_s)->list[(_s)->top++] = _v;\
  } while(0)

#define nstack_pop(_s) (_s)->top--
#define nstack_top(_s, _d) ((_s)->top > 0 ? (_s)->list[(_s)->top-1] : (_d))
#define nstack_free(_s) free((_s)->list)


#endif

