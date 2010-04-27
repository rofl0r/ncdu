/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2010 Yoran Heling

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


int graph = 0,
    show_as = 0,
    info_show = 0,
    info_page = 0,
    info_start = 0;



void browse_draw_info(struct dir *dr) {
  struct dir *t;
  int i;

  nccreate(11, 60, "Item info");

  if(dr->hlnk) {
    if(info_page == 0)
      attron(A_REVERSE);
    ncaddstr(0, 41, "1:Info");
    attroff(A_REVERSE);
    if(info_page == 1)
      attron(A_REVERSE);
    ncaddstr(0, 50, "2:Links");
    attroff(A_REVERSE);
  }

  switch(info_page) {
  case 0:
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
    break;

  case 1:
    for(i=0,t=dr->hlnk; t!=dr; t=t->hlnk,i++) {
      if(info_start > i)
        continue;
      if(i-info_start > 5)
        break;
      ncaddstr(2+i-info_start, 3, cropstr(getpath(t), 54));
    }
    if(t!=dr)
      ncaddstr(8, 25, "-- more --");
    break;
  }

  ncaddstr(9, 32, "Press i to hide this window");
}


void browse_draw_item(struct dir *n, int row) {
  char *line, ct, dt, *size, gr[11];
  int i, o;
  float pc;

  if(n->flags & FF_BSEL)
    attron(A_REVERSE);

  /* reference to parent dir has a different format */
  if(n == dirlist_parent) {
    mvhline(row, 0, ' ', wincols);
    o = graph == 0 ? 12 :
        graph == 1 ? 24 :
        graph == 2 ? 20 :
                     31 ;
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
      n->flags & FF_HLNKC ? 'H' :
     !(n->flags & FF_FILE
    || n->flags & FF_DIR) ? '@' :
        n->flags & FF_DIR
        && n->sub == NULL ? 'e' :
                            ' ' ;
  dt = n->flags & FF_DIR ? '/' : ' ';
  size = formatsize(show_as ? n->asize : n->size);

  /* create graph (if necessary) */
  if(graph) {
    /* percentage */
    if((pc = (float)(show_as ? n->parent->asize : n->parent->size)) < 1)
      pc = 1.0f;
    pc = ((float)(show_as ? n->asize : n->size) / pc) * 100.0f;
    /* graph */
    if(graph == 1 || graph == 3) {
      o = (int)(10.0f*(float)(show_as ? n->asize : n->size) / (float)(show_as ? dirlist_maxa : dirlist_maxs));
      for(i=0; i<10; i++)
        gr[i] = i < o ? '#' : ' ';
      gr[10] = '\0';
    }
  }

  /* format and add item to the list */
  line = malloc(wincols > 35 ? wincols+1 : 36);
  switch(graph) {
    case 0:
      sprintf(line, "%%c %%8s  %%c%%-%ds", wincols-13);
      mvprintw(row, 0, line, ct, size, dt, cropstr(n->name, wincols-13));
      break;
    case 1:
      sprintf(line, "%%c %%8s [%%10s] %%c%%-%ds", wincols-25);
      mvprintw(row, 0, line, ct, size, gr, dt, cropstr(n->name, wincols-25));
      break;
    case 2:
      sprintf(line, "%%c %%8s [%%5.1f%%%%] %%c%%-%ds", wincols-21);
      mvprintw(row, 0, line, ct, size, pc, dt, cropstr(n->name, wincols-21));
      break;
    case 3:
      sprintf(line, "%%c %%8s [%%5.1f%%%% %%10s] %%c%%-%ds", wincols-32);
      mvprintw(row, 0, line, ct, size, pc, gr, dt, cropstr(n->name, wincols-32));
  }
  free(line);

  if(n->flags & FF_BSEL)
    attroff(A_REVERSE);
}


void browse_draw() {
  struct dir *t;
  char fmtsize[9], *tmp;
  int selected, i;

  erase();
  t = dirlist_get(0);

  /* top line - basic info */
  attron(A_REVERSE);
  mvhline(0, 0, ' ', wincols);
  mvhline(winrows-1, 0, ' ', wincols);
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ? for help", PACKAGE_NAME, PACKAGE_VERSION);
  attroff(A_REVERSE);

  /* second line - the path */
  mvhline(1, 0, '-', wincols);
  if(t) {
    mvaddch(1, 3, ' ');
    tmp = getpath(t->parent);
    mvaddstr(1, 4, cropstr(tmp, wincols-8));
    mvaddch(1, 4+((int)strlen(tmp) > wincols-8 ? wincols-8 : (int)strlen(tmp)), ' ');
  }

  /* bottom line - stats */
  attron(A_REVERSE);
  if(t) {
    strcpy(fmtsize, formatsize(t->parent->size));
    mvprintw(winrows-1, 0, " Total disk usage: %s  Apparent size: %s  Items: %d",
      fmtsize, formatsize(t->parent->asize), t->parent->items);
  } else
    mvaddstr(winrows-1, 0, " No items to display.");
  attroff(A_REVERSE);

  /* nothing to display? stop here. */
  if(!t)
    return;

  /* get start position */
  t = dirlist_get(-1*((winrows)/2));
  if(t == dirlist_next(NULL))
    t = NULL;

  /* print the list to the screen */
  for(i=0; (t=dirlist_next(t)) && i<winrows-3; i++) {
    browse_draw_item(t, 2+i);
    /* save the selected row number for later */
    if(t->flags & FF_BSEL)
      selected = i;
  }

  /* draw information window */
  t = dirlist_get(0);
  if(info_show && t != dirlist_parent)
    browse_draw_info(t);

  /* move cursor to selected row for accessibility */
  move(selected+2, 0);
}


int browse_key(int ch) {
  struct dir *t, *sel;
  int i, catch = 0;

  sel = dirlist_get(0);

  /* info window overwrites a few keys */
  if(info_show && sel)
    switch(ch) {
    case '1':
      info_page = 0;
      break;
    case '2':
      if(sel->hlnk)
        info_page = 1;
      break;
    case KEY_RIGHT:
    case 'l':
      if(sel->hlnk) {
        info_page = 1;
        catch++;
      }
      break;
    case KEY_LEFT:
    case 'h':
      if(sel->hlnk) {
        info_page = 0;
        catch++;
      }
      break;
    case KEY_UP:
    case 'k':
      if(sel->hlnk && info_page == 1) {
        if(info_start > 0)
          info_start--;
        catch++;
      }
      break;
    case KEY_DOWN:
    case 'j':
    case ' ':
      if(sel->hlnk && info_page == 1) {
        for(i=0,t=sel->hlnk; t!=sel; t=t->hlnk)
          i++;
        if(i > info_start+6)
          info_start++;
        catch++;
      }
      break;
    }

  if(!catch)
    switch(ch) {
    /* selecting items */
    case KEY_UP:
    case 'k':
      dirlist_select(dirlist_get(-1));
      info_start = 0;
      break;
    case KEY_DOWN:
    case 'j':
      dirlist_select(dirlist_get(1));
      info_start = 0;
      break;
    case KEY_HOME:
      dirlist_select(dirlist_next(NULL));
      info_start = 0;
      break;
    case KEY_LL:
    case KEY_END:
      dirlist_select(dirlist_get(1<<30));
      info_start = 0;
      break;
    case KEY_PPAGE:
      dirlist_select(dirlist_get(-1*(winrows-3)));
      info_start = 0;
      break;
    case KEY_NPAGE:
      dirlist_select(dirlist_get(winrows-3));
      info_start = 0;
      break;

    /* sorting items */
    case 'n':
      dirlist_set_sort(DL_COL_NAME, dirlist_sort_col == DL_COL_NAME ? !dirlist_sort_desc : DL_NOCHANGE, DL_NOCHANGE);
      info_show = 0;
      break;
    case 's':
      i = show_as ? DL_COL_ASIZE : DL_COL_SIZE;
      dirlist_set_sort(i, dirlist_sort_col == i ? !dirlist_sort_desc : DL_NOCHANGE, DL_NOCHANGE);
      info_show = 0;
      break;
    case 'e':
      dirlist_set_hidden(!dirlist_hidden);
      info_show = 0;
      break;
    case 't':
      dirlist_set_sort(DL_NOCHANGE, DL_NOCHANGE, dirlist_sort_df);
      info_show = 0;
      break;
    case 'a':
      show_as = !show_as;
      if(dirlist_sort_col == DL_COL_ASIZE || dirlist_sort_col == DL_COL_SIZE)
        dirlist_set_sort(show_as ? DL_COL_ASIZE : DL_COL_SIZE, DL_NOCHANGE, DL_NOCHANGE);
      info_show = 0;
      break;

    /* browsing */
    case 10:
    case KEY_RIGHT:
    case 'l':
      if(sel != NULL && sel->sub != NULL)
        dirlist_open(sel->sub);
      info_show = 0;
      break;
    case KEY_LEFT:
    case 'h':
    case '<':
      if(sel != NULL && sel->parent->parent != NULL)
        dirlist_open(sel->parent);
      info_show = 0;
      break;

    /* and other stuff */
    case 'r':
      if(sel != NULL)
        calc_init(getpath(sel->parent), sel->parent);
      info_show = 0;
      break;
    case 'q':
      if(info_show)
        info_show = 0;
      else
        return 1;
      break;
    case 'g':
      if(++graph > 3)
        graph = 0;
      info_show = 0;
      break;
    case 'i':
      info_show = !info_show;
      break;
    case '?':
      help_init();
      info_show = 0;
      break;
    case 'd':
      if(sel == NULL || sel == dirlist_parent)
        break;
      info_show = 0;
      if((t = dirlist_get(1)) == sel)
        t = dirlist_get(-1);
      delete_init(sel, t);
      break;
    }

  /* make sure the info_* options are correct */
  sel = dirlist_get(0);
  if(!info_show || sel == dirlist_parent)
    info_show = info_page = info_start = 0;
  else if(sel && !sel->hlnk)
    info_page = info_start = 0;

  return 0;
}


void browse_init(struct dir *cur) {
  pstate = ST_BROWSE;
  dirlist_open(cur);
}

