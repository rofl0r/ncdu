/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2016 Yoran Heling

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


char *formatsize(int64_t from) {
  static char dat[10]; /* "xxx.x MiB" */
  float r = from;
  char c = ' ';
  if (si) {
    if(r < 1000.0f)   { }
    else if(r < 1e6f) { c = 'K'; r/=1e3f; }
    else if(r < 1e9f) { c = 'M'; r/=1e6f; }
    else if(r < 1e12f){ c = 'G'; r/=1e9f; }
    else if(r < 1e15f){ c = 'T'; r/=1e12f; }
    else if(r < 1e18f){ c = 'P'; r/=1e15f; }
    else              { c = 'E'; r/=1e18f; }
    sprintf(dat, "%5.1f %cB", r, c);
  }
  else {
    if(r < 1000.0f)      { }
    else if(r < 1023e3f) { c = 'K'; r/=1024.0f; }
    else if(r < 1023e6f) { c = 'M'; r/=1048576.0f; }
    else if(r < 1023e9f) { c = 'G'; r/=1073741824.0f; }
    else if(r < 1023e12f){ c = 'T'; r/=1099511627776.0f; }
    else if(r < 1023e15f){ c = 'P'; r/=1125899906842624.0f; }
    else                 { c = 'E'; r/=1152921504606846976.0f; }
    sprintf(dat, "%5.1f %c%cB", r, c, c == ' ' ? ' ' : 'i');
  }
  return dat;
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
  attron(A_BOLD);
  mvaddstr(subwinr, subwinc+4, title);
  attroff(A_BOLD);
}


void ncprint(int r, int c, char *fmt, ...) {
  va_list arg;
  va_start(arg, fmt);
  move(subwinr+r, subwinc+c);
  vw_printw(stdscr, fmt, arg);
  va_end(arg);
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
