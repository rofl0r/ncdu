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
#include <stdio.h>
#include <stdarg.h>


char *dir_curpath;   /* Full path of the last seen item. */
struct dir_output dir_output;
char *dir_fatalerr; /* Error message on a fatal error. (NULL if there was no fatal error) */
int dir_ui;         /* User interface to use */
static int confirm_quit_while_scanning_stage_1_passed; /* Additional check before quitting */
static char *lasterr; /* Path where the last error occured. */
static int curpathl; /* Allocated length of dir_curpath */
static int lasterrl; /* ^ of lasterr */


static void curpath_resize(int s) {
  if(curpathl < s) {
    curpathl = s < 128 ? 128 : s < curpathl*2 ? curpathl*2 : s;
    dir_curpath = realloc(dir_curpath, curpathl);
  }
}


void dir_curpath_set(const char *path) {
  curpath_resize(strlen(path)+1);
  strcpy(dir_curpath, path);
}


void dir_curpath_enter(const char *name) {
  curpath_resize(strlen(dir_curpath)+strlen(name)+2);
  if(dir_curpath[1])
    strcat(dir_curpath, "/");
  strcat(dir_curpath, name);
}


/* removes last component from dir_curpath */
void dir_curpath_leave() {
  char *tmp;
  if((tmp = strrchr(dir_curpath, '/')) == NULL)
    strcpy(dir_curpath, "/");
  else if(tmp != dir_curpath)
    tmp[0] = 0;
  else
    tmp[1] = 0;
}


void dir_setlasterr(const char *path) {
  if(!path) {
    free(lasterr);
    lasterr = NULL;
    lasterrl = 0;
    return;
  }
  int req = strlen(path)+1;
  if(lasterrl < req) {
    lasterrl = req;
    lasterr = realloc(lasterr, lasterrl);
  }
  strcpy(lasterr, dir_curpath);
}


void dir_seterr(const char *fmt, ...) {
  free(dir_fatalerr);
  dir_fatalerr = NULL;
  if(!fmt)
    return;

  va_list va;
  va_start(va, fmt);
  dir_fatalerr = malloc(1024); /* Should be enough for everything... */
  vsnprintf(dir_fatalerr, 1023, fmt, va);
  dir_fatalerr[1023] = 0;
  va_end(va);
}


static void draw_progress() {
  static const char scantext[] = "Scanning...";
  static const char loadtext[] = "Loading...";
  static size_t anpos = 0;
  const char *antext = dir_import_active ? loadtext : scantext;
  char ani[16] = {};
  size_t i;
  int width = wincols-5;

  nccreate(10, width, antext);

  ncaddstr(2, 2, "Total items: ");
  uic_set(UIC_NUM);
  printw("%-8d", dir_output.items);

  if(dir_output.size) {
    ncaddstrc(UIC_DEFAULT, 2, 23, "size: ");
    printsize(UIC_DEFAULT, dir_output.size);
  }

  uic_set(UIC_DEFAULT);
  ncprint(3, 2, "Current item: %s", cropstr(dir_curpath, width-18));
  if(confirm_quit_while_scanning_stage_1_passed) {
    ncaddstr(8, width-26, "Press ");
    addchc(UIC_KEY, 'y');
    addstrc(UIC_DEFAULT, " to confirm abort");
  } else {
    ncaddstr(8, width-18, "Press ");
    addchc(UIC_KEY, 'q');
    addstrc(UIC_DEFAULT, " to abort");
  }

  /* show warning if we couldn't open a dir */
  if(lasterr) {
     attron(A_BOLD);
     ncaddstr(5, 2, "Warning:");
     attroff(A_BOLD);
     ncprint(5, 11, "error scanning %-32s", cropstr(lasterr, width-28));
     ncaddstr(6, 3, "some directory sizes may not be correct");
  }

  /* animation - but only if the screen refreshes more than or once every second */
  if(update_delay <= 1000) {
    if(++anpos == strlen(antext)*2)
       anpos = 0;
    memset(ani, ' ', strlen(antext));
    if(anpos < strlen(antext))
      for(i=0; i<=anpos; i++)
        ani[i] = antext[i];
    else
      for(i=strlen(antext)-1; i>anpos-strlen(antext); i--)
        ani[i] = antext[i];
  } else
    strcpy(ani, antext);
  ncaddstr(8, 3, ani);
}


static void draw_error(char *cur, char *msg) {
  int width = wincols-5;
  nccreate(7, width, "Error!");

  attron(A_BOLD);
  ncaddstr(2, 2, "Error:");
  attroff(A_BOLD);

  ncprint(2, 9, "could not open %s", cropstr(cur, width-26));
  ncprint(3, 4, "%s", cropstr(msg, width-8));
  ncaddstr(5, width-30, "press any key to continue...");
}


void dir_draw() {
  float f;
  char *unit;

  switch(dir_ui) {
  case 0:
    if(dir_fatalerr)
      fprintf(stderr, "%s.\n", dir_fatalerr);
    break;
  case 1:
    if(dir_fatalerr)
      fprintf(stderr, "\r%s.\n", dir_fatalerr);
    else if(dir_output.size) {
      f = formatsize(dir_output.size, &unit);
      fprintf(stderr, "\r%-55s %8d files /%5.1f %s",
        cropstr(dir_curpath, 55), dir_output.items, f, unit);
    } else
      fprintf(stderr, "\r%-65s %8d files", cropstr(dir_curpath, 65), dir_output.items);
    break;
  case 2:
    browse_draw();
    if(dir_fatalerr)
      draw_error(dir_curpath, dir_fatalerr);
    else
      draw_progress();
    break;
  }
}


/* This function can't be called unless dir_ui == 2
 * (Doesn't really matter either way). */
int dir_key(int ch) {
  if(dir_fatalerr)
    return 1;
  if(confirm_quit && confirm_quit_while_scanning_stage_1_passed) {
    if (ch == 'y'|| ch == 'Y') {
      return 1;
    } else {
      confirm_quit_while_scanning_stage_1_passed = 0;
      return 0;
    }
  } else if(ch == 'q') {
    if(confirm_quit) {
      confirm_quit_while_scanning_stage_1_passed = 1;
      return 0;
    } else
      return 1;
  }
  return 0;
}
