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

#include "global.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>


#define BF_NAME   0x01
#define BF_SIZE   0x02
#define BF_NDIRF  0x04 /* Normally, dirs before files, setting this disables it */
#define BF_DESC   0x08
#define BF_HIDE   0x10 /* don't show hidden files... */
#define BF_AS     0x40 /* show apparent sizes instead of disk usage */
#define BF_INFO   0x80 /* show file information window */

struct dir *browse_dir = NULL;
unsigned char graph = 0;
unsigned char flags = BF_SIZE | BF_DESC;


#define ishidden(x) (flags & BF_HIDE && (\
    (x->next != browse_dir && (x->name[0] == '.' || x->name[strlen(x->name)-1] == '~'))\
    || x->flags & FF_EXL))


int browse_cmp(struct dir *x, struct dir *y) {
  struct dir *a, *b;
  int r = 0;

  if(flags & BF_DESC) {
    a = y; b = x;
  } else {
    b = y; a = x;
  }
  if(!(flags & BF_NDIRF) && y->flags & FF_DIR && !(x->flags & FF_DIR))
    return(1);
  if(!(flags & BF_NDIRF) && !(y->flags & FF_DIR) && x->flags & FF_DIR)
    return(-1);

  if(flags & BF_NAME)
    r = strcmp(a->name, b->name);
  if(flags & BF_AS) {
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
    if(nmerges <= 1) {
      if(list->parent)
        list->parent->sub = list;
      return list;
    }
    insize *= 2;
  }
}


void browse_draw_info(struct dir *dr) {
  nccreate(11, 60, "Item info");

  attron(A_BOLD);
  ncaddstr(2, 3, "Name:");
  ncaddstr(3, 3, "Path:");
  ncaddstr(4, 3, "Type:");
  ncaddstr(6, 3, "   Disk usage:");
  ncaddstr(7, 3, "Apparent size:");
  attroff(A_BOLD);

  ncaddstr(2,  9, cropstr(dr->name, 49));
  ncaddstr(3,  9, cropstr(getpath(dr->parent), 49));
  ncaddstr(4,  9, dr->flags & FF_DIR ? "Directory"
      : dr->flags & FF_FILE ? "File" : "Other (link, device, socket, ..)");
  ncprint(6, 18, "%s (%s B)", formatsize(dr->size),  fullsize(dr->size));
  ncprint(7, 18, "%s (%s B)", formatsize(dr->asize), fullsize(dr->asize));

  ncaddstr(9, 32, "Press i to hide this window");
}


