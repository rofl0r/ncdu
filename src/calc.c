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
#include "calc.h"
#include "exclude.h"
#include "util.h"

struct state_calc stcalc;



/* My own implementation of realpath()
    - assumes that *every* possible path fits in PATH_MAX bytes
    - does not set errno on error
    - has not yet been fully tested
*/
char *rpath(const char *from, char *to) {
  char tmp[PATH_MAX], cwd[PATH_MAX], cur[PATH_MAX], app[PATH_MAX];
  int i, j, l, k, last, ll = 0;
  struct stat st;

  getcwd(cwd, PATH_MAX);
  strcpy(cur, from);
  app[0] = 0;

  loop:
  /* not an absolute path, add current directory */
  if(cur[0] != '/') {
    if(!(cwd[0] == '/' && cwd[1] == 0))
      strcpy(tmp, cwd);
    else
      tmp[0] = 0;
    if(strlen(cur) + 2 > PATH_MAX - strlen(tmp))
      return(NULL);
    strcat(tmp, "/");
    strcat(tmp, cur);
  } else
    strcpy(tmp, cur);

  /* now fix things like '.' and '..' */
  i = j = last = 0;
  l = strlen(tmp);
  while(1) {
    if(tmp[i] == 0)
      break;
    /* . */
    if(l >= i+2 && tmp[i] == '/' && tmp[i+1] == '.' && (tmp[i+2] == 0 || tmp[i+2] == '/')) {
      i+= 2;
      continue;
    }
    /* .. */
    if(l >= i+3 && tmp[i] == '/' && tmp[i+1] == '.' && tmp[i+2] == '.' && (tmp[i+3] == 0 || tmp[i+3] == '/')) {
      for(k=j; --k>0;)
        if(to[k] == '/' && k != j-1)
          break;
      j -= j-k;
      if(j < 1) j = 1;
      i += 3;
      continue;
    }
    /* remove double slashes */
    if(tmp[i] == '/' && i>0 && tmp[i-1] == '/') {
      i++;
      continue;
    }
    to[j++] = tmp[i++];
  }
  /* remove leading slashes */
  while(--j > 0) {
    if(to[j] != '/')
      break;
  }
  to[j+1] = 0;
  /* make sure we do have something left in case our path is / */
  if(to[0] == 0) {
    to[0] = '/';
    to[1] = 0;
  }
  /* append 'app' */
  if(app[0] != 0)
    strcat(to, app);

  j = strlen(to);
  /* check for symlinks */
  for(i=1; i<=j; i++) {
    if(to[i] == '/' || to[i] == 0) {
      strncpy(tmp, to, i);
      tmp[i] = 0;
      if(lstat(tmp, &st) < 0)
        return(NULL);
      if(S_ISLNK(st.st_mode)) {
        if(++ll > LINK_MAX || (k = readlink(tmp, cur, PATH_MAX)) < 0)
          return(NULL);
        cur[k] = 0;
        if(to[i] != 0)
          strcpy(app, &to[i]);
        strcpy(cwd, tmp);
        for(k=strlen(cwd); --k>0;)
          if(cwd[k] == '/')
            break;
        cwd[k] = 0;
        goto loop;
      }
      if(!S_ISDIR(st.st_mode))
        return(NULL);
    }
  }

  return(to);
}


