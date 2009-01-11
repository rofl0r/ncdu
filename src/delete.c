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

suseconds_t lastupdate;


void drawConfirm(struct dir *del, int sel) {
  nccreate(6, 60, "Confirm delete");

  ncprint(1, 2, "Are you sure you want to delete \"%s\"%c",
    cropdir(del->name, 21), del->flags & FF_DIR ? ' ' : '?');
  if(del->flags & FF_DIR) 
    ncprint(2, 18, "and all of its contents?");

  if(sel == 0)
    attron(A_REVERSE);
  ncaddstr(4, 15, "yes");
  attroff(A_REVERSE);
  if(sel == 1)
    attron(A_REVERSE);
  ncaddstr(4, 24, "no");
  attroff(A_REVERSE);
  if(sel == 2)
    attron(A_REVERSE);
  ncaddstr(4, 31, "don't ask me again");
  attroff(A_REVERSE);

  ncmove(4, sel == 0 ? 15 : sel == 1 ? 24 : 31);
  
  refresh();
}


/* show progress */
static void drawProgress(char *file) {
  nccreate(6, 60, "Deleting...");

  ncaddstr(1, 2, cropdir(file, 47));
  ncaddstr(4, 41, "Press q to abort");

  refresh();
}


/* show error dialog */
static void drawError(int sel, char *file) {
  nccreate(6, 60, "Error!");

  ncprint(1, 2, "Can't delete %s:", cropdir(file, 42));
  ncaddstr(2, 4, strerror(errno));

  if(sel == 0)
    attron(A_REVERSE);
  ncaddstr(4, 14, "abort");
  attroff(A_REVERSE);
  if(sel == 1)
    attron(A_REVERSE);
  ncaddstr(4, 23, "ignore");
  attroff(A_REVERSE);
  if(sel == 2)
    attron(A_REVERSE);
  ncaddstr(4, 33, "ignore all");
  attroff(A_REVERSE);

  refresh();
}


struct dir *deleteDir(struct dir *dr) {
  struct dir *nxt, *cur;
  int ch, sel = 0;
  char file[PATH_MAX];
  struct timeval tv;

  getpath(dr, file);
  if(file[strlen(file)-1] != '/')
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
      while(nxt != NULL) {
        cur = nxt;
        nxt = cur->next;
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

