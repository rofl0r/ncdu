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


struct exclude {
  char *pattern;
  struct exclude *next;
};

struct exclude *excludes = NULL,
               *last = NULL;



void addExclude(char *pat) {
  struct exclude *n;

  n = (struct exclude *) malloc(sizeof(struct exclude));
  n->pattern = (char *) malloc(strlen(pat)+1);
  strcpy(n->pattern, pat);
  n->next = NULL;

  if(excludes == NULL) {
    excludes = n;
    last = excludes;
  } else {
    last->next = n;
    last = last->next;
  }
}


int addExcludeFile(char *file) {
  FILE *f;
  char buf[256];
  int len;

  if((f = fopen(file, "r")) == NULL)
    return(1);

  while(fgets(buf, 256, f) != NULL) {
    len = strlen(buf)-1;
    while(len >=0 && (buf[len] == '\r' || buf[len] == '\n'))
      buf[len--] = '\0';
    if(len < 0)
      continue;
    addExclude(buf);
  }

  fclose(f);
  return(0);
}


int matchExclude(char *path) {
  struct exclude *n = excludes;
  char *c;
  int matched = 0;

  if(excludes == NULL)
    return(0);
  
  do {
    matched = !fnmatch(n->pattern, path, 0);
    for(c = path; *c && !matched; c++)
      if(*c == '/' && c[1] != '/')
        matched = !fnmatch(n->pattern, c+1, 0);
  } while((n = n->next) != NULL && !matched);

  return(matched);
}

