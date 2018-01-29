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

#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <stdarg.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

int uic_theme;
int winrows, wincols;
int subwinr, subwinc;
int si;
char thou_sep;


char *cropstr(const char *from, int s) {
  static char dat[4096];
  int i, j, o = strlen(from);
  if(o < s) {
    strcpy(dat, from);
    return dat;
  }
  j=s/2-3;
  for(i=0; i<j; i++)
    dat[i] = from[i];
  dat[i] = '.';
  dat[++i] = '.';
  dat[++i] = '.';
  j=o-s;
  while(++i<s)
    dat[i] = from[j+i];
  dat[s] = '\0';
  return dat;
}


float formatsize(int64_t from, char **unit) {
  float r = from;
  if (si) {
    if(r < 1000.0f)   { *unit = " B"; }
    else if(r < 1e6f) { *unit = "KB"; r/=1e3f; }
    else if(r < 1e9f) { *unit = "MB"; r/=1e6f; }
    else if(r < 1e12f){ *unit = "GB"; r/=1e9f; }
    else if(r < 1e15f){ *unit = "TB"; r/=1e12f; }
    else if(r < 1e18f){ *unit = "PB"; r/=1e15f; }
    else              { *unit = "EB"; r/=1e18f; }
  }
  else {
    if(r < 1000.0f)      { *unit = "  B"; }
    else if(r < 1023e3f) { *unit = "KiB"; r/=1024.0f; }
    else if(r < 1023e6f) { *unit = "MiB"; r/=1048576.0f; }
    else if(r < 1023e9f) { *unit = "GiB"; r/=1073741824.0f; }
    else if(r < 1023e12f){ *unit = "TiB"; r/=1099511627776.0f; }
    else if(r < 1023e15f){ *unit = "PiB"; r/=1125899906842624.0f; }
    else                 { *unit = "EiB"; r/=1152921504606846976.0f; }
  }
  return r;
}


void printsize(enum ui_coltype t, int64_t from) {
  char *unit;
  float r = formatsize(from, &unit);
  uic_set(t == UIC_HD ? UIC_NUM_HD : t == UIC_SEL ? UIC_NUM_SEL : UIC_NUM);
  printw("%5.1f", r);
  addchc(t, ' ');
  addstrc(t, unit);
}


char *fullsize(int64_t from) {
  static char dat[26]; /* max: 9.223.372.036.854.775.807  (= 2^63-1) */
  char tmp[26];
  int64_t n = from;
  int i, j;

  /* the K&R method - more portable than sprintf with %lld */
  i = 0;
  do {
    tmp[i++] = n % 10 + '0';
  } while((n /= 10) > 0);
  tmp[i] = '\0';

  /* reverse and add thousand seperators */
  j = 0;
  while(i--) {
    dat[j++] = tmp[i];
    if(i != 0 && i%3 == 0)
      dat[j++] = thou_sep;
  }
  dat[j] = '\0';

  return dat;
}


char *fmtmode(unsigned short mode) {
  static char buf[11];
  unsigned short ft = mode & S_IFMT;
  buf[0] = ft == S_IFDIR  ? 'd'
         : ft == S_IFREG  ? '-'
         : ft == S_IFLNK  ? 'l'
         : ft == S_IFIFO  ? 'p'
         : ft == S_IFSOCK ? 's'
         : ft == S_IFCHR  ? 'c'
         : ft == S_IFBLK  ? 'b' : '?';
  buf[1] = mode & 0400 ? 'r' : '-';
  buf[2] = mode & 0200 ? 'w' : '-';
  buf[3] = mode & 0100 ? 'x' : '-';
  buf[4] = mode & 0040 ? 'r' : '-';
  buf[5] = mode & 0020 ? 'w' : '-';
  buf[6] = mode & 0010 ? 'x' : '-';
  buf[7] = mode & 0004 ? 'r' : '-';
  buf[8] = mode & 0002 ? 'w' : '-';
  buf[9] = mode & 0001 ? 'x' : '-';
  buf[10] = 0;
  return buf;
}


void read_locale() {
  thou_sep = '.';
#ifdef HAVE_LOCALE_H
  setlocale(LC_ALL, "");
  char *locale_thou_sep = localeconv()->thousands_sep;
  if(locale_thou_sep && 1 == strlen(locale_thou_sep))
    thou_sep = locale_thou_sep[0];
#endif
}


int ncresize(int minrows, int mincols) {
  int ch;

  getmaxyx(stdscr, winrows, wincols);
  while((minrows && winrows < minrows) || (mincols && wincols < mincols)) {
    erase();
    mvaddstr(0, 0, "Warning: terminal too small,");
    mvaddstr(1, 1, "please either resize your terminal,");
    mvaddstr(2, 1, "press i to ignore, or press q to quit.");
    refresh();
    nodelay(stdscr, 0);
    ch = getch();
    getmaxyx(stdscr, winrows, wincols);
    if(ch == 'q') {
      erase();
      refresh();
      endwin();
      exit(0);
    }
    if(ch == 'i')
      return 1;
  }
  erase();
  return 0;
}


