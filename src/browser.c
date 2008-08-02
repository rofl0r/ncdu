/* ncdu - NCurses Disk Usage 
  
  Copyright (c) 2007 Yoran Heling

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

struct dir *bcur;


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

  ncaddstr(9, 32, "Press any key to continue");
}


int cmp(struct dir *x, struct dir *y) {
  struct dir *a, *b;
  int r = 0;

  if(bflags & BF_DESC) {
    a = y; b = x;
  } else {
    b = y; a = x;
  }
  if(!(bflags & BF_NDIRF) && y->flags & FF_DIR && !(x->flags & FF_DIR))
    return(1);
  if(!(bflags & BF_NDIRF) && !(y->flags & FF_DIR) && x->flags & FF_DIR)
    return(-1);

  if(bflags & BF_NAME)
    r = strcmp(a->name, b->name);
  if(r == 0)
    r = a->size > b->size ? 1 : (a->size == b->size ? 0 : -1);
  if(r == 0)
    r = a->asize > b->asize ? 1 : (a->asize == b->asize ? 0 : -1);
  if(r == 0)
    r = strcmp(x->name, y->name);
  return(r);
}

/* Mergesort algorithm, many thanks to
   http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
*/
struct dir *sortFiles(struct dir *list) {
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
        } else if(cmp(p,q) <= 0) {
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


char graphdat[11];
char *graph(off_t max, off_t size) {
  int i, c = (int) (((float) size / (float) max) * 10.0f);
  for(i=0; i<10; i++)
    graphdat[i] = i < c ? '#' : ' ';
  graphdat[10] = '\0';
  return graphdat;
}


#define exlhid(x) if(bflags & BF_HIDE && (\
    (x != &ref && (x->name[0] == '.' || x->name[strlen(x->name)-1] == '~'))\
    || x->flags & FF_EXL)\
  ) { i--; continue; }

