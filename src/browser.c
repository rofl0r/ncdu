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

#include "ncdu.h"
#include "browser.h"
#include "util.h"
#include "calc.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

#define ishidden(x) (stbrowse.flags & BF_HIDE && (\
    (x->next != stbrowse.cur && (x->name[0] == '.' || x->name[strlen(x->name)-1] == '~'))\
    || x->flags & FF_EXL))

struct state_browser stbrowse;


/*
void drawInfo(struct dir *dr) {
  char path[PATH_MAX];

  nccreate(11, 60, "Item info");

  attron(A_BOLD);
  ncaddstr(2, 3, "Name:");
  ncaddstr(3, 3, "Path:");
  ncaddstr(4, 3, "Type:");
  ncaddstr(6, 3, "   Disk usage:");
  ncaddstr(7, 3, "Apparent size:");
  attroff(A_BOLD);

  ncaddstr(2,  9, cropdir(dr->name, 49));
  ncaddstr(3,  9, cropdir(getpath(dr, path), 49));
  ncaddstr(4,  9, dr->flags & FF_DIR ? "Directory"
      : dr->flags & FF_FILE ? "File" : "Other (link, device, socket, ..)");
  ncprint(6, 18, "%s (%s B)", cropsize(dr->size),  fullsize(dr->size));
  ncprint(7, 18, "%s (%s B)", cropsize(dr->asize), fullsize(dr->asize));

  ncaddstr(9, 32, "Press i to hide this window");
}
*/


int browse_cmp(struct dir *x, struct dir *y) {
  struct dir *a, *b;
  int r = 0;

  if(stbrowse.flags & BF_DESC) {
    a = y; b = x;
  } else {
    b = y; a = x;
  }
  if(!(stbrowse.flags & BF_NDIRF) && y->flags & FF_DIR && !(x->flags & FF_DIR))
    return(1);
  if(!(stbrowse.flags & BF_NDIRF) && !(y->flags & FF_DIR) && x->flags & FF_DIR)
    return(-1);

  if(stbrowse.flags & BF_NAME)
    r = strcmp(a->name, b->name);
  if(stbrowse.flags & BF_AS) {
    if(r == 0)
      r = a->asize > b->asize ? 1 : (a->asize == b->asize ? 0 : -1);
    if(r == 0)
      r = a->size > b->size ? 1 : (a->size == b->size ? 0 : -1);
  } else {
    if(r == 0)
      r = a->size > b->size ? 1 : (a->size == b->size ? 0 : -1);
    if(r == 0)
      r = a->asize > b->asize ? 1 : (a->asize == b->asize ? 0 : -1);
  }
  if(r == 0)
    r = strcmp(x->name, y->name);
  return(r);
}


struct dir *browse_sort(struct dir *list) {
  struct dir *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

  insize = 1;
  while(1) {
    p = list;
    list = NULL;
    tail = NULL;
    nmerges = 0;
    while(p) {
      nmerges++;
      q = p;
      psize = 0;
      for(i=0; i<insize; i++) {
        psize++;
        q = q->next;
        if(!q) break;
      }
      qsize = insize;
      while(psize > 0 || (qsize > 0 && q)) {
        if(psize == 0) {
          e = q; q = q->next; qsize--;
        } else if(qsize == 0 || !q) {
          e = p; p = p->next; psize--;
        } else if(browse_cmp(p,q) <= 0) {
          e = p; p = p->next; psize--;
        } else {
          e = q; q = q->next; qsize--;
        }
        if(tail) tail->next = e;
        else     list = e;
        tail = e;
      }
      p = q;
    }
    tail->next = NULL;
    if(nmerges <= 1)
      return list;
    insize *= 2;
  }
}


