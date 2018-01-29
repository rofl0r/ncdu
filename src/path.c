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

#include "path.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#ifndef LINK_MAX
# ifdef _POSIX_LINK_MAX
#  define LINK_MAX _POSIX_LINK_MAX
# else
#  define LINK_MAX 32
# endif
#endif


#define RPATH_CNKSZ 256


/* splits a path into components and does a bit of cannonicalization.
  a pointer to a reversed array of components is stored in res and the
  number of components is returned.
  cur is modified, and res has to be free()d after use */
static int path_split(char *cur, char ***res) {
  char **old;
  int i, j, n;

  cur += strspn(cur, "/");
  n = strlen(cur);

  /* replace slashes with zeros */
  for(i=j=0; i<n; i++)
    if(cur[i] == '/') {
      cur[i] = 0;
      if(cur[i-1] != 0)
        j++;
    }

  /* create array of the components */
  old = malloc((j+1)*sizeof(char *));
  *res = malloc((j+1)*sizeof(char *));
  for(i=j=0; i<n; i++)
    if(i == 0 || (cur[i-1] == 0 && cur[i] != 0))
      old[j++] = cur+i;

  /* re-order and remove parts */
  for(i=n=0; --j>=0; ) {
    if(!strcmp(old[j], "..")) {
      n++;
      continue;
    }
    if(!strcmp(old[j], "."))
      continue;
    if(n) {
      n--;
      continue;
    }
    (*res)[i++] = old[j];
  }
  free(old);
  return i;
}


/* copies path and prepends cwd if needed, to ensure an absolute path
   return value has to be free()'d manually */
static char *path_absolute(const char *path) {
  int i, n;
  char *ret;

  /* not an absolute path? prepend cwd */
  if(path[0] != '/') {
    n = RPATH_CNKSZ;
    ret = malloc(n);
    errno = 0;
    while(!getcwd(ret, n) && errno == ERANGE) {
      n += RPATH_CNKSZ;
      ret = realloc(ret, n);
      errno = 0;
    }
    if(errno) {
      free(ret);
      return NULL;
    }

    i = strlen(path) + strlen(ret) + 2;
    if(i > n)
      ret = realloc(ret, i);
    strcat(ret, "/");
    strcat(ret, path);
  /* otherwise, just make a copy */
  } else {
    ret = malloc(strlen(path)+1);
    strcpy(ret, path);
  }
  return ret;
}


/* NOTE: cwd and the memory cur points to are unreliable after calling this
 * function.
 * TODO: This code is rather fragile and inefficient. A rewrite is in order.
 */
static char *path_real_rec(char *cur, int *links) {
  int i, n, tmpl, lnkl = 0;
  char **arr, *tmp, *lnk = NULL, *ret = NULL;

  tmpl = strlen(cur)+1;
  tmp = malloc(tmpl);

  /* split path */
  i = path_split(cur, &arr);

  /* re-create full path */
  strcpy(tmp, "/");
  if(i > 0) {
    lnkl = RPATH_CNKSZ;
    lnk = malloc(lnkl);
    if(chdir("/") < 0)
      goto path_real_done;
  }

  while(--i>=0) {
    if(arr[i][0] == 0)
      continue;
    /* check for symlink */
    while((n = readlink(arr[i], lnk, lnkl)) == lnkl || (n < 0 && errno == ERANGE)) {
      lnkl += RPATH_CNKSZ;
      lnk = realloc(lnk, lnkl);
    }
    if(n < 0 && errno != EINVAL)
      goto path_real_done;
    if(n > 0) {
      if(++*links > LINK_MAX) {
        errno = ELOOP;
        goto path_real_done;
      }
      lnk[n++] = 0;
      /* create new path */
      if(lnk[0] != '/')
        n += strlen(tmp);
      if(tmpl < n) {
        tmpl = n;
        tmp = realloc(tmp, tmpl);
      }
      if(lnk[0] != '/')
        strcat(tmp, lnk);
      else
        strcpy(tmp, lnk);
      /* append remaining directories */
      while(--i>=0) {
        n += strlen(arr[i])+1;
        if(tmpl < n) {
          tmpl = n;
          tmp = realloc(tmp, tmpl);
        }
        strcat(tmp, "/");
        strcat(tmp, arr[i]);
      }
      /* call path_real_rec() with the new path */
      ret = path_real_rec(tmp, links);
      goto path_real_done;
    }
    /* not a symlink, append component and go to the next part */
    strcat(tmp, arr[i]);
    if(i) {
      if(chdir(arr[i]) < 0)
        goto path_real_done;
      strcat(tmp, "/");
    }
  }
  ret = tmp;

path_real_done:
  if(ret != tmp)
    free(tmp);
  if(lnkl > 0)
    free(lnk);
  free(arr);
  return ret;
}


char *path_real(const char *orig) {
  int links = 0;
  char *tmp, *ret;

  if(orig == NULL)
    return NULL;

  if((tmp = path_absolute(orig)) == NULL)
    return NULL;
  ret = path_real_rec(tmp, &links);
  free(tmp);
  return ret;
}


int path_chdir(const char *path) {
  char **arr, *cur;
  int i, r = -1;

  if((cur = path_absolute(path)) == NULL)
    return -1;

  i = path_split(cur, &arr);
  if(chdir("/") < 0)
    goto path_chdir_done;
  while(--i >= 0)
    if(chdir(arr[i]) < 0)
      goto path_chdir_done;
  r = 0;

path_chdir_done:
  free(cur);
  free(arr);
  return r;
}

