/* ncdu - NCurses Disk Usage 
    
  Copyright (c) 2007-2010 Yoran Heling

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

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>
#include <locale.h>

#ifndef COMPLL_NOLIB
#include <zlib.h>
#endif


int pstate;

int min_rows = 17,
    min_cols = 60;
long update_delay = 100,
     lastupdate = 999;


void screen_draw() {
  switch(pstate) {
    case ST_CALC:   calc_draw();   break;
    case ST_BROWSE: browse_draw(); break;
    case ST_HELP:   help_draw();   break;
    case ST_DEL:    delete_draw(); break;
  }
}


/* wait:
 *  -1: non-blocking, always draw screen
 *   0: blocking wait for input and always draw screen
 *   1: non-blocking, draw screen only if a configured delay has passed or after keypress
 */
int input_handle(int wait) {
  int ch;
  struct timeval tv;

  nodelay(stdscr, wait?1:0);
  if(wait != 1)
    screen_draw();
  else {
    gettimeofday(&tv, (void *)NULL);
    tv.tv_usec = (1000*(tv.tv_sec % 1000) + (tv.tv_usec / 1000)) / update_delay;
    if(lastupdate != tv.tv_usec) {
      screen_draw();
      lastupdate = tv.tv_usec;
    }
  }
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
char *argv_parse(int argc, char **argv) {
  int i, j, len;
  char *dir = NULL;

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
          case 'q': update_delay = 2000;     break;
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
    } else
      dir = argv[i];
  }
  return dir;
}


/* compression and decompression functions for compll */
#ifndef COMPLL_NOLIB

unsigned int block_compress(const unsigned char *src, unsigned int srclen, unsigned char *dst, unsigned int dstlen) {
  uLongf len = dstlen;
  compress2(dst, &len, src, srclen, 6);
  return len;
}

void block_decompress(const unsigned char *src, unsigned int srclen, unsigned char *dst, unsigned int dstlen) {
  uLongf len = dstlen;
  uncompress(dst, &len, src, srclen);
}

#endif


/* main program */
int main(int argc, char **argv) {
  char *dir;

  setlocale(LC_ALL, "");

  if((dir = argv_parse(argc, argv)) == NULL)
    dir = ".";

#ifndef COMPLL_NOLIB
  /* we probably want to make these options configurable */
  compll_init(32*1024, sizeof(off_t), 50, block_compress, block_decompress);
#endif

  calc_init(dir, (compll_t)0);

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;

  while(1) {
    if(pstate == ST_CALC && calc_process())
      break;
    else if(pstate == ST_DEL)
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

