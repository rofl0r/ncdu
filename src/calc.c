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


/* external vars */
char calc_smfs  = 0;

/* global vars for internal use */
char failed;             /* 1 on fatal error */
char *curpath;           /* last lstat()'ed item, used for excludes matching and display */
char *lasterr;           /* last unreadable dir/item */
char errmsg[128];        /* error message, when failed=1 */
struct dir *root;        /* root directory struct we're calculating */
struct dir *orig;        /* original directory, when recalculating */
dev_t curdev;            /* current device we're calculating on */
int anpos;               /* position of the animation string */
struct link_inode {      /* list of all non-dirs with nlink > 1 */
  dev_t dev;
  ino_t ino;
} *links = NULL;
int curpathl = 0, lasterrl = 0, linksl = 0, linkst = 0;



/* adds name to curpath */
void calc_enterpath(char *name) {
  int n;

  n = strlen(curpath)+strlen(name)+2;
  if(n > curpathl) {
    curpathl = n;
    curpath = realloc(curpath, curpathl);
  }
  if(curpath[1])
    strcat(curpath, "/");
  strcat(curpath, name);
}


/* removes last component from curpath */
void calc_leavepath() {
  char *tmp;
  if((tmp = strrchr(curpath, '/')) == NULL)
    strcpy(curpath, "/");
  else if(tmp != curpath)
    tmp[0] = 0;
  else
    tmp[1] = 0;
}


