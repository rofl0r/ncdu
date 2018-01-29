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

/* scratch space */
static struct dir    *buf_dir;
static struct dir_ext buf_ext[1];



/* Populates the buf_dir and buf_ext with information from the stat struct.
 * Sets everything necessary for output_dir.item() except FF_ERR and FF_EXL. */
static void stat_to_dir(struct stat *fs) {
  buf_dir->flags |= FF_EXT; /* We always read extended data because it doesn't have an additional cost */
  buf_dir->ino = (uint64_t)fs->st_ino;
  buf_dir->dev = (uint64_t)fs->st_dev;

  if(S_ISREG(fs->st_mode))
    buf_dir->flags |= FF_FILE;
  else if(S_ISDIR(fs->st_mode))
    buf_dir->flags |= FF_DIR;

  if(!S_ISDIR(fs->st_mode) && fs->st_nlink > 1)
    buf_dir->flags |= FF_HLNKC;

  if(dir_scan_smfs && curdev != buf_dir->dev)
    buf_dir->flags |= FF_OTHFS;

  if(!(buf_dir->flags & (FF_OTHFS|FF_EXL))) {
    buf_dir->size = fs->st_blocks * S_BLKSIZE;
    buf_dir->asize = fs->st_size;
  }

  buf_ext->mode  = fs->st_mode;
  buf_ext->mtime = fs->st_mtime;
  buf_ext->uid   = (int)fs->st_uid;
  buf_ext->gid   = (int)fs->st_gid;
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


/* Tries to recurse into the current directory item (buf_dir is assumed to be the current dir) */
static int dir_scan_recurse(const char *name) {
  int fail = 0;
  char *dir;

  if(chdir(name)) {
    dir_setlasterr(dir_curpath);
    buf_dir->flags |= FF_ERR;
    if(dir_output.item(buf_dir, name, buf_ext) || dir_output.item(NULL, 0, NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    return 0;
  }

  if((dir = dir_read(&fail)) == NULL) {
    dir_setlasterr(dir_curpath);
    buf_dir->flags |= FF_ERR;
    if(dir_output.item(buf_dir, name, buf_ext) || dir_output.item(NULL, 0, NULL)) {
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
    buf_dir->flags |= FF_ERR;

  if(dir_output.item(buf_dir, name, buf_ext)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }
  fail = dir_walk(dir);
  if(dir_output.item(NULL, 0, NULL)) {
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
 * resides. */
static int dir_scan_item(const char *name) {
  struct stat st;
  int fail = 0;

#ifdef __CYGWIN__
  /* /proc/registry names may contain slashes */
  if(strchr(name, '/') || strchr(name,  '\\')) {
    buf_dir->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }
#endif

  if(exclude_match(dir_curpath))
    buf_dir->flags |= FF_EXL;

  if(!(buf_dir->flags & (FF_ERR|FF_EXL)) && lstat(name, &st)) {
    buf_dir->flags |= FF_ERR;
    dir_setlasterr(dir_curpath);
  }

  if(!(buf_dir->flags & (FF_ERR|FF_EXL)))
    stat_to_dir(&st);

  if(cachedir_tags && (buf_dir->flags & FF_DIR) && !(buf_dir->flags & (FF_ERR|FF_EXL|FF_OTHFS)))
    if(has_cachedir_tag(buf_dir->name)) {
      buf_dir->flags |= FF_EXL;
      buf_dir->size = buf_dir->asize = 0;
    }

  /* Recurse into the dir or output the item */
  if(buf_dir->flags & FF_DIR && !(buf_dir->flags & (FF_ERR|FF_EXL|FF_OTHFS)))
    fail = dir_scan_recurse(name);
  else if(buf_dir->flags & FF_DIR) {
    if(dir_output.item(buf_dir, name, buf_ext) || dir_output.item(NULL, 0, NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  } else if(dir_output.item(buf_dir, name, buf_ext)) {
    dir_seterr("Output error: %s", strerror(errno));
    fail = 1;
  }

  return fail || input_handle(1);
}


/* Walks through the directory that we're currently chdir'ed to. *dir contains
 * the filenames as returned by dir_read(), and will be freed automatically by
 * this function. */
static int dir_walk(char *dir) {
  int fail = 0;
  char *cur;

  fail = 0;
  for(cur=dir; !fail&&cur&&*cur; cur+=strlen(cur)+1) {
    dir_curpath_enter(cur);
    memset(buf_dir, 0, offsetof(struct dir, name));
    memset(buf_ext, 0, sizeof(struct dir_ext));
    fail = dir_scan_item(cur);
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

  memset(buf_dir, 0, offsetof(struct dir, name));
  memset(buf_ext, 0, sizeof(struct dir_ext));

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
    if(fail)
      buf_dir->flags |= FF_ERR;
    stat_to_dir(&fs);

    if(dir_output.item(buf_dir, dir_curpath, buf_ext)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
    if(!fail)
      fail = dir_walk(dir);
    if(!fail && dir_output.item(NULL, 0, NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      fail = 1;
    }
  }

  while(dir_fatalerr && !input_handle(0))
    ;
  return dir_output.final(dir_fatalerr || fail);
}


void dir_scan_init(const char *path) {
  dir_curpath_set(path);
  dir_setlasterr(NULL);
  dir_seterr(NULL);
  dir_process = process;
  buf_dir = malloc(dir_memsize(""));
  pstate = ST_CALC;
}
