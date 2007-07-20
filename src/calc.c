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


WINDOW* calcWin() {
  WINDOW *calc;
  calc = newwin(10, 60, winrows/2 - 5, wincols/2 - 30);
  keypad(calc, TRUE);
  box(calc, 0, 0);
  wattron(calc, A_BOLD);
  mvwaddstr(calc, 0, 4, "Calculating...");
  wattroff(calc, A_BOLD);
  mvwaddstr(calc, 2, 2, "Total files:");
  mvwaddstr(calc, 2, 24, "dirs:");
  mvwaddstr(calc, 2, 39, "size:");
  mvwaddstr(calc, 3, 2, "Current dir:");
  mvwaddstr(calc, 8, 43, "Press q to quit");
  return(calc);
}

int calcUsage() {
  WINDOW *calc;
  DIR *dir;
  char antext[15] = "Calculating...";
  int ch, anpos = 0, level = 0, i, cdir1len;
  char cdir[PATH_MAX], emsg[PATH_MAX], tmp[PATH_MAX], err = 0, *f,
       *cdir1, direrr, staterr;
  dev_t dev = (dev_t) NULL;
  struct dirent *dr;
  struct stat fs;
  struct dir *d, *dirs[512]; /* 512 recursive directories should be enough for everyone! */
#ifdef HAVE_GETTIMEOFDAY
  struct timeval tv; suseconds_t l;
  gettimeofday(&tv, (void *)NULL);
  l = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / sdelay - 1;
#else
  time_t tv; time_t l;
  l = time(NULL) - 1;
#endif

  calc = calcWin();
  wrefresh(calc);
  
  memset(dirs, 0, sizeof(struct dir *)*512);
  level = 0;
  dirs[level] = &dat;
  memset(&dat, 0, sizeof(dat));

  if(rpath(sdir, tmp) == NULL || stat(tmp, &fs) != 0 || !S_ISDIR(fs.st_mode)) {
    mvwaddstr(calc, 8, 1, "                                                         ");
    wattron(calc, A_BOLD);
    mvwaddstr(calc, 5, 2, "Error:");
    wattroff(calc, A_BOLD);
    mvwprintw(calc, 5, 9, "could not open %s", cropdir(tmp, 34));
    mvwaddstr(calc, 6, 3, "press any key to continue...");
    wrefresh(calc);
    wgetch(calc);
    delwin(calc);
    return(1);
  }
  if(sflags & SF_AS) dat.size = fs.st_size;
  else               dat.size = fs.st_blocks * 512;
  if(sflags & SF_SMFS) dev = fs.st_dev;
  dat.name = malloc(strlen(tmp)+1);
  strcpy(dat.name, tmp);

  nodelay(calc, 1);
 /* main loop */
  while((ch = wgetch(calc)) != 'q') {
    direrr = staterr = 0;
    cdir1 = cdir;

    if(ch == KEY_RESIZE) {
      delwin(calc);
      ncresize();
      calc = calcWin();
      nodelay(calc, 1);
      erase();
      refresh();
    }

    /* calculate full path of the dir */
    cdir[0] = '\0';
    for(i=0; i<=level; i++) {
      if(i > 0 && !(i == 1 && dat.name[strlen(dat.name)-1] == '/')) strcat(cdir, "/");
      strcat(cdir, dirs[i]->name);
    }
    /* avoid lstat("//name", .) -- Linux:OK, Cygwin:UNC path, POSIX:Implementation-defined */
    if(cdir[0] == '/' && cdir[1] == '\0')
      cdir1++;
    cdir1len = strlen(cdir1);
    /* opendir */
    if((dir = opendir(cdir)) == NULL) {
      dirs[level]->flags |= FF_ERR;
      for(i=level; i-->0;)
        dirs[i]->flags |= FF_SERR;
      err = 1;
      strcpy(emsg, cdir);
      dirs[++level] = NULL;
      goto showstatus;
    }
    dirs[++level] = NULL;
    /* readdir */
    errno = 0;
    while((dr = readdir(dir)) != NULL) {
      int namelen;
      f = dr->d_name;
      if(f[0] == '.' && (f[1] == '\0' || (f[1] == '.' && f[2] == '\0')))
        continue;
      namelen = strlen(f);
      if(cdir1len+namelen+1 >= PATH_MAX) {
        direrr = 1;
        errno = 0;
        continue;
      }
      d = calloc(sizeof(struct dir), 1);
      d->name = malloc(namelen+1);
      strcpy(d->name, f);
      if(dirs[level] != NULL) dirs[level]->next = d;
      d->prev = dirs[level];
      d->parent = dirs[level-1];
      dirs[level-1]->sub = d;
      dirs[level] = d;
      sprintf(tmp, "%s/%s", cdir1, d->name);
      if(lstat(tmp, &fs)) {
        staterr = 1;
        d->flags = FF_ERR;
        errno = 0;
        continue;
      }
     /* check filetype */
      if(sflags & SF_SMFS && dev != fs.st_dev)
        d->flags |= FF_OTHFS;
      if(S_ISREG(fs.st_mode)) {
        d->flags |= FF_FILE;
        for(i=level; i-->0;)
          dirs[i]->files++;
      } else if(S_ISDIR(fs.st_mode)) {
        d->flags |= FF_DIR;
        for(i=level; i-->0;) 
          dirs[i]->dirs++;
      } else
        d->flags |= FF_OTHER;
     /* count size */
      if(sflags & SF_AS)
        d->size = fs.st_size;
      else
        d->size = fs.st_blocks * 512;
      if(d->flags & FF_OTHFS) d->size = 0;
      for(i=level; i-->0;)
        dirs[i]->size += d->size;
      errno = 0;
    }
    if(errno)
      direrr = 1;
    closedir(dir);
    if(direrr || staterr) {
      dirs[level-1]->flags |= (direrr ? FF_ERR : FF_SERR);
      for(i=level-1; i-->0;)
        dirs[i]->flags |= FF_SERR;
    }

   /* show progress status */
    showstatus:
#ifdef HAVE_GETTIMEOFDAY
    gettimeofday(&tv, (void *)NULL);
    tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / sdelay;
    if(l == tv.tv_usec) goto newdir;
#else
    time(&tv);
    if(l == tv) goto newdir;
#endif
    mvwprintw(calc, 3, 15, "%-43s", cropdir(cdir, 43));
    mvwprintw(calc, 2, 15, "%d", dat.files);
    mvwprintw(calc, 2, 30, "%d", dat.dirs);
    mvwaddstr(calc, 2, 45, cropsize(dat.size));

    if(err == 1) {
      wattron(calc, A_BOLD);
      mvwaddstr(calc, 5, 2, "Warning:");
      wattroff(calc, A_BOLD);
      mvwprintw(calc, 5, 11, "could not open %-32s", cropdir(emsg, 32));
      mvwaddstr(calc, 6, 3, "some directory sizes may not be correct");
    }

    /* animation */
    if(sdelay < 1000) {
      if(++anpos == 28) anpos = 0;
      mvwaddstr(calc, 8, 3, "              ");
      if(anpos < 14)
        for(i=0; i<=anpos; i++)
          mvwaddch(calc, 8, i+3, antext[i]);
      else
        for(i=13; i>anpos-14; i--)
          mvwaddch(calc, 8, i+3, antext[i]);
    } else
      mvwaddstr(calc, 8, 3, antext);
    wmove(calc, 8, 58);
    wrefresh(calc);

   /* select new directory */
    newdir:
#ifdef HAVE_GETTIMEOFDAY
    l = tv.tv_usec;
#else
    l = tv;
#endif
    while(dirs[level] == NULL || !(dirs[level]->flags & FF_DIR) || dirs[level]->flags & FF_OTHFS) {
      if(dirs[level] != NULL && dirs[level]->prev != NULL)
        dirs[level] = dirs[level]->prev;
      else {
        while(level-- > 0) {
          if(dirs[level]->prev != NULL) {
            dirs[level] = dirs[level]->prev;
            break;
          }
        }
        if(level < 1) goto endloop;
      }
    }
  }
  endloop:
  nodelay(calc, 0);
  delwin(calc);
  erase();
  refresh();
  if(ch == 'q')
    return(2);
  return(0);
}