int calc_item(struct dir *par, char *name) {
  struct dir *t, *d;
  struct stat fs;
  int i;

  if(name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
    return 0;

  /* allocate dir and fix references */
  d = calloc(sizeof(struct dir), 1);
  d->parent = par;
  d->next = par->sub;
  par->sub = d;
  d->name = malloc(strlen(name)+1);
  strcpy(d->name, name);

#ifdef __CYGWIN__
  /* /proc/registry names may contain slashes */
  if(strchr(d->name, '/') || strchr(d->name,  '\\')) {
    d->flags |= FF_ERR;
    return 0;
  }
#endif

  /* lstat */
  if(lstat(name, &fs)) {
    d->flags |= FF_ERR;
    return 0;
  }

  /* check for excludes and same filesystem */
  if(exclude_match(curpath))
    d->flags |= FF_EXL;

  if(calc_smfs && curdev != fs.st_dev)
    d->flags |= FF_OTHFS;

  /* determine type of this item */
  if(S_ISREG(fs.st_mode))
    d->flags |= FF_FILE;
  else if(S_ISDIR(fs.st_mode))
    d->flags |= FF_DIR;

  /* update parent dirs */
  if(!(d->flags & FF_EXL))
    for(t=d->parent; t!=NULL; t=t->parent)
      t->items++;

  /* check for hard links.
     An item is only considered a hard link if it's not a directory,
     has st_nlink > 1, and is already present in the links array */
  if(!S_ISDIR(fs.st_mode) && fs.st_nlink > 1) {
    for(i=0; i<linkst; i++)
      if(links[i].dev == fs.st_dev && links[i].ino == fs.st_ino)
        break;
    /* found in the list, set link flag (so the size won't get counted) */
    if(i != linkst)
      d->flags |= FF_HLNK;
    /* not found, add to the list */
    else {
      if(++linkst > linksl) {
        linksl *= 2;
        if(!linksl) {
          linksl = 64;
          links = malloc(linksl);
        } else
          links = realloc(links, linksl);
      }
      links[i].dev = fs.st_dev;
      links[i].ino = fs.st_ino;
    }
  }

  /* count the size */
  if(!(d->flags & FF_EXL || d->flags & FF_OTHFS || d->flags & FF_HLNK)) {
    d->size = fs.st_blocks * S_BLKSIZE;
    d->asize = fs.st_size;
    for(t=d->parent; t!=NULL; t=t->parent) {
      t->size += d->size;
      t->asize += d->asize;
    }
  }

  return 0;
}


/* recursively walk through the directory tree,
   assumes path resides in the cwd */
int calc_dir(struct dir *dest, char *name) {
  struct dir *t;
  DIR *dir;
  struct dirent *dr;
  int ch;

  if(input_handle(1))
    return 1;

  /* curpath */
  calc_enterpath(name);

  /* open & chdir into directory */
  if((dir = opendir(name)) == NULL || chdir(name) < 0) {
    if(lasterrl < (int)strlen(curpath)) {
      lasterrl = strlen(curpath)+1;
      lasterr = realloc(lasterr, lasterrl);
    }
    strcpy(lasterr, curpath);
    dest->flags |= FF_ERR;
    t = dest;
    while((t = t->parent) != NULL)
      t->flags |= FF_SERR;
    calc_leavepath();
    return 0;
  }

  /* read directory */
  while((dr = readdir(dir)) != NULL) {
    calc_enterpath(dr->d_name);
    if(calc_item(dest, dr->d_name))
      dest->flags |= FF_ERR;
    if(input_handle(1)) {
      calc_leavepath();
      return 1;
    }
    calc_leavepath();
    errno = 0;
  }

  if(errno) {
    if(dest->flags & FF_SERR)
      dest->flags -= FF_SERR;
    dest->flags |= FF_ERR;
  }
  closedir(dir);

  /* error occured while reading this dir, update parent dirs */
  for(t=dest->sub; t!=NULL; t=t->next)
    if(t->flags & FF_ERR || t->flags & FF_SERR)
      dest->flags |= FF_SERR;
  if(dest->flags & FF_ERR || dest->flags & FF_SERR) {
    for(t = dest; (t = t->parent) != NULL; )
      t->flags |= FF_SERR;
  }

  /* calculate subdirectories */
  ch = 0;
  for(t=dest->sub; t!=NULL; t=t->next)
    if(t->flags & FF_DIR && !(t->flags & FF_EXL || t->flags & FF_OTHFS))
      if(calc_dir(t, t->name)) {
        calc_leavepath();
        return 1;
      }

  /* chdir back */
  if(chdir("..") < 0) {
    failed = 1;
    strcpy(errmsg, "Couldn't chdir to previous directory");
    calc_leavepath();
    return 1;
  }
  calc_leavepath();

  return 0;
}


void calc_draw_progress() {
  static char antext[15] = "Calculating...";
  char ani[15];
  int i;

  nccreate(10, 60, !orig ? "Calculating..." : "Recalculating...");

  ncprint(2, 2, "Total items: %-8d size: %s",
    root->items, formatsize(root->size));
  ncprint(3, 2, "Current dir: %s", cropstr(curpath, 43));
  ncaddstr(8, 43, "Press q to quit");

  /* show warning if we couldn't open a dir */
  if(lasterr[0] != '\0') {
     attron(A_BOLD);
     ncaddstr(5, 2, "Warning:");
     attroff(A_BOLD);
     ncprint(5, 11, "could not open %-32s", cropstr(lasterr, 32));
     ncaddstr(6, 3, "some directory sizes may not be correct");
  }

  /* animation - but only if the screen refreshes more than or once every second */
  if(update_delay <= 1000) {
    if(++anpos == 28)
       anpos = 0;
    strcpy(ani, "              ");
    if(anpos < 14)
      for(i=0; i<=anpos; i++)
        ani[i] = antext[i];
    else
      for(i=13; i>anpos-14; i--)
        ani[i] = antext[i];
  } else
    strcpy(ani, antext);
  ncaddstr(8, 3, ani);
}


void calc_draw_error(char *cur, char *msg) {
  nccreate(7, 60, "Error!");

  attron(A_BOLD);
  ncaddstr(2, 2, "Error:");
  attroff(A_BOLD);

  ncprint(2, 9, "could not open %s", cropstr(cur, 34));
  ncprint(3, 4, "%s", cropstr(msg, 52));
  ncaddstr(5, 30, "press any key to continue...");
}


void calc_draw() {
  browse_draw();
  if(failed)
    calc_draw_error(curpath, errmsg);
  else
    calc_draw_progress();
}


int calc_key(int ch) {
  if(failed)
    return 1;
  if(ch == 'q')
    return 1;
  return 0;
}


void calc_process() {
  char *path, *name;
  struct stat fs;
  struct dir *t;
  int n;

  /* check root directory */
  if((path = path_real(curpath)) == NULL) {
    failed = 1;
    strcpy(errmsg, "Directory not found");
    goto calc_fail;
  }

  /* split into path and last component */
  name = strrchr(path, '/');
  if(name == path) {
    if(!path[1])
      name = ".";
    else {
      name = malloc(strlen(path));
      strcpy(name, path+1);
      path[1] = 0;
    }
  } else
    *(name++) = 0;

  /* we need to chdir so we can provide relative paths for lstat() and opendir(),
   * this to prevent creating path names longer than PATH_MAX */
  if(path_chdir(path) < 0) {
    failed = 1;
    strcpy(errmsg, "Couldn't chdir into directory");
    free(path);
    goto calc_fail;
  }
  /* would be strange for this to fail, but oh well... */
  if(lstat(name, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
    failed = 1;
    strcpy(errmsg, "Couldn't stat directory");
    free(path);
    goto calc_fail;
  }

  /* initialize parent dir */
  t = (struct dir *) calloc(1, sizeof(struct dir));
  t->size = fs.st_blocks * S_BLKSIZE;
  t->asize = fs.st_size;
  t->flags |= FF_DIR;
  if(orig) {
    t->name = malloc(strlen(orig->name)+1);
    strcpy(t->name, orig->name);
  } else {
    t->name = malloc(strlen(path)+strlen(name)+2);
    t->name[0] = 0;
    if(strcmp(path, "/"))
      strcpy(t->name, path);
    if(strcmp(name, ".")) {
      strcat(t->name, "/");
      strcat(t->name, name);
    }
  }
  root = t;
  curdev = fs.st_dev;

  /* update curpath */
  if(strcmp(name, ".")) {
    if((int)strlen(path)+1 > curpathl) {
      curpathl = strlen(path)+1;
      curpath = realloc(curpath, curpathl);
    }
    strcpy(curpath, path);
  } else
    curpath[0] = 0;

  /* calculate */
  n = calc_dir(root, name);

  /* free some resources */
  if(!path[1] && strcmp(name, "."))
    free(name);
  free(path);
  if(linksl) {
    linksl = linkst = 0;
    free(links);
  }

  /* success */
  if(!n && !failed) {
    if(root->sub == NULL) {
      freedir(root);
      failed = 1;
      strcpy(errmsg, "Directory empty.");
      goto calc_fail;
    }
    browse_init(root->sub);

    /* update references and free original item */
    if(orig) {
      root->parent = orig->parent;
      root->next = orig->next;
      for(t=root->parent; t!=NULL; t=t->parent) {
        t->size += root->size;
        t->asize += root->asize;
        t->items += root->items+1;
      }

      if(orig->parent) {
        t = orig->parent->sub;
        if(t == orig)
          orig->parent->sub = root;
        else if(t != NULL)
          for(; t->next!=NULL; t=t->next)
            if(t->next == orig)
              t->next = root;
      }
      freedir(orig);
    }
    return;
  }

  /* something went wrong... */
  freedir(root);
calc_fail:
  while(failed && !input_handle(0))
    ;
  pstate = orig ? ST_BROWSE : ST_QUIT;
  return;
}


void calc_init(char *dir, struct dir *org) {
  failed = anpos = 0;
  orig = org;
  if(curpathl == 0) {
    curpathl = strlen(dir)+1;
    curpath = malloc(curpathl);
  } else if(curpathl < (int)strlen(dir)+1) {
    curpathl = strlen(dir)+1;
    curpath = realloc(curpath, curpathl);
  }
  strcpy(curpath, dir);
  if(lasterrl == 0) {
    lasterrl = 1;
    lasterr = malloc(lasterrl);
  }
  lasterr[0] = 0;
  pstate = ST_CALC;
}