void browse_draw_item(struct dir *n, int row, off_t max, int ispar) {
  char *line, ct, dt, *size, gr[11];
  int i, o;
  float pc;

  if(n->flags & FF_BSEL)
    attron(A_REVERSE);

  /* reference to parent dir has a different format */
  if(ispar) {
    mvhline(row, 0, ' ', wincols);
    o = graph == 0 ? 11 :
        graph == 1 ? 23 :
        graph == 2 ? 18 :
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
  size = formatsize(flags & BF_AS ? n->asize : n->size);

  /* create graph (if necessary) */
  if((pc = (float)(flags & BF_AS ? n->parent->asize : n->parent->size)) < 1)
    pc = 1.0f;
  pc = ((float)(flags & BF_AS ? n->asize : n->size) / pc) * 100.0f;
  if(graph == 1 || graph == 3) {
    o = (int)(10.0f*(float)(flags & BF_AS ? n->asize : n->size) / (float)max);
    for(i=0; i<10; i++)
      gr[i] = i < o ? '#' : ' ';
    gr[10] = '\0';
  }

  /* format and add item to the list */
  line = malloc(winrows+1);
  switch(graph) {
    case 0:
      sprintf(line, "%%c %%7s  %%c%%-%ds", wincols-12);
      mvprintw(row, 0, line, ct, size, dt, cropstr(n->name, wincols-12));
      break;
    case 1:
      sprintf(line, "%%c %%7s [%%10s] %%c%%-%ds", wincols-24);
      mvprintw(row, 0, line, ct, size, gr, dt, cropstr(n->name, wincols-24));
      break;
    case 2:
      sprintf(line, "%%c %%7s [%%5.1f%%%%] %%c%%-%ds", wincols-20);
      mvprintw(row, 0, line, ct, size, pc, dt, cropstr(n->name, wincols-19));
      break;
    case 3:
      sprintf(line, "%%c %%7s [%%5.1f%%%% %%10s] %%c%%-%ds", wincols-31);
      mvprintw(row, 0, line, ct, size, pc, gr, dt, cropstr(n->name, wincols-30));
  }
  free(line);

  if(n->flags & FF_BSEL)
    attroff(A_REVERSE);
}


void browse_draw() {
  struct dir *n, ref, *cur, *sel = NULL;
  char *line, *tmp;
  int selected, i;
  off_t max = 1;

  erase();
  cur = browse_dir;

  /* create header and status bar */
  attron(A_REVERSE);
  mvhline(0, 0, ' ', wincols);
  mvhline(winrows-1, 0, ' ', wincols);
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ? for help", PACKAGE_NAME, PACKAGE_VERSION);

  if(cur) {
    line = malloc(winrows+1);
    strcpy(line, formatsize(cur->parent->size));
    mvprintw(winrows-1, 0, " Total disk usage: %s  Apparent size: %s  Items: %d",
      line, formatsize(cur->parent->asize), cur->parent->items);
    free(line);
  } else
    mvaddstr(winrows-1, 0, " No items to display.");
  attroff(A_REVERSE);

  mvhline(1, 0, '-', wincols);
  if(cur) {
    mvaddch(1, 3, ' ');
    tmp = getpath(cur->parent);
    mvaddstr(1, 4, cropstr(tmp, wincols-8));
    mvaddch(1, 4+((int)strlen(tmp) > wincols-8 ? wincols-8 : (int)strlen(tmp)), ' ');
  }

  if(!cur)
    return;

  /* add reference to parent dir */
  memset(&ref, 0, sizeof(struct dir));
  if(cur->parent->parent) {
    ref.name = "..";
    ref.next = cur;
    ref.parent = cur->parent;
    cur = &ref;
  }

  /* get maximum size and selected item */
  for(n=cur, selected=-1, i=0; n!=NULL; n=n->next) {
    if(ishidden(n))
      continue;
    if(n->flags & FF_BSEL) {
      selected = i;
      sel = n;
    }
    if((flags & BF_AS ? n->asize : n->size) > max)
      max = flags & BF_AS ? n->asize : n->size;
    i++;
  }
  if(selected < 0)
    cur->flags |= FF_BSEL;

  /* determine start position */
  for(n=cur,i=0; n!=NULL; n=n->next) {
    if(ishidden(n))
      continue;
    if(i == (selected / (winrows-3)) * (winrows-3))
      break;
    i++;
  }
  selected -= i;

  /* print the list to the screen */
  for(i=0; n!=NULL && i<winrows-3; n=n->next) {
    if(ishidden(n))
      continue;
    browse_draw_item(n, 2+i++, max, n == &ref);
  }

  /* draw information window */
  if(sel && (flags & BF_INFO) && sel != &ref)
    browse_draw_info(sel);

  /* move cursor to selected row for accessibility */
  move(selected+2, 0);
}


void browse_key_sel(int change) {
  struct dir *n, *cur, par;
  int i, max;

  if((cur = browse_dir) == NULL)
    return;
  par.next = cur;
  par.flags = 0;
  if(cur->parent->parent)
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

int browse_key(int ch) {
  char sort = 0, nonfo = 0;
  struct dir *n, *r;

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
      if(flags & BF_NAME)
        toggle(flags, BF_DESC);
      else
        flags = (flags & BF_HIDE) + (flags & BF_NDIRF) + BF_NAME;
      sort++;
      nonfo++;
      break;
    case 's':
      if(flags & BF_SIZE)
        toggle(flags, BF_DESC);
      else
        flags = (flags & BF_HIDE) + (flags & BF_NDIRF) + BF_SIZE + BF_DESC;
      sort++;
      nonfo++;
      break;
    case 'h':
      toggle(flags, BF_HIDE);
      browse_key_sel(0);
      nonfo++;
      break;
    case 't':
      toggle(flags, BF_NDIRF);
      sort++;
      nonfo++;
      break;
    case 'a':
      toggle(flags, BF_AS);
      nonfo++;
      break;

    /* browsing */
    case 10:
    case KEY_RIGHT:
      for(n=browse_dir; n!=NULL; n=n->next)
        if(n->flags & FF_BSEL)
          break;
      if(n != NULL && n->sub != NULL) {
        browse_dir = n->sub;
        sort++;
      }
      if(n == NULL && browse_dir != NULL && browse_dir->parent->parent) {
        browse_dir = browse_dir->parent->parent->sub;
        sort++;
      }
      browse_key_sel(0);
      nonfo++;
      break;
    case KEY_LEFT:
      if(browse_dir != NULL && browse_dir->parent->parent != NULL) {
        browse_dir = browse_dir->parent->parent->sub;
        sort++;
      }
      browse_key_sel(0);
      nonfo++;
      break;

    /* and other stuff */
    case 'r':
      if(browse_dir != NULL)
        calc_init(getpath(browse_dir->parent), browse_dir->parent);
      nonfo++;
      break;
    case 'q':
      if(flags & BF_INFO)
        nonfo++;
      else
        return 1;
      break;
    case 'g':
      if(++graph > 3)
        graph = 0;
      nonfo++;
      break;
    case 'i':
      toggle(flags, BF_INFO);
      break;
    case '?':
      help_init();
      nonfo++;
      break;
    case 'd':
      for(n=browse_dir; n!=NULL; n=n->next)
        if(n->flags & FF_BSEL)
          break;
      if(n == NULL)
        break;
      nonfo++;
      /* quirky method of getting the next selected dir without actually selecting it */
      browse_key_sel(n->next ? 1 : -1);
      for(r=browse_dir; r!=NULL; r=r->next)
        if(r->flags & FF_BSEL)
          break;
      browse_key_sel(n->next ? -1 : 1);
      delete_init(n, r);
      break;
  }

  if(sort && browse_dir != NULL)
    browse_dir = browse_sort(browse_dir);
  if(nonfo)
    flags &= ~BF_INFO;
  return 0;
}


void browse_init(struct dir *cur) {
  pstate = ST_BROWSE;
  if(cur != NULL && cur->parent == NULL)
    browse_dir = cur->sub;
  else
    browse_dir = cur;
  if(browse_dir != NULL && browse_dir->parent->sub != browse_dir)
    browse_dir = cur->parent->sub;
  if(browse_dir != NULL)
    browse_dir = browse_sort(browse_dir);
  browse_key_sel(0);
}

