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
#include "exclude.h"
#include "util.h"
#include "calc.h"
#include "delete.h"
#include "browser.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>

int pstate;

int min_rows = 17,
    min_cols = 60;

void screen_draw() {
  int n = 1;
  switch(pstate) {
    case ST_CALC:   n = calc_draw();   break;
    case ST_BROWSE: n = browse_draw(); break;
    case ST_HELP:   n = help_draw();   break;
    case ST_DEL:    n = delete_draw(); break;
  }
  if(!n)
    refresh();
}


int input_handle(int wait) {
  int ch;

  nodelay(stdscr, wait);
  screen_draw();
  while((ch = getch()) != ERR) {
    if(ch == KEY_RESIZE) {
      if(ncresize(min_rows, min_cols))
        min_rows = min_cols = 0;
      screen_draw();
      continue;
    }
    switch(pstate) {
      case ST_CALC:   return calc_key(ch);
      case ST_BROWSE: return browse_key(ch);
      case ST_HELP:   return help_key(ch);
      case ST_DEL:    return delete_key(ch);
    }
    screen_draw();
  }
  return 0;
}


/* parse command line */
void argv_parse(int argc, char **argv, char *dir) {
  int i, j, len;

 /* load defaults */
  memset(dir, 0, PATH_MAX);
  getcwd(dir, PATH_MAX);

 /* read from commandline */
  for(i=1; i<argc; i++) {
    if(argv[i][0] == '-') {
     /* flags requiring arguments */
      if(argv[i][1] == 'X' || !strcmp(argv[i], "--exclude-from") || !strcmp(argv[i], "--exclude")
          || argv[i][1] == 'e' || argv[i][1] == 'l') {
        if(i+1 >= argc) {
          printf("Option %s requires an argument\n", argv[i]);
          exit(1);
        }
        else if(strcmp(argv[i], "--exclude") == 0)
          exclude_add(argv[++i]);
        else if(exclude_addfile(argv[++i])) {
          printf("Can't open %s: %s\n", argv[i], strerror(errno));
          exit(1);
        }
        continue;
      }
     /* small flags */
      len = strlen(argv[i]);
      for(j=1; j<len; j++)
        switch(argv[i][j]) {
          case 'x': calc_smfs = 1; break;
          case 'q': calc_delay = delete_delay = 2000;     break;
          case '?':
          case 'h':
            printf("ncdu [-hqvx] [--exclude PATTERN] [-X FILE] directory\n\n");
            printf("  -h                         This help message\n");
            printf("  -q                         Quiet mode, refresh interval 2 seconds\n");
            printf("  -v                         Print version\n");
            printf("  -x                         Same filesystem\n");
            printf("  --exclude PATTERN          Exclude files that match PATTERN\n");
            printf("  -X, --exclude-from FILE    Exclude files that match any pattern in FILE\n");
            exit(0);
          case 'v':
            printf("ncdu %s\n", PACKAGE_VERSION);
            exit(0);  
          default:
            printf("Unknown option: -%c\nSee '%s -h' for more information.\n", argv[i][j], argv[0]);
            exit(1);
        }
    } else {
      strncpy(dir, argv[i], PATH_MAX);
      if(dir[PATH_MAX - 1] != 0) {
        printf("Error: path length exceeds PATH_MAX\n");
        exit(1);
      }
      dir[PATH_MAX - 1] = 0;
    }
  }
}


/* main program */
int main(int argc, char **argv) {
  char dir[PATH_MAX];
  argv_parse(argc, argv, dir);

  calc_init(dir, NULL);

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;

  while(pstate != ST_QUIT) {
    if(pstate == ST_CALC)
      calc_process();
    if(pstate == ST_DEL)
      delete_process();
    else if(input_handle(0))
      break;
  }

  erase();
  refresh();
  endwin();
  exclude_clear();

  return 0;
}