void browse_draw_item(struct dir *n, int row, off_t max, int ispar) {
  char tmp[1000], ct, dt, *size, gr[11];
  int i, o;
  float pc;

  if(n->flags & FF_BSEL)
    attron(A_REVERSE);

  /* reference to parent dir has a different format */
  if(ispar) {
    mvhline(row, 0, ' ', wincols);
    o = stbrowse.graph == 0 ? 11 :
        stbrowse.graph == 1 ? 23 :
        stbrowse.graph == 2 ? 18 :
                      29 ;
    mvaddstr(row, o, "/..");
    if(n->flags & FF_BSEL)
      attroff(A_REVERSE);
    return;
  }

  /* determine indication character */
  ct =  n->flags & FF_EXL ? '<' :
        n->flags & FF_ERR ? '!' :
       n->flags & FF_SERR ? '.' :
      n->flags & FF_OTHFS ? '>' :
     !(n->flags & FF_FILE
    || n->flags & FF_DIR) ? '@' :
        n->flags & FF_DIR
        && n->sub == NULL ? 'e' :
                            ' ' ;
  dt = n->flags & FF_DIR ? '/' : ' ';
  size = formatsize(stbrowse.flags & BF_AS ? n->asize : n->size);

  /* create graph (if necessary) */
  pc = ((float)(stbrowse.flags & BF_AS ? n->asize : n->size) / (float)(stbrowse.flags & BF_AS ? n->parent->asize : n->parent->size)) * 100.0f;
  if(stbrowse.graph == 1 || stbrowse.graph == 3) {
    o = (int)(10.0f*(float)(stbrowse.flags & BF_AS ? n->asize : n->size) / (float)max);
    for(i=0; i<10; i++)
      gr[i] = i < o ? '#' : ' ';
    gr[10] = '\0';
  }

  /* format and add item to the list */
  switch(stbrowse.graph) {
    case 0:
      sprintf(tmp, "%%c %%7s  %%c%%-%ds", wincols-12);
      mvprintw(row, 0, tmp, ct, size, dt, cropstr(n->name, wincols-12));
      break;
    case 1:
      sprintf(tmp, "%%c %%7s [%%10s] %%c%%-%ds", wincols-24);
      mvprintw(row, 0, tmp, ct, size, gr, dt, cropstr(n->name, wincols-24));
      break;
    case 2:
      sprintf(tmp, "%%c %%7s [%%4.1f%%%%] %%c%%-%ds", wincols-19);
      mvprintw(row, 0, tmp, ct, size, pc, dt, cropstr(n->name, wincols-19));
      break;
    case 3:
      sprintf(tmp, "%%c %%7s [%%4.1f%%%% %%10s] %%c%%-%ds", wincols-30);
      mvprintw(row, 0, tmp, ct, size, pc, gr, dt, cropstr(n->name, wincols-30));
  }

  if(n->flags & FF_BSEL)
    attroff(A_REVERSE);
}


int browse_draw() {
  struct dir *n, ref, *cur;
  char tmp[PATH_MAX];
  int selected, i;
  off_t max = 1;

  erase();
  cur = stbrowse.cur;

  /* exit if there are no items to display */
  if(cur == NULL || cur->parent == NULL) {
    if(cur == NULL || cur->sub == NULL) {
      erase();
      refresh();
      endwin();
      printf("No items to display...\n");
      exit(0);
    } else
      stbrowse.cur = cur = cur->sub;
  }

  /* create header and status bar */
  attron(A_REVERSE);
  mvhline(0, 0, ' ', wincols);
  mvhline(winrows-1, 0, ' ', wincols);
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ? for help", PACKAGE_NAME, PACKAGE_VERSION);

  strcpy(tmp, formatsize(cur->parent->size));
  mvprintw(winrows-1, 0, " Total disk usage: %s  Apparent size: %s  Items: %d",
    tmp, formatsize(cur->parent->asize), cur->parent->items);
  attroff(A_REVERSE);

  mvhline(1, 0, '-', wincols);
  mvaddch(1, 3, ' ');
  getpath(cur, tmp);
  mvaddstr(1, 4, cropstr(tmp, wincols-8));
  mvaddch(1, 4+((int)strlen(tmp) > wincols-8 ? wincols-8 : (int)strlen(tmp)), ' ');

  /* TODO: don't sort when it's not necessary */
  cur = stbrowse.cur = browse_sort(cur);
  cur->parent->sub = cur;

  /* add reference to parent dir */
  memset(&ref, 0, sizeof(struct dir));
  if(cur->parent->parent) {
    ref.name = "..";
    ref.next = cur;
    ref.parent = cur->parent;
    cur = &ref;
  }

  /* get maximum size and selected item */
  for(n=cur, selected=i=0; n!=NULL; n=n->next) {
    if(ishidden(n))
      continue;
    if(n->flags & FF_BSEL)
      selected = i;
    if((stbrowse.flags & BF_AS ? n->asize : n->size) > max)
      max = stbrowse.flags & BF_AS ? n->asize : n->size;
    i++;
  }
  if(!selected)
    cur->flags |= FF_BSEL;

  /* determine start position */
  for(n=cur,i=0; n!=NULL; n=n->next) {
    if(ishidden(n))
      continue;
    if(i == (selected / (winrows-3)) * (winrows-3))
      break;
    i++;
  }

  /* print the list to the screen */
  for(i=0; n!=NULL && i<winrows-3; n=n->next) {
    if(ishidden(n))
      continue;
    browse_draw_item(n, 2+i++, max, n == &ref);
  }

  /* move cursor to selected row for accessibility */
  move(selected+2, 0);
  return 0;
}


