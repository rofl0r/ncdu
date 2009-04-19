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

#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

int winrows, wincols;
int subwinr, subwinc;

char cropstrdat[4096];
char formatsizedat[8];
char fullsizedat[20]; /* max: 999.999.999.999.999 */


char *cropstr(const char *from, int s) {
  int i, j, o = strlen(from);
  if(o < s) {
    strcpy(cropstrdat, from);
    return cropstrdat;
  }
  j=s/2-3;
  for(i=0; i<j; i++)
    cropstrdat[i] = from[i];
  cropstrdat[i] = '.';
  cropstrdat[++i] = '.';
  cropstrdat[++i] = '.';
  j=o-s;
  while(++i<s)
    cropstrdat[i] = from[j+i];
  cropstrdat[s] = '\0';
  return cropstrdat;
}


char *formatsize(const off_t from) {
  float r = from; 
  char c = ' ';
  if(r < 1000.0f)      { }
  else if(r < 1023e3f) { c = 'k'; r/=1024.0f; }
  else if(r < 1023e6f) { c = 'M'; r/=1048576.0f; }
  else if(r < 1023e9f) { c = 'G'; r/=1073741824.0f; }
  else                 { c = 'T'; r/=1099511627776.0f; }
  sprintf(formatsizedat, "%5.1f%cB", r, c);
  return formatsizedat;
}


char *fullsize(const off_t from) {
  char tmp[20];
  off_t n = from;
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
    fullsizedat[j++] = tmp[i];
    if(i != 0 && i%3 == 0)
      fullsizedat[j++] = '.';
  }
  fullsizedat[j] = '\0';

  return fullsizedat;
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


void nccreate(int height, int width, char *title) {
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


void freedir_rec(struct dir *dr) {
  struct dir *tmp, *tmp2;
  tmp2 = dr;
  while((tmp = tmp2) != NULL) {
    if(tmp->sub) freedir_rec(tmp->sub);
    free(tmp->name);
    tmp2 = tmp->next;
    free(tmp);
  }
}


void freedir(struct dir *dr) {
  struct dir *tmp;

  /* update sizes of parent directories */
  tmp = dr;
  while((tmp = tmp->parent) != NULL) {
    tmp->size -= dr->size;
    tmp->asize -= dr->asize;
    tmp->items -= dr->items+1;
  }

  /* free dr->sub recursively */
  if(dr->sub)
    freedir_rec(dr->sub);
 
  /* update references */
  if(dr->parent) {
    /* item is at the top of the dir, refer to next item */
    if(dr->parent->sub == dr)
      dr->parent->sub = dr->next;
    /* else, get the previous item and update it's "next"-reference */
    else
      for(tmp = dr->parent->sub; tmp != NULL; tmp = tmp->next)
        if(tmp->next == dr)
          tmp->next = dr->next;
  }

  free(dr->name);
  free(dr);
}


char *getpath(struct dir *cur, char *to) {
  struct dir *d, *list[100];
  int c = 0;

  for(d = cur; (d = d->parent) != NULL; )
    list[c++] = d;

  to[0] = '\0';
  while(c--) {
    if(list[c]->parent && list[c]->parent->name[strlen(list[c]->parent->name)-1] != '/')
      strcat(to, "/");
    strcat(to, list[c]->name);
  }

  return to;
}

