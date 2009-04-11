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

/* check ncdu.h what these are for */
struct dir *dat;
int winrows, wincols;
char sdir[PATH_MAX];
int sflags, bflags, sdelay, bgraph;
int subwinc, subwinr;
int pstate;


void screen_draw() {
  int n = 1;
  switch(pstate) {
    case ST_CALC: n = calc_draw();
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
      ncresize();
      screen_draw();
      continue;
    }
    switch(pstate) {
      case ST_CALC: return calc_key(ch);
    }
  }
  return 0;
}


/* parse command line */
void argv_parse(int argc, char **argv, char *dir) {
  int i, j, len;

 /* load defaults */
  memset(dir, 0, PATH_MAX);
  getcwd(dir, PATH_MAX);
  sflags = 0;
  sdelay = 100;
  bflags = BF_SIZE | BF_DESC;

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
          addExclude(argv[++i]);
        else if(addExcludeFile(argv[++i])) {
          printf("Can't open %s: %s\n", argv[i], strerror(errno));
          exit(1);
        }
        continue;
      }
     /* small flags */
      len = strlen(argv[i]);
      for(j=1; j<len; j++)
        switch(argv[i][j]) {
          case 'x': sflags |= SF_SMFS; break;
          case 'q': sdelay = 2000;     break;
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
  dat = NULL;

  argv_parse(argc, argv, stcalc.cur);
  pstate = ST_CALC;
  stcalc.sterr = ST_QUIT;

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  ncresize();

  while(pstate != ST_QUIT) {
    if(pstate == ST_CALC)
      calc_process();
    /*else
       wait_for_input() */
  }

  erase();
  refresh();
  endwin();

  return 0;
}