void browse_key_sel(int change) {
  struct dir *n, *cur, par;
  int i, max;

  cur = stbrowse.cur;
  par.next = cur;
  if(stbrowse.cur->parent->parent)
    cur = &par;

  i = 0;
  max = -1;
  for(n=cur; n!=NULL; n=n->next) {
    if(!ishidden(n)) {
      max++;
      if(n->flags & FF_BSEL)
        i = max;
    }
    n->flags &= ~FF_BSEL;
  }
  i += change;
  i = i<0 ? 0 : i>max ? max : i;

  for(n=cur; n!=NULL; n=n->next)
    if(!ishidden(n) && !i--)
      n->flags |= FF_BSEL;
}



#define toggle(x,y) if(x & y) x -=y; else x |= y
#define hideinfo if(stbrowse.flags & BF_INFO) stbrowse.flags -= BF_INFO

int browse_key(int ch) {
  char tmp[PATH_MAX];
  struct dir *n;

  switch(ch) {
    /* selecting items */
    case KEY_UP:
      browse_key_sel(-1);
      break;
    case KEY_DOWN:
      browse_key_sel(1);
      break;
    case KEY_HOME:
      browse_key_sel(-1*(1<<30));
      break;
    case KEY_LL:
    case KEY_END:
      browse_key_sel(1<<30);
      break;
    case KEY_PPAGE:
      browse_key_sel(-1*(winrows-3));
      break;
    case KEY_NPAGE:
      browse_key_sel(winrows-3);
      break;

   /* sorting items */
    case 'n':
      hideinfo;
      if(stbrowse.flags & BF_NAME)
        toggle(stbrowse.flags, BF_DESC);
      else
        stbrowse.flags = (stbrowse.flags & BF_HIDE) + (stbrowse.flags & BF_NDIRF) + BF_NAME;
      break;
    case 's':
      hideinfo;
      if(stbrowse.flags & BF_SIZE)
        toggle(stbrowse.flags, BF_DESC);
      else
        stbrowse.flags = (stbrowse.flags & BF_HIDE) + (stbrowse.flags & BF_NDIRF) + BF_SIZE + BF_DESC;
      break;
    case 'h':
      hideinfo;
      toggle(stbrowse.flags, BF_HIDE);
      browse_key_sel(0);
      break;
    case 't':
      hideinfo;
      toggle(stbrowse.flags, BF_NDIRF);
      break;
    case 'a':
      hideinfo;
      toggle(stbrowse.flags, BF_AS);
      break;

   /* browsing */
    case 10:
    case KEY_RIGHT:
      hideinfo;
      for(n=stbrowse.cur; n!=NULL; n=n->next)
        if(n->flags & FF_BSEL)
          break;
      if(n != NULL && n->sub != NULL)
        stbrowse.cur = n->sub;
      if(n == NULL && stbrowse.cur->parent->parent)
        stbrowse.cur = stbrowse.cur->parent->parent->sub;
      break;
    case KEY_LEFT:
      hideinfo;
      if(stbrowse.cur->parent->parent != NULL)
        stbrowse.cur = stbrowse.cur->parent->parent->sub;
      break;

   /* refresh */
    case 'r':
      hideinfo;
      calc_init(getpath(stbrowse.cur, tmp), stbrowse.cur->parent);
      break;

    /* and other stuff */
    case 'q':
      return 1;
    case 'g':
      hideinfo;
      if(++stbrowse.graph > 3) stbrowse.graph = 0;
      break;
      /*
    case '?':
      hideinfo;
      showHelp();
      break;
    case 'i':
      toggle(stbrowse.flags, BF_INFO);
      break;
    case 'd':
      hideinfo;
      drawBrowser(0);
      n = selected();
      if(n != bcur->parent)
        bcur = showDelete(n);
      if(bcur && bcur->parent)
        bcur = bcur->parent->sub;
      break;
        */
  }
  return 0;
}


