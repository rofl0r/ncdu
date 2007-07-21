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


/* somewhat lazy form of removing a file/directory
   BUG: does not handle terminal resizing          */
WINDOW *rm_win;
char rm_main[PATH_MAX];
struct dir *rm_dr;

struct dir *removedir(struct dir *dr) {
  struct dir *nxt, *cur;
  char cd[PATH_MAX], fl[PATH_MAX], par = 0;
  int ch;

  getpath(dr, cd);
  strcpy(fl, cd);
  strcat(fl, "/");
  strcat(fl, dr->name);

  if(rm_win == NULL) {
    par = 1;
    strcpy(rm_main, fl);
    rm_dr = dr;
    rm_win = newwin(6, 60, winrows/2 - 3, wincols/2 - 30);
    keypad(rm_win, TRUE);
    box(rm_win, 0, 0);
    if(sflags & SF_NOCFM)
      goto doit;

    wattron(rm_win, A_BOLD);
    mvwaddstr(rm_win, 0, 4, "Confirm...");
    wattroff(rm_win, A_BOLD);
    mvwprintw(rm_win, 2, 2, "Are you sure you want to delete the selected %s?", dr->flags & FF_DIR ? "directory" : "file");
/*    mvwaddstr(rm_win, 3, 3, cropdir(fl, 56));*/
    wattron(rm_win, A_BOLD);
    mvwaddstr(rm_win, 4, 8, "y:");
    mvwaddstr(rm_win, 4,20, "n:");
    mvwaddstr(rm_win, 4,31, "a:");
    wattroff(rm_win, A_BOLD);
    mvwaddstr(rm_win, 4,11, "yes");
    mvwaddstr(rm_win, 4,23, "no");
    mvwaddstr(rm_win, 4,34, "don't ask me again");
    wrefresh(rm_win);
    ch = wgetch(rm_win);
    if(ch != 'y' && ch != 'a') {
      delwin(rm_win);
      rm_win = NULL;
      return(dr);
    }
    if(ch == 'a')
      sflags |= SF_NOCFM;

    doit:
    werase(rm_win);
    box(rm_win, 0, 0);
    wattron(rm_win, A_BOLD);
    mvwaddstr(rm_win, 0, 4, "Deleting...");
    wattroff(rm_win, A_BOLD);
    mvwprintw(rm_win, 2, 2, "Deleting %s...", cropdir(rm_main, 44));
    wrefresh(rm_win);
  }

  if(dr->flags & FF_DIR) {
    if(dr->sub != NULL) {
      nxt = dr->sub;
      while(nxt->prev != NULL)
        nxt = nxt->prev;
      while(nxt != NULL) {
        cur = nxt;
        nxt = cur->next;
        if(removedir(cur) == rm_dr) {
          if(rm_dr == dr)
            break;
          if(rm_win != NULL)
            delwin(rm_win);
          rm_win = NULL;
          return(rm_dr);
        }
      }
    }
    ch = rmdir(fl);
  } else
    ch = unlink(fl);

  if(ch == -1 && !(sflags & SF_IGNE)) {
    mvwaddstr(rm_win, 2, 2, "                                                ");
    mvwprintw(rm_win, 1, 2, "Error deleting %s", cropdir(fl, 42));
    mvwaddstr(rm_win, 2, 3, cropdir(strerror(errno), 55));
    wattron(rm_win, A_BOLD);
    mvwaddstr(rm_win, 4, 8, "a:");
    mvwaddstr(rm_win, 4,21, "i:");
    mvwaddstr(rm_win, 4,35, "I:");
    wattroff(rm_win, A_BOLD);
    mvwaddstr(rm_win, 4,11, "abort");
    mvwaddstr(rm_win, 4,24, "ignore");
    mvwaddstr(rm_win, 4,38, "ignore all");
    wrefresh(rm_win);
    ch = wgetch(rm_win);
    if((ch != 'i' && ch != 'I') || dr == rm_dr) {
      delwin(rm_win);
      rm_win = NULL;
      return(rm_dr);
    }
    if(ch == 'I')
      sflags |= SF_IGNE;
    
    mvwaddstr(rm_win, 1, 2, "                                                         ");
    mvwaddstr(rm_win, 2, 2, "                                                         ");
    mvwaddstr(rm_win, 4, 2, "                                                         ");
    mvwprintw(rm_win, 2, 2, "Deleting %s...", cropdir(rm_main, 44));
    wrefresh(rm_win);
    return(dr);
  }