int calc_item(struct dir *par, char *path, char *name) {
  char tmp[PATH_MAX];
  struct dir *t, *d;
  struct stat fs;

  if(name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
    return 0;

  /* path too long - ignore file */
  if(strlen(path)+strlen(name)+1 > PATH_MAX)
    return 1;

  /* allocate dir and fix references */
  d = calloc(sizeof(struct dir), 1);
  d->parent = par;
  if(par->sub == NULL)
    par->sub = d;
  else {
    for(t=par->sub; t->next!=NULL; t=t->next)
      ;
    t->next = d;
  }
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
  strcpy(tmp, path);
  strcat(tmp, name);
  if(lstat(tmp, &fs)) {
    d->flags |= FF_ERR;
    return 0;
  }

  /* check for excludes and same filesystem */
  if(exclude_match(tmp))
    d->flags |= FF_EXL;

  if(sflags & SF_SMFS && stcalc.curdev != fs.st_dev)
    d->flags |= FF_OTHFS;

  /* determine type of this item */
  if(S_ISREG(fs.st_mode))
    d->flags |= FF_FILE;
  else if(S_ISDIR(fs.st_mode))
    d->flags |= FF_DIR;

  /* update parent dirs */
  if(!(d->flags & FF_EXL))
    for(t=d; t!=NULL; t=t->parent)
      t->items++;

  /* count the size */
  if(!(d->flags & FF_EXL || d->flags & FF_OTHFS)) {
    d->size = fs.st_blocks * S_BLKSIZE;
    d->asize = fs.st_size;
    for(t=d; t!=NULL; t=t->parent) {
      t->size += d->size;
      t->asize += d->asize;
    }
  }
  return 0;
}


/* recursively walk through the directory tree */
int calc_dir(struct dir *dest, char *path) {
  struct dir *t;
  DIR *dir;
  struct dirent *dr;
  char tmp[PATH_MAX];
  int len;

  if(input_handle(1))
    return 1;

  /* open directory */
  if((dir = opendir(path)) == NULL) {
    strcpy(stcalc.lasterr, path);
    dest->flags |= FF_ERR;
    t = dest;
    while((t = t->parent) != NULL)
      t->flags |= FF_SERR;
    return 0;
  }

  /* add leading slash */
  len = strlen(path);
  if(path[len-1] != '/') {
    path[len] = '/';
    path[++len] = '\0';
  }

  /* read directory */
  while((dr = readdir(dir)) != NULL) {
    if(calc_item(dest, path, dr->d_name))
      dest->flags |= FF_ERR;
    if(input_handle(1))
      return 1;
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
  for(t=dest->sub; t!=NULL; t=t->next)
    if(t->flags & FF_DIR && !(t->flags & FF_EXL || t->flags & FF_OTHFS)) {
      strcpy(tmp, path);
      strcat(tmp, t->name);
      if(calc_dir(t, tmp))
        return 1;
    }

  return 0;
}


void calc_draw_progress() {
  static char antext[15] = "Calculating...";
  char ani[15];
  int i;

  nccreate(10, 60, dat == NULL ? "Calculating..." : "Recalculating...");

  ncprint(2, 2, "Total items: %-8d size: %s",
    stcalc.parent->items, formatsize(stcalc.parent->size, sflags & SF_SI));
  ncprint(3, 2, "Current dir: %s", cropstr(stcalc.cur, 43));
  ncaddstr(8, 43, "Press q to quit");

  /* show warning if we couldn't open a dir */
  if(stcalc.lasterr[0] != '\0') {
     attron(A_BOLD);
     ncaddstr(5, 2, "Warning:");
     attroff(A_BOLD);
     ncprint(5, 11, "could not open %-32s", cropstr(stcalc.lasterr, 32));
     ncaddstr(6, 3, "some directory sizes may not be correct");
  }

  /* animation - but only if the screen refreshes more than or once every second */
  if(sdelay <= 1000) {
    if(++stcalc.anpos == 28)
       stcalc.anpos = 0;
    strcpy(ani, "              ");
    if(stcalc.anpos < 14)
      for(i=0; i<=stcalc.anpos; i++)
        ani[i] = antext[i];
    else
      for(i=13; i>stcalc.anpos-14; i--)
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


int calc_draw() {
  struct timeval tv;

  if(stcalc.err) {
    calc_draw_error(stcalc.cur, stcalc.errmsg);
    return 0;
  }

  /* should we really draw the screen again? */
  gettimeofday(&tv, (void *)NULL);
  tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / sdelay;
  if(stcalc.lastupdate != tv.tv_usec) {
    calc_draw_progress();
    stcalc.lastupdate = tv.tv_usec;
    return 0;
  }
  return 1;
}


int calc_key(int ch) {
  if(stcalc.err)
    return 1;
  if(ch == 'q')
    return 1;
  return 0;
}


void calc_process() {
  char tmp[PATH_MAX];
  struct stat fs;
  struct dir *t;

  /* init/reset global vars */
  stcalc.err = 0;
  stcalc.lastupdate = 999;
  stcalc.lasterr[0] = 0;
  stcalc.anpos = 0;

  /* check root directory */
  if(rpath(stcalc.cur, tmp) == NULL || lstat(tmp, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
    stcalc.err = 1;
    strcpy(stcalc.errmsg, "Directory not found");
    goto fail;
  }

  /* initialize parent dir */
  t = (struct dir *) calloc(1, sizeof(struct dir));
  t->size = fs.st_blocks * S_BLKSIZE;
  t->asize = fs.st_size;
  t->flags |= FF_DIR;
  t->name = (char *) malloc(strlen(tmp)+1);
  strcpy(t->name, tmp);
  stcalc.parent = t;
  stcalc.curdev = fs.st_dev;

  /* start calculating */
  if(!calc_dir(stcalc.parent, tmp) && !stcalc.err) {
    pstate = ST_BROWSE;
    return;
  }

  /* something went wrong... */
  freedir(stcalc.parent);
fail:
  while(stcalc.err && !input_handle(0))
    ;
  pstate = stcalc.sterr;
  return;
}

