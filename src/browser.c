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
int helpwin;


struct dir * removedir(struct dir *dr) {
  return(dr);
}

int cmp(struct dir *x, struct dir *y) {
  struct dir *a, *b;
  int r = 0;

  if(y->flags & FF_PAR)
    return(1);
  if(x->flags & FF_PAR)
    return(-1);

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
    r = strcmp(a->name, b->name);
  return(r);
}

/* Mergesort algorithm, many thanks to
   http://www.chiark.greenend.org.uk/~sgtatham/algorithms/listsort.html
*/
struct dir *sortFiles(struct dir *list) {
  struct dir *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, i;

  while(list->prev != NULL)
    list = list->prev;
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
        e->prev = tail;
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
    (!(x->flags & FF_PAR) && x->name[0] == '.')\
    || x->flags & FF_EXL)\
  ) { i--; continue; }

void drawBrowser(int change) {
  struct dir *n;
  char tmp[PATH_MAX], ct, dt;
  int selected, i, o;
  off_t max;

  erase();

 /* exit if there are no items to display */
  if(bcur->parent == NULL) {
    if(bcur->sub == NULL) {
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
  
  mvprintw(winrows-1, 0, " Total size: %s   Files: %-6d   Dirs: %-6d",
      cropsize(bcur->parent->size), bcur->parent->files, bcur->parent->dirs);
  attroff(A_REVERSE);

  mvhline(1, 0, '-', wincols);
  mvaddstr(1, 3, cropdir(getpath(bcur, tmp), wincols-5));

 /* make sure we have the first item, and the items are in correct order */
  bcur = sortFiles(bcur);
  while(bcur->prev != NULL)
    bcur = bcur->prev;

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
    if(n->flags & FF_PAR) {
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
        n->flags & FF_OTHER ? '@' :
          n->flags & FF_DIR
          && n->sub == NULL ? 'e' :
                              ' ' ;
    dt = n->flags & FF_DIR ? '/' : ' ';

   /* format and add item to the list */
    switch(bgraph) {
      case 0:
        mvprintw(i+2, 0, tmp, ct, cropsize(n->size),
          dt, cropdir(n->name, wincols-12)
        );
        break;
      case 1:
        mvprintw(i+2, 0, tmp, ct, cropsize(n->size),
          graph(max, n->size),
          dt, cropdir(n->name, wincols-24)
        );
        break;
      case 2:
        mvprintw(i+2, 0, tmp, ct, cropsize(n->size),
          ((float) n->size / (float) n->parent->size) * 100.0f,
          dt, cropdir(n->name, wincols-19)
        );
        break;
      case 3:
        mvprintw(i+2, 0, tmp, ct, cropsize(n->size),
          ((float) n->size / (float) n->parent->size) * 100.0f, graph(max, n->size),
          dt, cropdir(n->name, wincols-30)
        );
    }
    
    if(i == selected)
      attroff(A_REVERSE);
  }
}

struct dir * selected(void) {
  struct dir *n = bcur;
  do {
    if(n->flags & FF_BSEL)
      return n;
  } while((n = n->next) != NULL);
}


#define toggle(x,y) if(x & y) x -=y; else x |= y

void showBrowser(void) {
  int ch, change;
  struct dir *n;
  
  bcur = dat.sub;
  bgraph = 1;
  nodelay(stdscr, 0);
  bflags = BF_SIZE | BF_DESC | BF_NDIRF;

  drawBrowser(0);
  refresh();

  while((ch = getch())) {
    change = 0;
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

     /* browsing */
      case 10:
      case KEY_RIGHT:
        n = selected();
        if(n->flags & FF_PAR)
          bcur = bcur->parent;
        else if(n->sub != NULL)
          bcur = n->sub;
        break;
      case KEY_LEFT:
        if(bcur->parent->parent != NULL) {
          bcur = bcur->parent;
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
      case 'd':
        n = selected();
        if(!(n->flags & FF_PAR))
          bcur = showDelete(n);
        break;
      case 'q':
        goto endloop;
    }
    drawBrowser(change);
    refresh();
  }
  endloop:
  erase();
}


