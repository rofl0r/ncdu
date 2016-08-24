/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2016 Yoran Heling

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
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>


/* set S_BLKSIZE if not defined already in sys/stat.h */
#ifndef S_BLKSIZE
# define S_BLKSIZE 512
#endif


int dir_scan_smfs; /* Stay on the same filesystem */

static uint64_t curdev;   /* current device we're scanning on */


/* Populates the struct dir item with information from the stat struct. Sets
 * everything necessary for output_dir.item() except FF_ERR and FF_EXL. */
static void stat_to_dir(struct dir *d, struct stat *fs) {
  d->ino = (uint64_t)fs->st_ino;
  d->dev = (uint64_t)fs->st_dev;

  if(S_ISREG(fs->st_mode))
    d->flags |= FF_FILE;
  else if(S_ISDIR(fs->st_mode))
    d->flags |= FF_DIR;

  if(!S_ISDIR(fs->st_mode) && fs->st_nlink > 1)
    d->flags |= FF_HLNKC;

  if(dir_scan_smfs && curdev != d->dev)
    d->flags |= FF_OTHFS;

  if(!(d->flags & (FF_OTHFS|FF_EXL))) {
    d->size = fs->st_blocks * S_BLKSIZE;
    d->asize = fs->st_size;
  }
}


/* Reads all filenames in the currently chdir'ed directory and stores it as a
 * nul-separated list of filenames. The list ends with an empty filename (i.e.
 * two nuls). . and .. are not included. Returned memory should be freed. *err
 * is set to 1 if some error occured. Returns NULL if that error was fatal.
 * The reason for reading everything in memory first and then walking through
 * the list is to avoid eating too many file descriptors in a deeply recursive
 * directory. */
static char *dir_read(int *err) {
  DIR *dir;
  struct dirent *item;
  char *buf = NULL;
  int buflen = 512;
  int off = 0;

  if((dir = opendir(".")) == NULL) {
    *err = 1;
    return NULL;
  }

  buf = malloc(buflen);
  errno = 0;

  while((item = readdir(dir)) != NULL) {
    if(item->d_name[0] == '.' && (item->d_name[1] == 0 || (item->d_name[1] == '.' && item->d_name[2] == 0)))
      continue;
    int req = off+3+strlen(item->d_name);
    if(req > buflen) {
      buflen = req < buflen*2 ? buflen*2 : req;
      buf = realloc(buf, buflen);
    }
    strcpy(buf+off, item->d_name);
    off += strlen(item->d_name)+1;
  }
  if(errno)
    *err = 1;
  if(closedir(dir) < 0)
    *err = 1;

  buf[off] = 0;
  buf[off+1] = 0;
  return buf;
}


static int dir_walk(char *);


/* Tries to recurse into the given directory item */
static int dir_scan_recurse(struct dir *d) {
  int fail = 0;
  char *dir;

  if(chdir(d->name)) {
    dir_setlasterr(dir_curpath);
    d->flags |= FF_ERR;
    if(dir_output.item(d) || dir_output.item(NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    return 0;
  }

  if((dir = dir_read(&fail)) == NULL) {
    dir_setlasterr(dir_curpath);
    d->flags |= FF_ERR;
    if(dir_output.item(d) || dir_output.item(NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    if(chdir("..")) {
      dir_seterr("Error going back to parent directory: %s", strerror(errno));
      return 1;
    } else
      return 0;
  }

  /* readdir() failed halfway, not fatal. */
  if(fail)
    d->flags |= FF_ERR;

  if(dir_output.item(d)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }
  fail = dir_walk(dir);
  if(dir_output.item(NULL)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }

  /* Not being able to chdir back is fatal */
  if(!fail && chdir("..")) {
    dir_seterr("Error going back to parent directory: %s", strerror(errno));
    return 1;
  }

  return fail;
}


/* Scans and adds a single item. Recurses into dir_walk() again if this is a
 * directory. Assumes we're chdir'ed in the directory in which this item
 * resides, i.e. d->name is a valid relative path to the item. */
static int dir_scan_item(struct dir *d) {
  struct stat st;
  int fail = 0;

#ifdef __CYGWIN__
  /* /proc/registry names may contain slashes */
  if(strchr(d->name, '/') || strchr(d->name,  '\\')) {
    d->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }
#endif

  if(exclude_match(dir_curpath))
    d->flags |= FF_EXL;

  if(!(d->flags & (FF_ERR|FF_EXL)) && lstat(d->name, &st)) {
    d->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }

  if(!(d->flags & (FF_ERR|FF_EXL)))
    stat_to_dir(d, &st);

  if(cachedir_tags && (d->flags & FF_DIR) && !(d->flags & (FF_ERR|FF_EXL|FF_OTHFS)))
    if(has_cachedir_tag(d->name)) {
      d->flags |= FF_EXL;
      d->size = d->asize = 0;
    }

  /* Recurse into the dir or output the item */
  if(d->flags & FF_DIR && !(d->flags & (FF_ERR|FF_EXL|FF_OTHFS)))
    fail = dir_scan_recurse(d);
  else if(d->flags & FF_DIR) {
    if(dir_output.item(d) || dir_output.item(NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  } else if(dir_output.item(d)) {
    dir_seterr("Output error: %s", strerror(errno));
    fail = 1;
  }

  return fail || input_handle(1);
}


/* Walks through the directory that we're currently chdir'ed to. *dir contains
 * the filenames as returned by dir_read(), and will be freed automatically by
 * this function. */
static int dir_walk(char *dir) {
  struct dir *d;
  int fail = 0;
  char *cur;

  fail = 0;
  for(cur=dir; !fail&&cur&&*cur; cur+=strlen(cur)+1) {
    dir_curpath_enter(cur);
    d = dir_createstruct(cur);
    fail = dir_scan_item(d);
    dir_curpath_leave();
  }

  free(dir);
  return fail;
}


static int process() {
  char *path;
  char *dir;
  int fail = 0;
  struct stat fs;
  struct dir *d;

  if((path = path_real(dir_curpath)) == NULL)
    dir_seterr("Error obtaining full path: %s", strerror(errno));
  else {
    dir_curpath_set(path);
    free(path);
  }

  if(!dir_fatalerr && path_chdir(dir_curpath) < 0)
    dir_seterr("Error changing directory: %s", strerror(errno));

  /* Can these even fail after a chdir? */
  if(!dir_fatalerr && lstat(".", &fs) != 0)
    dir_seterr("Error obtaining directory information: %s", strerror(errno));
  if(!dir_fatalerr && !S_ISDIR(fs.st_mode))
    dir_seterr("Not a directory");

  if(!dir_fatalerr && !(dir = dir_read(&fail)))
    dir_seterr("Error reading directory: %s", strerror(errno));

  if(!dir_fatalerr) {
    curdev = (uint64_t)fs.st_dev;
    d = dir_createstruct(dir_curpath);
    if(fail)
      d->flags |= FF_ERR;
    stat_to_dir(d, &fs);

    if(dir_output.item(d)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
    if(!fail)
      fail = dir_walk(dir);
    if(!fail && dir_output.item(NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  }

  while(dir_fatalerr && !input_handle(0))
    ;
  return dir_output.final(dir_fatalerr || fail);
}


extern int confirm_quit_while_scanning_stage_1_passed;

void dir_scan_init(const char *path) {
  dir_curpath_set(path);
  dir_setlasterr(NULL);
  dir_seterr(NULL);
  dir_process = process;
  pstate = ST_CALC;
  confirm_quit_while_scanning_stage_1_passed = 0;
}
