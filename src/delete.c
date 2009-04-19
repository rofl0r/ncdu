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
#include "util.h"
#include "browser.h"

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>


#define DS_CONFIRM  0
#define DS_PROGRESS 1
#define DS_FAILED   2


int delete_delay = 100;

suseconds_t lastupdate;
struct dir *root, *nextsel;
char noconfirm = 0,
     ignoreerr = 0,
     state, seloption, curfile[PATH_MAX];
int lasterrno;


void delete_draw_confirm() {
  nccreate(6, 60, "Confirm delete");

  ncprint(1, 2, "Are you sure you want to delete \"%s\"%c",
    cropstr(root->name, 21), root->flags & FF_DIR ? ' ' : '?');
  if(root->flags & FF_DIR)
    ncprint(2, 18, "and all of its contents?");

  if(seloption == 0)
    attron(A_REVERSE);
  ncaddstr(4, 15, "yes");
  attroff(A_REVERSE);
  if(seloption == 1)
    attron(A_REVERSE);
  ncaddstr(4, 24, "no");
  attroff(A_REVERSE);
  if(seloption == 2)
    attron(A_REVERSE);
  ncaddstr(4, 31, "don't ask me again");
  attroff(A_REVERSE);

  ncmove(4, seloption == 0 ? 15 : seloption == 1 ? 24 : 31);
}


void delete_draw_progress() {
  nccreate(6, 60, "Deleting...");

  ncaddstr(1, 2, cropstr(curfile, 47));
  ncaddstr(4, 41, "Press q to abort");
}


void delete_draw_error() {
  nccreate(6, 60, "Error!");

  ncprint(1, 2, "Can't delete %s:", cropstr(curfile, 42));
  ncaddstr(2, 4, strerror(lasterrno));

  if(seloption == 0)
    attron(A_REVERSE);
  ncaddstr(4, 14, "abort");
  attroff(A_REVERSE);
  if(seloption == 1)
    attron(A_REVERSE);
  ncaddstr(4, 23, "ignore");
  attroff(A_REVERSE);
  if(seloption == 2)
    attron(A_REVERSE);
  ncaddstr(4, 33, "ignore all");
  attroff(A_REVERSE);
}


int delete_draw() {
  struct timeval tv;

  switch(state) {
    case DS_CONFIRM:
      browse_draw();
      delete_draw_confirm();
      return 0;
    case DS_PROGRESS:
      gettimeofday(&tv, (void *)NULL);
      tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / delete_delay;
      if(lastupdate != tv.tv_usec) {
        browse_draw();
        delete_draw_progress();
        lastupdate = tv.tv_usec;
        return 0;
      }
      return 1;
    case DS_FAILED:
      browse_draw();
      delete_draw_error();
      return 0;
  }
  return 1;
}


int delete_key(int ch) {
  /* confirm */
  if(state == DS_CONFIRM)
    switch(ch) {
      case KEY_LEFT:
        if(--seloption < 0)
          seloption = 0;
        break;
      case KEY_RIGHT:
        if(++seloption > 2)
          seloption = 2;
        break;
      case '\n':
        if(seloption == 1)
          return 1;
        if(seloption == 2)
          noconfirm++;
        state = DS_PROGRESS;
        break;
      case 'q':
        return 1;
    }
  /* processing deletion */
  else if(state == DS_PROGRESS)
    switch(ch) {
      case 'q':
        return 1;
    }
  /* error */
  else if(state == DS_FAILED)
    switch(ch) {
      case KEY_LEFT:
        if(--seloption < 0)
          seloption = 0;
        break;
      case KEY_RIGHT:
        if(++seloption > 2)
          seloption = 2;
        break;
      case 10:
        if(seloption == 0)
          return 1;
        if(seloption == 2)
          ignoreerr++;
        state = DS_PROGRESS;
        break;
      case 'q':
        return 1;
    }

  return 0;
}


int delete_dir(struct dir *dr) {
  struct dir *nxt, *cur;
  int r;
  char file[PATH_MAX];

  getpath(dr, file);
  if(file[strlen(file)-1] != '/')
    strcat(file, "/");
  strcat(file, dr->name);

  /* check for input or screen resizes */
  strcpy(curfile, file);
  if(input_handle(0))
    return 1;

  /* do the actual deleting */
  if(dr->flags & FF_DIR) {
    if(dr->sub != NULL) {
      nxt = dr->sub;
      while(nxt != NULL) {
        cur = nxt;
        nxt = cur->next;
        if(delete_dir(cur))
          return 1;
      }
    }
    r = rmdir(file);
  } else
    r = unlink(file);

  /* error occured, ask user what to do */
  if(r == -1 && !ignoreerr) {
    state = DS_FAILED;
    lasterrno = errno;
    while(state == DS_FAILED)
      if(input_handle(0))
        return 1;
  }

  freedir(dr);
  return 0;
}


void delete_process() {
  /* determine dir to open after successful delete */
  struct dir *n = root->parent->sub != root ? root->parent->sub : root->next ? root->next : root->parent;

  /* confirm */
  seloption = 1;
  while(state == DS_CONFIRM && !noconfirm)
    if(input_handle(0))
      return browse_init(root);

  /* delete */
  lastupdate = 999;
  seloption = 0;
  if(delete_dir(root))
    browse_init(root);
  else {
    browse_init(n);
    if(nextsel)
      nextsel->flags |= FF_BSEL;
  }
}


void delete_init(struct dir *dr, struct dir *s) {
  state = DS_CONFIRM;
  root = dr;
  pstate = ST_DEL;
  nextsel = s;
}

