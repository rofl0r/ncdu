/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2012 Yoran Heling

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
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>
#include <locale.h>


int pstate;
int read_only = 0;
long update_delay = 100;

static int min_rows = 17, min_cols = 60;
static int ncurses_init = 0;
static int ncurses_tty = 0; /* Explicitely open /dev/tty instead of using stdio */
static long lastupdate = 999;


static void screen_draw() {
  switch(pstate) {
    case ST_CALC:   dir_draw();    break;
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

  /* No actual input handling is done if ncurses hasn't been initialized yet. */
  if(!ncurses_init)
    return wait == 0 ? 1 : 0;

  nodelay(stdscr, wait?1:0);
  while((ch = getch()) != ERR) {
    if(ch == KEY_RESIZE) {
      if(ncresize(min_rows, min_cols))
        min_rows = min_cols = 0;
      /* ncresize() may change nodelay state, make sure to revert it. */
      nodelay(stdscr, wait?1:0);
      screen_draw();
      continue;
    }
    switch(pstate) {
      case ST_CALC:   return dir_key(ch);
      case ST_BROWSE: return browse_key(ch);
      case ST_HELP:   return help_key(ch);
      case ST_DEL:    return delete_key(ch);
    }
    screen_draw();
  }
  return 0;
}


/* parse command line */
static void argv_parse(int argc, char **argv) {
  int i, j, len;
  char *export = NULL;
  char *import = NULL;
  char *dir = NULL;
  dir_ui = -1;

  /* read from commandline */
  for(i=1; i<argc; i++) {
    if(argv[i][0] == '-') {
      /* flags requiring arguments */
      if(!strcmp(argv[i], "-X") || !strcmp(argv[i], "-o") || !strcmp(argv[i], "-f")
          || !strcmp(argv[i], "--exclude-from") || !strcmp(argv[i], "--exclude")) {
        if(i+1 >= argc) {
          printf("Option %s requires an argument\n", argv[i]);
          exit(1);
        } else if(strcmp(argv[i], "-o") == 0)
          export = argv[++i];
        else if(strcmp(argv[i], "-f") == 0)
          import = argv[++i];
        else if(strcmp(argv[i], "--exclude") == 0)
          exclude_add(argv[++i]);
        else if(exclude_addfile(argv[++i])) {
          printf("Can't open %s: %s\n", argv[i], strerror(errno));
          exit(1);
        }
        continue;
      }
      /* short flags */
      len = strlen(argv[i]);
      for(j=1; j<len; j++)
        switch(argv[i][j]) {
          case '0': dir_ui = 0; break;
          case '1': dir_ui = 1; break;
          case '2': dir_ui = 2; break;
          case 'x': dir_scan_smfs = 1; break;
          case 'r': read_only = 1; break;
          case 'q': update_delay = 2000;     break;
          case '?':
          case 'h':
            printf("ncdu <options> <directory>\n\n");
            printf("  -h                         This help message\n");
            printf("  -q                         Quiet mode, refresh interval 2 seconds\n");
            printf("  -v                         Print version\n");
            printf("  -x                         Same filesystem\n");
            printf("  -r                         Read only\n");
            printf("  -o FILE                    Export scanned directory to FILE\n");
            printf("  -f FILE                    Import scanned directory from FILE\n");
            printf("  -0,-1,-2                   UI to use when scanning (0=none,2=full ncurses)\n");
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

  if(export) {
    if(dir_export_init(export)) {
      printf("Can't open %s: %s\n", export, strerror(errno));
      exit(1);
    }
    if(strcmp(export, "-") == 0)
      ncurses_tty = 1;
  } else
    dir_mem_init(NULL);

  if(import) {
    if(dir_import_init(import)) {
      printf("Can't open %s: %s\n", import, strerror(errno));
      exit(1);
    }
    if(strcmp(import, "-") == 0)
      ncurses_tty = 1;
  } else
    dir_scan_init(dir ? dir : ".");

  /* Use the single-line scan feedback by default when exporting to file, no
   * feedback when exporting to stdout. */
  if(dir_ui == -1)
    dir_ui = export && strcmp(export, "-") == 0 ? 0 : export ? 1 : 2;
}


/* Initializes ncurses only when not done yet. */
static void init_nc() {
  int ok = 0;
  FILE *tty;
  SCREEN *term;

  if(ncurses_init)
    return;
  ncurses_init = 1;

  if(ncurses_tty) {
    tty = fopen("/dev/tty", "r+");
    if(!tty) {
      fprintf(stderr, "Error opening /dev/tty: %s\n", strerror(errno));
      exit(1);
    }
    term = newterm(NULL, tty, tty);
    if(term)
      set_term(term);
    ok = !!term;
  } else {
    /* Make sure the user doesn't accidentally pipe in data to ncdu's standard
     * input without using "-f -". An annoying input sequence could result in
     * the deletion of your files, which we want to prevent at all costs. */
    if(!isatty(0)) {
      fprintf(stderr, "Standard input is not a TTY. Did you mean to import a file using '-f -'?\n");
      exit(1);
    }
    ok = !!initscr();
  }

  if(!ok) {
    fprintf(stderr, "Error while initializing ncurses.\n");
    exit(1);
  }

  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;
}


/* main program */
int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  argv_parse(argc, argv);

  if(dir_ui == 2)
    init_nc();

  while(1) {
    /* We may need to initialize/clean up the screen when switching from the
     * (sometimes non-ncurses) CALC state to something else. */
    if(pstate != ST_CALC) {
      if(dir_ui == 1)
        fputc('\n', stderr);
      init_nc();
    }

    if(pstate == ST_CALC) {
      if(dir_process()) {
        if(dir_ui == 1)
          fputc('\n', stderr);
        break;
      }
    } else if(pstate == ST_DEL)
      delete_process();
    else if(input_handle(0))
      break;
  }

  if(ncurses_init) {
    erase();
    refresh();
    endwin();
  }
  exclude_clear();

  return 0;
}