void drawBrowser(int change) {
  struct dir *n, ref;
  char tmp[PATH_MAX], ct, dt, *size;
  int selected, i, o;
  off_t max = 1;

  erase();

 /* exit if there are no items to display */
  if(bcur == NULL || bcur->parent == NULL) {
    if(bcur == NULL || bcur->sub == NULL) {
      erase();
      refresh();
      endwin();
      printf("No items to display...\n");
      exit(0);
    } else
      bcur = bcur->sub;
  }

 /* create header and status bar */
  attron(A_REVERSE);
  mvhline(0, 0, ' ', wincols);
  mvhline(winrows-1, 0, ' ', wincols);
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ? for help", PACKAGE_NAME, PACKAGE_VERSION);

  strcpy(tmp, cropsize(bcur->parent->size));
  mvprintw(winrows-1, 0, " Total disk usage: %s  Apparent size: %s  Items: %d",
    tmp, cropsize(bcur->parent->asize), bcur->parent->items);
  attroff(A_REVERSE);
  
  mvhline(1, 0, '-', wincols);
  mvaddstr(1, 3, cropdir(getpath(bcur, tmp), wincols-5));

 /* make sure the items are in correct order */
  if(!(bflags & BF_SORT)) {
    bcur = sortFiles(bcur);
    bcur->parent->sub = bcur;
    bflags |= BF_SORT;
  }

 /* add reference to parent dir */
  memset(&ref, 0, sizeof(struct dir));
  if(bcur->parent->parent) {
    ref.name = "..";
    ref.next = bcur;
    ref.parent = bcur->parent;
    bcur = &ref;
  }

 /* get maximum size and selected item */
  for(n = bcur, selected = i = 0; n != NULL; n = n->next, i++) {
    exlhid(n)
    if(n->flags & FF_BSEL)
      selected = i;
    if(n->size > max)
      max = n->size;
  }

  if(selected+change < 0)
    change -= selected+change;
  if(selected+change > --i)
    change -= (selected+change)-i;
  for(n = bcur, i = 0; n != NULL; n = n->next, i++) {
    exlhid(n)
    if(i == selected && n->flags & FF_BSEL)
      n->flags -= FF_BSEL;
    if(i == selected+change)
      n->flags |= FF_BSEL;
  }
  selected += change;

 /* determine the listing format */
  switch(bgraph) {
    case 0:
      sprintf(tmp, "%%c %%7s  %%c%%-%ds", wincols-12);
      break;
    case 1:
      sprintf(tmp, "%%c %%7s [%%10s] %%c%%-%ds", wincols-24);
      break;
    case 2:
      sprintf(tmp, "%%c %%7s [%%4.1f%%%%] %%c%%-%ds", wincols-19);
      break;
    case 3:
      sprintf(tmp, "%%c %%7s [%%4.1f%%%% %%10s] %%c%%-%ds", wincols-30);
  }

 /* determine start position */
  for(n = bcur, i = 0; n != NULL; n = n->next, i++) {
    exlhid(n)
    if(i == (selected / (winrows-3)) * (winrows-3))
      break;
  }
  selected -= i;

 /* print the list to the screen */
  for(i=0; n != NULL && i < winrows-3; n = n->next, i++) {
    exlhid(n)
    if(i == selected)
      attron(A_REVERSE);

   /* reference to parent dir has a different format */
    if(n == &ref) {
      mvhline(i+2, 0, ' ', wincols);
      o = bgraph == 0 ? 11 :
          bgraph == 1 ? 23 :
          bgraph == 2 ? 18 :
                        29 ;
      mvaddstr(i+2, o, "/..");
      if(i == selected)
        attroff(A_REVERSE);
      continue;
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
    size = cropsize(bflags & BF_AS ? n->asize : n->size);

   /* format and add item to the list */
    switch(bgraph) {
      case 0:
        mvprintw(i+2, 0, tmp, ct, size,
          dt, cropdir(n->name, wincols-12)
        );
        break;
      case 1:
        mvprintw(i+2, 0, tmp, ct, size,
          graph(max, n->size),
          dt, cropdir(n->name, wincols-24)
        );
        break;
      case 2:
        mvprintw(i+2, 0, tmp, ct, size,
          ((float) n->size / (float) n->parent->size) * 100.0f,
          dt, cropdir(n->name, wincols-19)
        );
        break;
      case 3:
        mvprintw(i+2, 0, tmp, ct, size,
          ((float) n->size / (float) n->parent->size) * 100.0f, graph(max, n->size),
          dt, cropdir(n->name, wincols-30)
        );
    }
    
    if(i == selected)
      attroff(A_REVERSE);
  }

 /* move cursor to selected row for accessibility */
  move(selected+2, 0);

 /* remove reference to parent dir */
  if(bcur == &ref)
    bcur = ref.next;
}

struct dir * selected(void) {
  struct dir *n = bcur;
  do {
    if(n->flags & FF_BSEL)
      return n;
  } while((n = n->next) != NULL);
  if(bcur->parent->parent)
    return(bcur->parent);
  return NULL;
}


#define toggle(x,y) if(x & y) x -=y; else x |= y
#define resort if(bflags & BF_SORT) bflags -= BF_SORT

void showBrowser(void) {
  int ch, change, oldflags;
  char tmp[PATH_MAX];
  struct dir *n, *t, *d, *last;
  
  bcur = dat->sub;
  bgraph = 1;
  nodelay(stdscr, 0);
  bflags = BF_SIZE | BF_DESC | BF_NDIRF;

  drawBrowser(0);
  refresh();

  while((ch = getch())) {
    change = 0;
    last = bcur;
    oldflags = bflags;
    
    switch(ch) {
     /* selecting items */
      case KEY_UP:
        change = -1;
        break;
      case KEY_DOWN:
        change =  1;
        break;
      case KEY_HOME:
        change = -16777216;
        break;
      case KEY_LL:
      case KEY_END:
        change = 16777216;
        break;
      case KEY_PPAGE:
        change = -1*(winrows-3);
        break;
      case KEY_NPAGE:
        change = winrows-3;
        break;

     /* sorting items */
      case 'n':
        if(bflags & BF_NAME)
          toggle(bflags, BF_DESC);
        else
          bflags = (bflags & BF_HIDE) + (bflags & BF_NDIRF) + BF_NAME;
        break;
      case 's':
        if(bflags & BF_SIZE)
          toggle(bflags, BF_DESC);
        else
          bflags = (bflags & BF_HIDE) + (bflags & BF_NDIRF) + BF_SIZE + BF_DESC;
        break;
      case 'p':
        toggle(sflags, SF_SI);
        break;
      case 'h':
        toggle(bflags, BF_HIDE);
        break;
      case 't':
        toggle(bflags, BF_NDIRF);
        break;
      case 'a':
        toggle(bflags, BF_AS);
        break;

     /* browsing */
      case 10:
      case KEY_RIGHT:
        n = selected();
        if(n == bcur->parent)
          bcur = bcur->parent->parent->sub;
        else if(n->sub != NULL)
          bcur = n->sub;
        break;
      case KEY_LEFT:
        if(bcur->parent->parent != NULL) {
          bcur = bcur->parent->parent->sub;
        }
        break;

     /* refresh */
      case 'r':
        if((n = showCalc(getpath(bcur, tmp))) != NULL) {
         /* free current items */
          d = bcur;
          bcur = bcur->parent;
          while(d != NULL) {
            t = d;
            d = t->next;
            freedir(t);
          }

         /* update parent dir */
          bcur->sub = n->sub;
          bcur->items = n->items;
          bcur->size = n->size;
          for(t = bcur->sub; t != NULL; t = t->next)
            t->parent = bcur;

         /* update sizes of parent dirs */
          for(t = bcur; (t = t->parent) != NULL; ) {
            t->size += bcur->size;
            t->items += bcur->items;
          }

          bcur = bcur->sub;
          free(n->name);
          free(n);
        }
        break;

     /* and other stuff */
      case KEY_RESIZE:
        ncresize();
        break;
      case 'g':
        if(++bgraph > 3) bgraph = 0;
        break;
      case '?':
        showHelp();
        break;
      case 'i':
        n = selected();
        if(n != bcur->parent) {
          drawInfo(n);
          while(getch() == KEY_RESIZE) {
            drawBrowser(0);
            drawInfo(n);
          }
        }
        break;
      case 'd':
        n = selected();
        if(n != bcur->parent)
          bcur = showDelete(n);
        if(bcur && bcur->parent)
          bcur = bcur->parent->sub;
        break;
      case 'q':
        goto endloop;
    }
    if((last != bcur || (oldflags | BF_HIDE | BF_AS) != (bflags | BF_HIDE | BF_AS)) && bflags & BF_SORT)
      bflags -= BF_SORT;
    
    drawBrowser(change);
    refresh();
  }
  endloop:
  erase();
}