void nccreate(int height, int width, const char *title) {
  int i;

  uic_set(UIC_DEFAULT);
  subwinr = winrows/2-height/2;
  subwinc = wincols/2-width/2;

  /* clear window */
  for(i=0; i<height; i++)
    mvhline(subwinr+i, subwinc, ' ', width);

  /* box() only works around curses windows, so create our own */
  move(subwinr, subwinc);
  addch(ACS_ULCORNER);
  for(i=0; i<width-2; i++)
    addch(ACS_HLINE);
  addch(ACS_URCORNER);

  move(subwinr+height-1, subwinc);
  addch(ACS_LLCORNER);
  for(i=0; i<width-2; i++)
    addch(ACS_HLINE);
  addch(ACS_LRCORNER);

  mvvline(subwinr+1, subwinc, ACS_VLINE, height-2);
  mvvline(subwinr+1, subwinc+width-1, ACS_VLINE, height-2);

  /* title */
  uic_set(UIC_BOX_TITLE);
  mvaddstr(subwinr, subwinc+4, title);
  uic_set(UIC_DEFAULT);
}


void ncprint(int r, int c, char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  move(subwinr+r, subwinc+c);
  vw_printw(stdscr, fmt, arg);
  va_end(arg);
}


void nctab(int c, int sel, int num, char *str) {
  uic_set(sel ? UIC_KEY_HD : UIC_KEY);
  ncprint(0, c, "%d", num);
  uic_set(sel ? UIC_HD : UIC_DEFAULT);
  addch(':');
  addstr(str);
  uic_set(UIC_DEFAULT);
}


static int colors[] = {
#define C(name, ...) 0,
  UI_COLORS
#undef C
  0
};
static int lastcolor = 0;


static const struct {
  short fg, bg;
  int attr;
} color_defs[] = {
#define C(name, off_fg, off_bg, off_a, dark_fg, dark_bg, dark_a) \
  {off_fg,  off_bg,  off_a}, \
  {dark_fg, dark_bg, dark_a},
  UI_COLORS
#undef C
  {0,0,0}
};

void uic_init() {
  size_t i, j;

  start_color();
  use_default_colors();
  for(i=0; i<sizeof(colors)/sizeof(*colors)-1; i++) {
    j = i*2 + uic_theme;
    init_pair(i+1, color_defs[j].fg, color_defs[j].bg);
    colors[i] = color_defs[j].attr | COLOR_PAIR(i+1);
  }
}

void uic_set(enum ui_coltype c) {
  attroff(lastcolor);
  lastcolor = colors[(int)c];
  attron(lastcolor);
}



/* removes item from the hlnk circular linked list and size counts of the parents */
static void freedir_hlnk(struct dir *d) {
  struct dir *t, *par, *pt;
  int i;

  if(!(d->flags & FF_HLNKC))
    return;

  /* remove size from parents.
   * This works the same as with adding: only the parents in which THIS is the
   * only occurence of the hard link will be modified, if the same file still
   * exists within the parent it shouldn't get removed from the count.
   * XXX: Same note as for dir_mem.c / hlink_check():
   *      this is probably not the most efficient algorithm */
  for(i=1,par=d->parent; i&&par; par=par->parent) {
    if(d->hlnk)
      for(t=d->hlnk; i&&t!=d; t=t->hlnk)
        for(pt=t->parent; i&&pt; pt=pt->parent)
          if(pt==par)
            i=0;
    if(i) {
      par->size = adds64(par->size, -d->size);
      par->asize = adds64(par->size, -d->asize);
    }
  }

  /* remove from hlnk */
  if(d->hlnk) {
    for(t=d->hlnk; t->hlnk!=d; t=t->hlnk)
      ;
    t->hlnk = d->hlnk;
  }
}


static void freedir_rec(struct dir *dr) {
  struct dir *tmp, *tmp2;
  tmp2 = dr;
  while((tmp = tmp2) != NULL) {
    freedir_hlnk(tmp);
    /* remove item */
    if(tmp->sub) freedir_rec(tmp->sub);
    tmp2 = tmp->next;
    free(tmp);
  }
}


void freedir(struct dir *dr) {
  if(!dr)
    return;

  /* free dr->sub recursively */
  if(dr->sub)
    freedir_rec(dr->sub);

  /* update references */
  if(dr->parent && dr->parent->sub == dr)
    dr->parent->sub = dr->next;
  if(dr->prev)
    dr->prev->next = dr->next;
  if(dr->next)
    dr->next->prev = dr->prev;

  freedir_hlnk(dr);

  /* update sizes of parent directories if this isn't a hard link.
   * If this is a hard link, freedir_hlnk() would have done so already */
  addparentstats(dr->parent, dr->flags & FF_HLNKC ? 0 : -dr->size, dr->flags & FF_HLNKC ? 0 : -dr->asize, -(dr->items+1));

  free(dr);
}


char *getpath(struct dir *cur) {
  static char *dat;
  static int datl = 0;
  struct dir *d, **list;
  int c, i;

  if(!cur->name[0])
    return "/";

  c = i = 1;
  for(d=cur; d!=NULL; d=d->parent) {
    i += strlen(d->name)+1;
    c++;
  }

  if(datl == 0) {
    datl = i;
    dat = malloc(i);
  } else if(datl < i) {
    datl = i;
    dat = realloc(dat, i);
  }
  list = malloc(c*sizeof(struct dir *));

  c = 0;
  for(d=cur; d!=NULL; d=d->parent)
    list[c++] = d;

  dat[0] = '\0';
  while(c--) {
    if(list[c]->parent)
      strcat(dat, "/");
    strcat(dat, list[c]->name);
  }
  free(list);
  return dat;
}


struct dir *getroot(struct dir *d) {
  while(d && d->parent)
    d = d->parent;
  return d;
}


void addparentstats(struct dir *d, int64_t size, int64_t asize, int items) {
  while(d) {
    d->size = adds64(d->size, size);
    d->asize = adds64(d->asize, asize);
    d->items += items;
    d = d->parent;
  }
}