  if(par == 1) {
    delwin(rm_win);
    rm_win = NULL;
  }
  return(freedir(dr));
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
    r = 1;
  else if(!(bflags & BF_NDIRF) && !(y->flags & FF_DIR) && x->flags & FF_DIR)
    r = -1;
  else if(bflags & BF_NAME)
    r = strcmp(a->name, b->name);
  else if(bflags & BF_FILES)
    r = (a->files - b->files);
  if(r == 0)
    r = a->size > b->size ? 1 : (a->size == b->size ? 0 : -1);
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

int helpScreen(void) {
  WINDOW *hlp;
  int ch=0, x, y;

  hlp = newwin(15, 60, winrows/2-7, wincols/2-30);
  curs_set(0);
  if(!helpwin) helpwin = 1;
  do {
    if(ch == ERR)
      continue;
    if(ch && ch != '1' && ch != '2' && ch != '3' && ch != '4')
      break;
    if(ch)
      helpwin = ch - 48;

    werase(hlp);
    box(hlp, 0, 0);
    wattron(hlp, A_BOLD);
    mvwaddstr(hlp, 0, 4, "ncdu help");
    wattroff(hlp, A_BOLD);
    mvwaddstr(hlp, 13, 32, "Press any key to continue");
    switch(helpwin) {
      case 1:
        wattron(hlp, A_BOLD);
        mvwaddstr(hlp, 1, 30, "1:Keys");
        wattroff(hlp, A_BOLD);
        mvwaddstr(hlp, 1, 39, "2:Format");
        mvwaddstr(hlp, 1, 50, "3:About");
        wattron(hlp, A_BOLD);
        mvwaddstr(hlp, 3,  7, "up/down");
        mvwaddstr(hlp, 4,  3, "right/enter");
        mvwaddstr(hlp, 5, 10, "left");
        mvwaddstr(hlp, 6, 11, "n/s");
        mvwaddch( hlp, 7, 13, 'd');
        mvwaddch( hlp, 8, 13, 't');
        mvwaddch( hlp, 9, 13, 'g');
        mvwaddch( hlp,10, 13, 'p');
        mvwaddch( hlp,11, 13, 'h');
        mvwaddch( hlp,12, 13, 'q');
        wattroff(hlp, A_BOLD);
        mvwaddstr(hlp, 3, 16, "Cycle through the items");
        mvwaddstr(hlp, 4, 16, "Open directory");
        mvwaddstr(hlp, 5, 16, "Previous directory");
        mvwaddstr(hlp, 6, 16, "Sort by name or size (asc/desc)");
        mvwaddstr(hlp, 7, 16, "Delete selected file or directory");
        mvwaddstr(hlp, 8, 16, "Toggle dirs before files when sorting");
        mvwaddstr(hlp, 9, 16, "Show percentage and/or graph");
        mvwaddstr(hlp,10, 16, "Toggle between powers of 1000 and 1024");
        mvwaddstr(hlp,11, 16, "Show/hide hidden files");
        mvwaddstr(hlp,12, 16, "Quit ncdu");
        break;
      case 2:
        mvwaddstr(hlp, 1, 30, "1:Keys");
        wattron(hlp, A_BOLD);
        mvwaddstr(hlp, 1, 39, "2:Format");
        wattroff(hlp, A_BOLD);
        mvwaddstr(hlp, 1, 50, "3:About");
        wattron(hlp, A_BOLD);
        mvwaddstr(hlp, 3, 3, "X  [size]  [file or directory]");
        wattroff(hlp, A_BOLD);
        mvwaddstr(hlp, 5, 4, "The X is only present in the following cases:");
        wattron(hlp, A_BOLD);
        mvwaddch(hlp, 6, 4, '!');
        mvwaddch(hlp, 7, 4, '.');
        mvwaddch(hlp, 8, 4, '>');
        mvwaddch(hlp, 9, 4, '@');
        mvwaddch(hlp,10, 4, 'e');
        wattroff(hlp, A_BOLD);
        mvwaddstr(hlp, 6, 7, "An error occured while reading this directory");
        mvwaddstr(hlp, 7, 7, "An error occured while reading a subdirectory");
        mvwaddstr(hlp, 8, 7, "Directory was on an other filesystem");
        mvwaddstr(hlp, 9, 7, "This is not a file nor a dir (symlink, socket, ...)");
        mvwaddstr(hlp,10, 7, "Empty directory");
        break;
      case 3:
        /* Indeed, too much spare time */
        mvwaddstr(hlp, 1, 30, "1:Keys");
        mvwaddstr(hlp, 1, 39, "2:Format");
        wattron(hlp, A_BOLD);
        mvwaddstr(hlp, 1, 50, "3:About");
        wattroff(hlp, A_BOLD);
        wattron(hlp, A_REVERSE);
        x=12;y=4;
        /* N */
        mvwaddstr(hlp, y+0, x+0, "      ");
        mvwaddstr(hlp, y+1, x+0, "  ");
        mvwaddstr(hlp, y+2, x+0, "  ");
        mvwaddstr(hlp, y+3, x+0, "  ");
        mvwaddstr(hlp, y+4, x+0, "  ");
        mvwaddstr(hlp, y+1, x+4, "  ");
        mvwaddstr(hlp, y+2, x+4, "  ");
        mvwaddstr(hlp, y+3, x+4, "  ");
        mvwaddstr(hlp, y+4, x+4, "  ");
        /* C */
        mvwaddstr(hlp, y+0, x+8, "     ");
        mvwaddstr(hlp, y+1, x+8, "  ");
        mvwaddstr(hlp, y+2, x+8, "  ");
        mvwaddstr(hlp, y+3, x+8, "  ");
        mvwaddstr(hlp, y+4, x+8, "     ");
        /* D */
        mvwaddstr(hlp, y+0, x+19, "  ");
        mvwaddstr(hlp, y+1, x+19, "  ");
        mvwaddstr(hlp, y+2, x+15, "      ");
        mvwaddstr(hlp, y+3, x+15, "  ");
        mvwaddstr(hlp, y+3, x+19, "  ");
        mvwaddstr(hlp, y+4, x+15, "      ");
        /* U */
        mvwaddstr(hlp, y+0, x+23, "  ");
        mvwaddstr(hlp, y+1, x+23, "  ");
        mvwaddstr(hlp, y+2, x+23, "  ");
        mvwaddstr(hlp, y+3, x+23, "  ");
        mvwaddstr(hlp, y+0, x+27, "  ");
        mvwaddstr(hlp, y+1, x+27, "  ");
        mvwaddstr(hlp, y+2, x+27, "  ");
        mvwaddstr(hlp, y+3, x+27, "  ");
        mvwaddstr(hlp, y+4, x+23, "      ");
        wattroff(hlp, A_REVERSE);
        mvwaddstr(hlp, y+0, x+30, "NCurses");
        mvwaddstr(hlp, y+1, x+30, "Disk");
        mvwaddstr(hlp, y+2, x+30, "Usage");
        mvwprintw(hlp, y+4, x+30, "%s", PACKAGE_VERSION);

        mvwaddstr(hlp,10,  7, "Written by Yoran Heling <projects@yorhel.nl>");
        mvwaddstr(hlp,11, 16, "http://dev.yorhel.nl/ncdu/");
        break;
      case 4:
        mvwaddstr(hlp, 1, 30, "1:Keys"); 
        mvwaddstr(hlp, 1, 39, "2:Format");
        mvwaddstr(hlp, 1, 50, "3:About");
        mvwaddstr(hlp, 3, 3, "There is no fourth window, baka~~");
    }
    wrefresh(hlp);
  } while((ch = getch()));
  delwin(hlp);
  touchwin(stdscr);
  refresh();
  if(ch == KEY_RESIZE) {
    ncresize();
    return(1);
  }
  helpwin = 0;
  return(0);
}

#define toggle(x,y) if(x & y) x -=y; else x |= y

/* bgraph:
    0 -> none
    1 -> graph
    2 -> percentage
    3 -> percentage + graph
*/

char graphdat[11];
char *graph(off_t max, off_t size) {
  int i, c = (int) (((float) size / (float) max) * 10.0f);
  for(i=0; i<10; i++)
    graphdat[i] = i < c ? '#' : ' ';
  graphdat[10] = '\0';
  return graphdat;
}


void browseDir(void) {
  ITEM **items;
  char **itrows, tmp[PATH_MAX], tmp2[PATH_MAX], ct;
  MENU *menu;
  struct dir *d;
  off_t max;
  int i, fls, ch, bf, s;

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
  

  erase();
  attron(A_REVERSE);
  for(i=0; i<wincols;i++) {
    mvaddch(0, i, ' ');
    mvaddch(winrows-1, i, ' ');
  }
  mvprintw(0,0,"%s %s ~ Use the arrow keys to navigate, press ? for help", PACKAGE_NAME, PACKAGE_VERSION);
  mvprintw(winrows-1, 0, " Total size: %s   Files: %-6d   Dirs: %-6d",
      cropsize(bcur->parent->size), bcur->parent->files, bcur->parent->dirs);
  attroff(A_REVERSE);
  for(i=wincols; i>=0; i--)
    mvaddch(1,i,'-');

  bcur = sortFiles(bcur);
  fls = 1; d = bcur; max = 0;
  while(d != NULL) {
    if(!(bflags & BF_HIDE) || d->name[0] != '.') {
      if(d->size > max)
        max = d->size;
      fls++;
    }
    d = d->next;
  }

  mvaddstr(1, 3, cropdir(getpath(bcur, tmp), wincols-5));

  items = calloc(fls+2, sizeof(ITEM *));
  itrows = malloc((fls+1) * sizeof(char *));
  itrows[0] = malloc((fls+1) * wincols * sizeof(char));
  for(i=1; i<fls; i++)
    itrows[i] = itrows[0] + i*wincols;

  s = 0;
  switch(bgraph) {
    case 0:
      sprintf(tmp, "%%c %%7s  %%c%%-%ds", wincols-12);
      s = 0;
      break;
    case 1:
      s = 12;
      sprintf(tmp, "%%c %%7s [%%10s] %%c%%-%ds", wincols-24);
      break;
    case 2:
      s = 7;
      sprintf(tmp, "%%c %%7s [%%4.1f%%%%] %%c%%-%ds", wincols-19);
      break;
    case 3:
      s = 18;
      sprintf(tmp, "%%c %%7s [%%4.1f%%%% %%10s] %%c%%-%ds", wincols-30);
  }

  if(bcur->parent->parent != NULL) {
    s += 11;
    for(i=0; i<s; i++)
      tmp2[i] = ' ';
    tmp2[i++] = '\0';
    strcat(tmp2, "/..");
    items[0] = new_item(tmp2, 0);
    set_item_userptr(items[0], NULL);
    i = 1;
  } else
    i = 0;
  
  d = bcur; s=-1;
  while(d != NULL) {
    if(bflags & BF_HIDE && d->name[0] == '.') {
      d = d->next;
      continue;
    }
    ct =  d->flags & FF_ERR ? '!' :
         d->flags & FF_SERR ? '.' :
        d->flags & FF_OTHFS ? '>' :
          d->flags & FF_EXL ? '<' :
        d->flags & FF_OTHER ? '@' :
          d->flags & FF_DIR
          && d->sub == NULL ? 'e' :
                              ' ' ;
    switch(bgraph) {
      case 0:
        sprintf(itrows[i], tmp, ct, cropsize(d->size),
          d->flags & FF_DIR ? '/' : ' ', cropdir(d->name, wincols-12));
        break;
      case 1:
        sprintf(itrows[i], tmp, ct, cropsize(d->size), graph(max, d->size),
          d->flags & FF_DIR ? '/' : ' ', cropdir(d->name, wincols-24));
        break;
      case 2:
        sprintf(itrows[i], tmp, ct, cropsize(d->size), ((float) d->size / (float) d->parent->size) * 100.0f,
          d->flags & FF_DIR ? '/' : ' ', cropdir(d->name, wincols-19));
        break;
      case 3:
        sprintf(itrows[i], tmp, ct, cropsize(d->size), ((float) d->size / (float) d->parent->size) * 100.0f, graph(max, d->size),
          d->flags & FF_DIR ? '/' : ' ', cropdir(d->name, wincols-30));
    }

    items[i] = new_item(itrows[i], 0);
    set_item_userptr(items[i], d);
    if(d->flags & FF_BSEL) {
      s = i;
      d->flags -= FF_BSEL;
    }
    d = d->next;
    i++;
  }
  items[i] = NULL;
  menu = new_menu(items);
  set_menu_format(menu, winrows-3, 1);
  set_menu_sub(menu, derwin(stdscr, winrows-3, wincols, 2, 0));
  set_menu_mark(menu, "");
  post_menu(menu);
  if(s >= 0)
    for(i=0; i<s; i++)
      menu_driver(menu, REQ_DOWN_ITEM);
  
  d = bcur; bf = bflags;
  refresh();
  
  if(helpwin > 0 && helpScreen() == 1) goto endbrowse;
  while((ch = getch())) {
    switch(ch) {
      case KEY_UP:    menu_driver(menu, REQ_UP_ITEM);    break;
      case KEY_DOWN:  menu_driver(menu, REQ_DOWN_ITEM);  break;
      case KEY_HOME:  menu_driver(menu, REQ_FIRST_ITEM); break;
      case KEY_LL: 
      case KEY_END:   menu_driver(menu, REQ_LAST_ITEM);  break;
      case KEY_PPAGE: menu_driver(menu, REQ_SCR_UPAGE);  break;
      case KEY_NPAGE: menu_driver(menu, REQ_SCR_DPAGE);  break;
      case '?':
        if(helpScreen() == 1)
          goto endbrowse;
        break;
      case 'q':
        bcur = NULL;
        goto endbrowse;
      case 10: case KEY_RIGHT:
        bcur = item_userptr(current_item(menu));
        if(bcur == NULL) {
          bcur = d->parent;
          goto endbrowse;
        } else if(bcur->sub != NULL) { 
          bcur->flags |= FF_BSEL;
          bcur = bcur->sub;
          goto endbrowse;
        } else
          bcur = d;
        break;
      case KEY_RESIZE:
        ncresize();
        goto endbrowse;
      case KEY_LEFT:
        if(bcur->parent->parent != NULL)
          bcur = bcur->parent;
        else
          bcur = d;
        goto endbrowse;
      case 'n':
        if(bflags & BF_NAME)
          toggle(bflags, BF_DESC);
        else
          bflags = (bflags & BF_HIDE) + (bflags & BF_NDIRF) + BF_NAME;
        goto endbrowse;
      case 's':
        if(bflags & BF_SIZE)
          toggle(bflags, BF_DESC);
        else
          bflags = (bflags & BF_HIDE) + (bflags & BF_NDIRF) + BF_SIZE;
        goto endbrowse;
      case 'p':
        toggle(sflags, SF_SI);
        goto endbrowse;
      case 'h':
        toggle(bflags, BF_HIDE);
        goto endbrowse;
      case 't':
        toggle(bflags, BF_NDIRF);
        goto endbrowse;
      case 'g':
        if(++bgraph > 3) bgraph = 0;
        goto endbrowse;
      case 'd':
        bcur = item_userptr(current_item(menu));
        if(bcur == NULL) {
          bcur = d;
          break;
        }
        set_item_userptr(current_item(menu), NULL);
        bcur->flags |= FF_BSEL;
        bcur = removedir(bcur);
        goto endbrowse;
    }
    refresh();
  }
  endbrowse:
  if(bcur == d && (d = item_userptr(current_item(menu))) != NULL)
    d->flags |= FF_BSEL;
  unpost_menu(menu);
  for(i=fls-3;i>=0;i--)
    free_item(items[i]);
  free(items);
  free(itrows[0]);
  free(itrows);
  erase();
}

void showBrowser(void) {
  bcur = dat.sub;
  bgraph = 1;
  helpwin = 0;
  while(bcur != NULL)
    browseDir();
}




