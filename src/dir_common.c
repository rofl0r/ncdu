/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2012 Yoran Heling

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


char *dir_curpath;   /* Full path of the last seen item. */
struct dir_output dir_output;
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


struct dir *dir_createstruct(const char *name) {
  static struct dir *d = NULL;
  static size_t len = 0;
  size_t req = SDIRSIZE+strlen(name);
  if(len < req) {
    len = req < SDIRSIZE+256 ? SDIRSIZE+256 : req < len*2 ? len*2 : req;
    d = realloc(d, len);
  }
  memset(d, 0, SDIRSIZE);
  strcpy(d->name, name);
  return d;
}
