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

suseconds_t lastupdate;


void drawConfirm(struct dir *del, int sel) {
  WINDOW *cfm;

  cfm = newwin(6, 60, winrows/2-3, wincols/2-30);
  box(cfm, 0, 0);
  wattron(cfm, A_BOLD);
  mvwaddstr(cfm, 0, 4, "Confirm delete");
  wattroff(cfm, A_BOLD);

  mvwprintw(cfm, 1, 2, "Are you sure you want to delete \"%s\"%c",
    cropdir(del->name, 21), del->flags & FF_DIR ? ' ' : '?');
  if(del->flags & FF_DIR) 
    mvwprintw(cfm, 2, 18, "and all of its contents?");

  if(sel == 0)
    wattron(cfm, A_REVERSE);
  mvwaddstr(cfm, 4, 15, "yes");
  wattroff(cfm, A_REVERSE);
  if(sel == 1)
    wattron(cfm, A_REVERSE);
  mvwaddstr(cfm, 4, 24, "no");
  wattroff(cfm, A_REVERSE);
  if(sel == 2)
    wattron(cfm, A_REVERSE);
  mvwaddstr(cfm, 4, 31, "don't ask me again");
  wattroff(cfm, A_REVERSE);

  wrefresh(cfm);
  delwin(cfm);
}


/* show progress */
void drawProgress(char *file) {
  WINDOW *prg;

  prg = newwin(6, 60, winrows/2-3, wincols/2-30);
  nodelay(prg, 1);
  box(prg, 0, 0);
  wattron(prg, A_BOLD);
  mvwaddstr(prg, 0, 4, "Deleting...");
  wattroff(prg, A_BOLD); 

  mvwaddstr(prg, 1, 2, cropdir(file, 47));
  mvwaddstr(prg, 5, 41, "Press q to abort");

  wrefresh(prg);
  delwin(prg);
}


/* show error dialog */
void drawError(int sel, char *file) {
  WINDOW *err;

  err = newwin(6, 60, winrows/2-3, wincols/2-30);
  box(err, 0, 0);
  wattron(err, A_BOLD);
  mvwaddstr(err, 0, 4, "Error!");
  wattroff(err, A_BOLD);

  mvwprintw(err, 1, 2, "Can't delete %s:", cropdir(file, 42));
  mvwaddstr(err, 2, 4, strerror(errno));

  if(sel == 0)
    wattron(err, A_REVERSE);
  mvwaddstr(err, 4, 14, "abort");
  wattroff(err, A_REVERSE);
  if(sel == 1)
    wattron(err, A_REVERSE);
  mvwaddstr(err, 4, 23, "ignore");
  wattroff(err, A_REVERSE);
  if(sel == 2)
    wattron(err, A_REVERSE);
  mvwaddstr(err, 4, 33, "ignore all");
  wattroff(err, A_REVERSE);

  wrefresh(err);
  delwin(err); 
}


struct dir *deleteDir(struct dir *dr) {
  struct dir *nxt, *cur;
  int ch, sel = 0;
  char file[PATH_MAX];
  struct timeval tv;

  getpath(dr, file);
  strcat(file, "/");
  strcat(file, dr->name);

 /* check for input or screen resizes */
  nodelay(stdscr, 1);
  while((ch = getch()) != ERR) {
    if(ch == 'q')
      return(NULL);
    if(ch == KEY_RESIZE) {
      ncresize();
      drawBrowser(0);
      drawProgress(file);
    }
  }
  nodelay(stdscr, 0);

 /* don't update the screen with shorter intervals than sdelay */
  gettimeofday(&tv, (void *)NULL);
  tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / sdelay;
  if(lastupdate != tv.tv_usec) {
    drawProgress(file);
    lastupdate = tv.tv_usec;
  }

 /* do the actual deleting */
  if(dr->flags & FF_DIR) {
    if(dr->sub != NULL) {
      nxt = dr->sub;
      while(nxt->prev != NULL)
        nxt = nxt->prev;
      while(nxt != NULL) {
        cur = nxt;
        nxt = cur->next;
        if(cur->flags & FF_PAR) {
          freedir(cur);
          continue;
        }
        if(deleteDir(cur) == NULL)
          return(NULL);
      }
    }
    ch = rmdir(file);
  } else
    ch = unlink(file); 

 /* error occured, ask user what to do */
  if(ch == -1 && !(sflags & SF_IGNE)) {
    drawError(sel, file);
    while((ch = getch())) {
      switch(ch) {
        case KEY_LEFT:
          if(--sel < 0)
            sel = 0;
          break;
        case KEY_RIGHT:
          if(++sel > 2)
            sel = 2;
          break;
        case 10:
          if(sel == 0)
            return(NULL);
          if(sel == 2)
            sflags |= SF_IGNE;
          goto ignore;
        case 'q':
          return(NULL);
        case KEY_RESIZE:
          ncresize();
          drawBrowser(0);
          break;
      }
      drawError(sel, file);
    }
  };
  ignore: 

  return(freedir(dr));
}


struct dir *showDelete(struct dir *dr) {
  int ch, sel = 1;
  struct dir *ret;

 /* confirm */
  if(sflags & SF_NOCFM)
    goto doit;
  
  drawConfirm(dr, sel);
  while((ch = getch())) {
    switch(ch) {
      case KEY_LEFT:
        if(--sel < 0)
          sel = 0;
        break;
      case KEY_RIGHT:
        if(++sel > 2)
          sel = 2;
        break;
      case 10:
        if(sel == 1)
          return(dr);
        if(sel == 2)
          sflags |= SF_NOCFM;
        goto doit;
      case 'q':
        return(dr);
      case KEY_RESIZE:
        ncresize();
        drawBrowser(0);
        break;
    }
    drawConfirm(dr, sel);
  }

  doit:
  lastupdate = 999; /* just some random high value as initialisation */

  ret = deleteDir(dr);

  return(ret == NULL ? dr : ret);
}

