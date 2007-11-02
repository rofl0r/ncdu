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


/* parent dir we are calculating */
struct dir *parent;
/* current device we are on */
dev_t curdev;
/* path of the last dir we couldn't read */
char lasterr[PATH_MAX];
/* and for the animation... */
int anpos;
char antext[15] = "Calculating...";
suseconds_t lastupdate;



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


/* the progress window */
static void drawProgress(char *cdir) {
  char ani[15];
  int i;

  nccreate(10, 60, dat == NULL ? "Calculating..." : "Recalculating...");

  ncprint(2, 2, "Total items: %-8d size: %s",
    parent->items, cropsize(parent->size));
  ncprint(3, 2, "Current dir: %s", cropdir(cdir, 43));
  ncaddstr(8, 43, "Press q to quit");

 /* show warning if we couldn't open a dir */
  if(lasterr[0] != '\0') {
     attron(A_BOLD);
     ncaddstr(5, 2, "Warning:");
     attroff(A_BOLD);
     ncprint(5, 11, "could not open %-32s", cropdir(lasterr, 32));
     ncaddstr(6, 3, "some directory sizes may not be correct");  
  }

 /* animation - but only if the screen refreshes more than or once every second */
  if(sdelay <= 1000) {
    if(++anpos == 28) anpos = 0;
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

  refresh();
}


/* show error if can't open parent dir */
static void drawError(char *dir) {
  nccreate(10, 60, "Error!");

  attron(A_BOLD);
  ncaddstr(5, 2, "Error:");
  attroff(A_BOLD);

  ncprint(5, 9, "could not open %s", cropdir(dir, 34));
  ncaddstr(6, 3, "press any key to continue...");

  refresh();
}


/* checks for input and calls drawProgress */
int updateProgress(char *path) {
  struct timeval tv;
  int ch;

 /* don't do anything if s_export is set (ncurses isn't initialized) */
  if(s_export)
    return(1);

 /* check for input or screen resizes */
  nodelay(stdscr, 1);
  while((ch = getch()) != ERR) {
    if(ch == 'q')
      return(0);
    if(ch == KEY_RESIZE) {
      ncresize();
      if(dat != NULL)
        drawBrowser(0);
      drawProgress(path);
    }
  }
  nodelay(stdscr, 0);
 
 /* don't update the screen with shorter intervals than sdelay */
  gettimeofday(&tv, (void *)NULL);
  tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / sdelay;
  if(lastupdate != tv.tv_usec) {
    drawProgress(path);
    lastupdate = tv.tv_usec;
  }
  return(1);
}


/* recursive */
int calcDir(struct dir *dest, char *path) {
  struct dir *d, *t, *last;
  struct stat fs;
  DIR *dir;
  struct dirent *dr;
  char *f, tmp[PATH_MAX];
  int len, derr = 0, serr = 0;

  if(!updateProgress(path))
    return(0);

 /* open directory */
  if((dir = opendir(path)) == NULL) {
    strcpy(lasterr, path);
    dest->flags |= FF_ERR;
    t = dest;
    while((t = t->parent) != NULL)
      t->flags |= FF_SERR;
    return(1);
  }
  len = strlen(path);

 /* add leading slash */
  if(path[len-1] != '/') {
    path[len] = '/';
    path[++len] = '\0';
  }
 
 /* read directory */
  last = NULL;
  while((dr = readdir(dir)) != NULL) {
    f = dr->d_name;
    if(f[0] == '.' && (f[1] == '\0' || (f[1] == '.' && f[2] == '\0')))
      continue;

   /* path too long - ignore file */
    if(len+strlen(f)+1 > PATH_MAX) {
      derr = 1;
      errno = 0;
      continue;
    }

   /* allocate dir and fix references */
    d = calloc(sizeof(struct dir), 1);
    d->parent = dest;
    if(dest->sub == NULL)
      dest->sub = d;
    if(last != NULL)
      last->next = d;
    last = d;

   /* set d->name */
    d->name = malloc(strlen(f)+1);
    strcpy(d->name, f);

   /* get full path */
    strcpy(tmp, path);
    strcat(tmp, f);

   /* lstat */
    if(lstat(tmp, &fs)) {
      serr = 1;
      errno = 0;
      d->flags |= FF_ERR;
      continue;
    }

   /* check for excludes and same filesystem */
    if(matchExclude(tmp))
      d->flags |= FF_EXL;

    if(sflags & SF_SMFS && curdev != fs.st_dev)
      d->flags |= FF_OTHFS;

   /* determine type of this item */
    if(S_ISREG(fs.st_mode))
      d->flags |= FF_FILE;
    else if(S_ISDIR(fs.st_mode))
      d->flags |= FF_DIR;

   /* update parent dirs */
    if(!(d->flags & FF_EXL))
      for(t = dest; t != NULL; t = t->parent)
        t->items++;

   /* count the size */
    if(!(d->flags & FF_EXL || d->flags & FF_OTHFS)) {
      d->size = fs.st_blocks * S_BLKSIZE;
      d->asize = fs.st_size;
      for(t = dest; t != NULL; t = t->parent) {
        t->size += d->size;
        t->asize += d->asize;
      }
    }

   /* show status */
    if(!updateProgress(tmp))
      return(0);   

    errno = 0;
  }
  derr = derr || errno;
  closedir(dir);
 
 /* error occured while reading this dir, update parent dirs */
  if(derr || serr) {
    dest->flags |= derr ? FF_ERR : FF_SERR;
    for(t = dest; (t = t->parent) != NULL; )
      t->flags |= FF_SERR;
  }

  if(dest->sub) {
   /* calculate subdirectories */
    for(d = dest->sub; d != NULL; d = d->next)
      if(d->flags & FF_DIR && !(d->flags & FF_EXL || d->flags & FF_OTHFS)) {
        strcpy(tmp, path);
        strcat(tmp, d->name);
        if(!calcDir(d, tmp))
          return(0);
      }
  }

  return(1);
}


struct dir *showCalc(char *path) {
  char tmp[PATH_MAX];
  struct stat fs;
  struct dir *t;

 /* init/reset global vars */
  *lasterr = '\0';
  anpos = 0;
  lastupdate = 999;
  memset(tmp, 0, PATH_MAX);

 /* init parent dir */
  if(rpath(path, tmp) == NULL || lstat(tmp, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
    if(s_export) {
      printf("Error: could not open %s\n", path);
      exit(1);
    }
    do {
      ncresize();
      if(dat != NULL)
        drawBrowser(0);
      drawError(path);
    } while (getch() == KEY_RESIZE);
    return(NULL);
  }
  parent = calloc(sizeof(struct dir), 1);
  parent->size = fs.st_blocks * S_BLKSIZE;
  parent->asize = fs.st_size;
  parent->flags |= FF_DIR;
  curdev = fs.st_dev;
  parent->name = malloc(strlen(tmp)+1);
  strcpy(parent->name, tmp);

 /* start calculating */
  if(!calcDir(parent, tmp)) {
    freedir(parent);
    return(NULL);
  }

  return(parent);
}







