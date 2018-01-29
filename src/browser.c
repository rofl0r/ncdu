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

#include "global.h"

#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <time.h>


static int graph = 1, show_as = 0, info_show = 0, info_page = 0, info_start = 0, show_items = 0;
static char *message = NULL;



static void browse_draw_info(struct dir *dr) {
  struct dir *t;
  struct dir_ext *e = dir_ext_ptr(dr);
  char mbuf[46];
  int i;

  nccreate(11, 60, "Item info");

  if(dr->hlnk) {
    nctab(41, info_page == 0, 1, "Info");
    nctab(50, info_page == 1, 2, "Links");
  }

  switch(info_page) {
  case 0:
    attron(A_BOLD);
    ncaddstr(2, 3, "Name:");
    ncaddstr(3, 3, "Path:");
    if(!e)
      ncaddstr(4, 3, "Type:");
    else {
      ncaddstr(4, 3, "Mode:");
      ncaddstr(4, 21, "UID:");
      ncaddstr(4, 33, "GID:");
      ncaddstr(5, 3, "Last modified:");
    }
    ncaddstr(6, 3, "   Disk usage:");
    ncaddstr(7, 3, "Apparent size:");
    attroff(A_BOLD);

    ncaddstr(2,  9, cropstr(dr->name, 49));
    ncaddstr(3,  9, cropstr(getpath(dr->parent), 49));
    ncaddstr(4,  9, dr->flags & FF_DIR ? "Directory" : dr->flags & FF_FILE ? "File" : "Other");

    if(e) {
      ncaddstr(4, 9, fmtmode(e->mode));
      ncprint(4, 26, "%d", e->uid);
      ncprint(4, 38, "%d", e->gid);
      time_t t = (time_t)e->mtime;
      strftime(mbuf, sizeof(mbuf), "%Y-%m-%d %H:%M:%S %z", localtime(&t));
      ncaddstr(5, 18, mbuf);
    }

    ncmove(6, 18);
    printsize(UIC_DEFAULT, dr->size);
    addstrc(UIC_DEFAULT, " (");
    addstrc(UIC_NUM, fullsize(dr->size));
    addstrc(UIC_DEFAULT, " B)");

    ncmove(7, 18);
    printsize(UIC_DEFAULT, dr->asize);
    addstrc(UIC_DEFAULT, " (");
    addstrc(UIC_NUM, fullsize(dr->asize));
    addstrc(UIC_DEFAULT, " B)");
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

  ncaddstr(9, 31, "Press ");
  addchc(UIC_KEY, 'i');
  addstrc(UIC_DEFAULT, " to hide this window");
}


static void browse_draw_flag(struct dir *n, int *x) {
  addchc(n->flags & FF_BSEL ? UIC_FLAG_SEL : UIC_FLAG,
      n == dirlist_parent ? ' ' :
        n->flags & FF_EXL ? '<' :
        n->flags & FF_ERR ? '!' :
       n->flags & FF_SERR ? '.' :
      n->flags & FF_OTHFS ? '>' :
      n->flags & FF_HLNKC ? 'H' :
     !(n->flags & FF_FILE
    || n->flags & FF_DIR) ? '@' :
        n->flags & FF_DIR
        && n->sub == NULL ? 'e' :
                            ' ');
  *x += 2;
}


static void browse_draw_graph(struct dir *n, int *x) {
  float pc = 0.0f;
  int o, i;
  enum ui_coltype c = n->flags & FF_BSEL ? UIC_SEL : UIC_DEFAULT;

  if(!graph)
    return;

  *x += graph == 1 ? 13 : graph == 2 ? 9 : 20;
  if(n == dirlist_parent)
    return;

  addchc(c, '[');

  /* percentage (6 columns) */
  if(graph == 2 || graph == 3) {
    pc = (float)(show_as ? n->parent->asize : n->parent->size);
    if(pc < 1)
      pc = 1.0f;
    uic_set(c == UIC_SEL ? UIC_NUM_SEL : UIC_NUM);
    printw("%5.1f", ((float)(show_as ? n->asize : n->size) / pc) * 100.0f);
    addchc(c, '%');
  }

  if(graph == 3)
    addch(' ');

  /* graph (10 columns) */
  if(graph == 1 || graph == 3) {
    uic_set(c == UIC_SEL ? UIC_GRAPH_SEL : UIC_GRAPH);
    o = (int)(10.0f*(float)(show_as ? n->asize : n->size) / (float)(show_as ? dirlist_maxa : dirlist_maxs));
    for(i=0; i<10; i++)
      addch(i < o ? '#' : ' ');
  }

  addchc(c, ']');
}


static void browse_draw_items(struct dir *n, int *x) {
  enum ui_coltype c = n->flags & FF_BSEL ? UIC_SEL : UIC_DEFAULT;

  if(!show_items)
    return;
  *x += 7;

  if(n->items > 99999) {
    addstrc(c, "> ");
    addstrc(c == UIC_SEL ? UIC_NUM_SEL : UIC_NUM, "100");
    addchc(c, 'k');
  } else if(n->items) {
    uic_set(c == UIC_SEL ? UIC_NUM_SEL : UIC_NUM);
    printw("%6s", fullsize(n->items));
  }
}


static void browse_draw_item(struct dir *n, int row) {
  int x = 0;

  enum ui_coltype c = n->flags & FF_BSEL ? UIC_SEL : UIC_DEFAULT;
  uic_set(c);
  mvhline(row, 0, ' ', wincols);
  move(row, 0);

  browse_draw_flag(n, &x);
  move(row, x);

  if(n != dirlist_parent)
    printsize(c, show_as ? n->asize : n->size);
  x += 10;
  move(row, x);

  browse_draw_graph(n, &x);
  move(row, x);

  browse_draw_items(n, &x);
  move(row, x);

  if(n->flags & FF_DIR)
    c = c == UIC_SEL ? UIC_DIR_SEL : UIC_DIR;
  addchc(c, n->flags & FF_DIR ? '/' : ' ');
  addstrc(c, cropstr(n->name, wincols-x-1));
}


void browse_draw() {
  struct dir *t;
  char *tmp;
  int selected = 0, i;

  erase();
  t = dirlist_get(0);

  /* top line - basic info */
  uic_set(UIC_HD);
  mvhline(0, 0, ' ', wincols);
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ", PACKAGE_NAME, PACKAGE_VERSION);
  addchc(UIC_KEY_HD, '?');
  addstrc(UIC_HD, " for help");
  if(dir_import_active)
    mvaddstr(0, wincols-10, "[imported]");
  else if(read_only)
    mvaddstr(0, wincols-11, "[read-only]");

  /* second line - the path */
  mvhlinec(UIC_DEFAULT, 1, 0, '-', wincols);
  if(dirlist_par) {
    mvaddchc(UIC_DEFAULT, 1, 3, ' ');
    tmp = getpath(dirlist_par);
    mvaddstrc(UIC_DIR, 1, 4, cropstr(tmp, wincols-8));
    mvaddchc(UIC_DEFAULT, 1, 4+((int)strlen(tmp) > wincols-8 ? wincols-8 : (int)strlen(tmp)), ' ');
  }

  /* bottom line - stats */
  uic_set(UIC_HD);
  mvhline(winrows-1, 0, ' ', wincols);
  if(t) {
    mvaddstr(winrows-1, 0, " Total disk usage: ");
    printsize(UIC_HD, t->parent->size);
    addstrc(UIC_HD, "  Apparent size: ");
    uic_set(UIC_NUM_HD);
    printsize(UIC_HD, t->parent->asize);
    addstrc(UIC_HD, "  Items: ");
    uic_set(UIC_NUM_HD);
    printw("%d", t->parent->items);
  } else
    mvaddstr(winrows-1, 0, " No items to display.");
  uic_set(UIC_DEFAULT);

  /* nothing to display? stop here. */
  if(!t)
    return;

  /* get start position */
  t = dirlist_top(0);

  /* print the list to the screen */
  for(i=0; t && i<winrows-3; t=dirlist_next(t),i++) {
    browse_draw_item(t, 2+i);
    /* save the selected row number for later */
    if(t->flags & FF_BSEL)
      selected = i;
  }

  /* draw message window */
  if(message) {
    nccreate(6, 60, "Message");
    ncaddstr(2, 2, message);
    ncaddstr(4, 34, "Press any key to continue");
  }

  /* draw information window */
  t = dirlist_get(0);
  if(!message && info_show && t != dirlist_parent)
    browse_draw_info(t);

  /* move cursor to selected row for accessibility */
  move(selected+2, 0);
}


int browse_key(int ch) {
  struct dir *t, *sel;
  int i, catch = 0;

  /* message window overwrites all keys */
  if(message) {
    message = NULL;
    return 0;
  }

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
      dirlist_top(-1);
      info_start = 0;
      break;
    case KEY_DOWN:
    case 'j':
      dirlist_select(dirlist_get(1));
      dirlist_top(1);
      info_start = 0;
      break;
    case KEY_HOME:
      dirlist_select(dirlist_next(NULL));
      dirlist_top(2);
      info_start = 0;
      break;
    case KEY_LL:
    case KEY_END:
      dirlist_select(dirlist_get(1<<30));
      dirlist_top(1);
      info_start = 0;
      break;
    case KEY_PPAGE:
      dirlist_select(dirlist_get(-1*(winrows-3)));
      dirlist_top(-1);
      info_start = 0;
      break;
    case KEY_NPAGE:
      dirlist_select(dirlist_get(winrows-3));
      dirlist_top(1);
      info_start = 0;
      break;

    /* sorting items */
    case 'n':
      dirlist_set_sort(DL_COL_NAME, dirlist_sort_col == DL_COL_NAME ? !dirlist_sort_desc : 0, DL_NOCHANGE);
      info_show = 0;
      break;
    case 's':
      i = show_as ? DL_COL_ASIZE : DL_COL_SIZE;
      dirlist_set_sort(i, dirlist_sort_col == i ? !dirlist_sort_desc : 1, DL_NOCHANGE);
      info_show = 0;
      break;
    case 'C':
      dirlist_set_sort(DL_COL_ITEMS, dirlist_sort_col == DL_COL_ITEMS ? !dirlist_sort_desc : 1, DL_NOCHANGE);
      info_show = 0;
      break;
    case 'e':
      dirlist_set_hidden(!dirlist_hidden);
      info_show = 0;
      break;
    case 't':
      dirlist_set_sort(DL_NOCHANGE, DL_NOCHANGE, !dirlist_sort_df);
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
      if(sel != NULL && sel->flags & FF_DIR) {
        dirlist_open(sel == dirlist_parent ? dirlist_par->parent : sel);
        dirlist_top(-3);
      }
      info_show = 0;
      break;
    case KEY_LEFT:
    case KEY_BACKSPACE:
    case 'h':
    case '<':
      if(dirlist_par && dirlist_par->parent != NULL) {
        dirlist_open(dirlist_par->parent);
        dirlist_top(-3);
      }
      info_show = 0;
      break;

    /* and other stuff */
    case 'r':
      if(dir_import_active) {
        message = "Directory imported from file, won't refresh.";
        break;
      }
      if(dirlist_par) {
        dir_ui = 2;
        dir_mem_init(dirlist_par);
        dir_scan_init(getpath(dirlist_par));
      }
      info_show = 0;
      break;
    case 'q':
      if(info_show)
        info_show = 0;
      else
        if (confirm_quit)
          quit_init();
        else return 1;
      break;
    case 'g':
      if(++graph > 3)
        graph = 0;
      info_show = 0;
      break;
    case 'c':
      show_items = !show_items;
      break;
    case 'i':
      info_show = !info_show;
      break;
    case '?':
      help_init();
      info_show = 0;
      break;
    case 'd':
      if(read_only >= 1 || dir_import_active) {
        message = read_only >= 1
          ? "File deletion disabled in read-only mode."
          : "File deletion not available for imported directories.";
        break;
      }
      if(sel == NULL || sel == dirlist_parent)
        break;
      info_show = 0;
      if((t = dirlist_get(1)) == sel)
        if((t = dirlist_get(-1)) == sel || t == dirlist_parent)
          t = NULL;
      delete_init(sel, t);
      break;
     case 'b':
      if(read_only >= 2 || dir_import_active) {
        message = read_only >= 2
          ? "Shell feature disabled in read-only mode."
          : "Shell feature not available for imported directories.";
        break;
      }
      shell_init();
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


void browse_init(struct dir *par) {
  pstate = ST_BROWSE;
  message = NULL;
  dirlist_open(par);
}

