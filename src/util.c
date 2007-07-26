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

char cropsizedat[8];
char cropdirdat[4096];

char *cropdir(const char *from, int s) {
  int i, j, o = strlen(from);
  if(o < s) {
    strcpy(cropdirdat, from);
    return(cropdirdat);
  }
  j=s/2-3;
  for(i=0; i<j; i++)
    cropdirdat[i] = from[i];
  cropdirdat[i] = '.';
  cropdirdat[++i] = '.';
  cropdirdat[++i] = '.';
  j=o-s;
  while(++i<s)
    cropdirdat[i] = from[j+i];
  cropdirdat[s] = '\0';
  return(cropdirdat);
}

/* return value is always xxx.xXB = 8 bytes (including \0) */
char *cropsize(const off_t from) {
  float r = from; 
  char c = ' ';
  if(sflags & SF_SI) {
    if(r < 1000.0f)      { }
    else if(r < 1000e3f) { c = 'k'; r/=1000.0f; }
    else if(r < 1000e6f) { c = 'M'; r/=1000e3f; }
    else if(r < 1000e9f) { c = 'G'; r/=1000e6f; }
    else                 { c = 'T'; r/=1000e9f; }
  } else {
    if(r < 1000.0f)      { }
    else if(r < 1023e3f) { c = 'k'; r/=1024.0f; }
    else if(r < 1023e6f) { c = 'M'; r/=1048576.0f; }
    else if(r < 1023e9f) { c = 'G'; r/=1073741824.0f; }
    else                 { c = 'T'; r/=1099511627776.0f; }
  }
  sprintf(cropsizedat, "%5.1f%cB", r, c);
  return(cropsizedat);
}

void ncresize(void) {
  int ch;
  getmaxyx(stdscr, winrows, wincols);
  while(!(sflags & SF_IGNS) && (winrows < 17 || wincols < 60)) {
    erase();
    mvaddstr(0, 0, "Warning: terminal too small,");
    mvaddstr(1, 1, "please either resize your terminal,");
    mvaddstr(2, 1, "press i to ignore, or press q to quit.");
    touchwin(stdscr);
    refresh();
    nodelay(stdscr, 0);
    ch = getch();
    getmaxyx(stdscr, winrows, wincols);
    if(ch == 'q') {
      erase();
      refresh();
      endwin();
      exit(0); 
    }
    if(ch == 'i')
      sflags |= SF_IGNS;
  }
  erase();
}



void freedir_rec(struct dir *dr) {
  struct dir *tmp, *tmp2;
  tmp2 = dr;
  while((tmp = tmp2) != NULL) {
    if(tmp->sub) freedir_rec(tmp->sub);
    free(tmp->name);
    tmp2 = tmp->next;
    free(tmp);
  }
}

/* remove a file/directory from the in-memory map */
struct dir *freedir(struct dir *dr) {
  struct dir *tmp, *cur;

 /* update sizes of parent directories */
  tmp = dr;
  if(dr->flags & FF_FILE) dr->files++;
  if(dr->flags & FF_DIR) dr->dirs++;
  while((tmp = tmp->parent) != NULL) {
    tmp->size -= dr->size;
    tmp->files -= dr->files;
    tmp->dirs -= dr->dirs;
  }

 /* free dr->sub recursive */
  if(dr->sub) freedir_rec(dr->sub);
 
 /* update references */
  cur = NULL;
  if(dr->parent) {
   /* item is at the top of the dir, refer to next item */
    if(dr->parent->sub == dr) {
      dr->parent->sub = dr->next;
      cur = dr->next;
    }
   /* else, get the previous item and update it's "next"-reference */
    else
      for(tmp = dr->parent->sub; tmp != NULL; tmp = tmp->next)
        if(tmp->next == dr) {
          tmp->next = dr->next;
          cur = tmp;
        }
  }
  if(cur != NULL)
    cur->flags |= FF_BSEL;

  free(dr->name);
  free(dr);

  return(cur);
}

char *getpath(struct dir *cur, char *to) {
  struct dir *d, *list[100];
  int c = 0;

  for(d = cur; (d = d->parent) != NULL; )
    list[c++] = d;

  to[0] = '\0';
  while(c--) {
    if(list[c]->parent && list[c]->name[strlen(list[c]->name)-1] != '/')
      strcat(to, "/");
    strcat(to, list[c]->name);
  }

  return to;
}
