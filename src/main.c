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

#include "global.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/time.h>

#include <yopt.h>


int pstate;
int read_only = 0;
long update_delay = 100;
int cachedir_tags = 0;
int extended_info = 0;

static int min_rows = 17, min_cols = 60;
static int ncurses_init = 0;
static int ncurses_tty = 0; /* Explicitely open /dev/tty instead of using stdio */
static long lastupdate = 999;


static void screen_draw() {
  switch(pstate) {
    case ST_CALC:   dir_draw();    break;
    case ST_BROWSE: browse_draw(); break;
    case ST_HELP:   help_draw();   break;
    case ST_SHELL:  shell_draw();  break;
    case ST_DEL:    delete_draw(); break;
    case ST_QUIT:   quit_draw();   break;
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
      case ST_QUIT:   return quit_key(ch);
    }
    screen_draw();
  }
  return 0;
}


/* parse command line */
static void argv_parse(int argc, char **argv) {
  yopt_t yopt;
  int v;
  char *val;
  char *export = NULL;
  char *import = NULL;
  char *dir = NULL;

  static yopt_opt_t opts[] = {
    { 'h', 0, "-h,-?" },
    { 'q', 0, "-q" },
    { 'v', 0, "-v" },
    { 'x', 0, "-x" },
    { 'e', 0, "-e" },
    { 'r', 0, "-r" },
    { 'o', 1, "-o" },
    { 'f', 1, "-f" },
    { '0', 0, "-0" },
    { '1', 0, "-1" },
    { '2', 0, "-2" },
    {  1,  1, "--exclude" },
    { 'X', 1, "-X,--exclude-from" },
    { 'C', 0, "--exclude-caches" },
    { 's', 0, "--si" },
    { 'Q', 0, "--confirm-quit" },
    { 'c', 1, "--color" },
    {0,0,NULL}
  };

  dir_ui = -1;
  si = 0;

  yopt_init(&yopt, argc, argv, opts);
  while((v = yopt_next(&yopt, &val)) != -1) {
    switch(v) {
    case  0 : dir = val; break;
    case 'h':
      printf("ncdu <options> <directory>\n\n");
      printf("  -h                         This help message\n");
      printf("  -q                         Quiet mode, refresh interval 2 seconds\n");
      printf("  -v                         Print version\n");
      printf("  -x                         Same filesystem\n");
      printf("  -e                         Enable extended information\n");
      printf("  -r                         Read only\n");
      printf("  -o FILE                    Export scanned directory to FILE\n");
      printf("  -f FILE                    Import scanned directory from FILE\n");
      printf("  -0,-1,-2                   UI to use when scanning (0=none,2=full ncurses)\n");
      printf("  --si                       Use base 10 (SI) prefixes instead of base 2\n");
      printf("  --exclude PATTERN          Exclude files that match PATTERN\n");
      printf("  -X, --exclude-from FILE    Exclude files that match any pattern in FILE\n");
      printf("  --exclude-caches           Exclude directories containing CACHEDIR.TAG\n");
      printf("  --confirm-quit             Confirm quitting ncdu\n");
      printf("  --color SCHEME             Set color scheme\n");
      exit(0);
    case 'q': update_delay = 2000; break;
    case 'v':
      printf("ncdu %s\n", PACKAGE_VERSION);
      exit(0);
    case 'x': dir_scan_smfs = 1; break;
    case 'e': extended_info = 1; break;
    case 'r': read_only++; break;
    case 's': si = 1; break;
    case 'o': export = val; break;
    case 'f': import = val; break;
    case '0': dir_ui = 0; break;
    case '1': dir_ui = 1; break;
    case '2': dir_ui = 2; break;
    case 'Q': confirm_quit = 1; break;
    case  1 : exclude_add(val); break; /* --exclude */
    case 'X':
      if(exclude_addfile(val)) {
        fprintf(stderr, "Can't open %s: %s\n", val, strerror(errno));
        exit(1);
      }
      break;
    case 'C':
      cachedir_tags = 1;
      break;
    case 'c':
      if(strcmp(val, "off") == 0)  { uic_theme = 0; }
      if(strcmp(val, "dark") == 0) { uic_theme = 1; }
      else {
        fprintf(stderr, "Unknown --color option: %s\n", val);
        exit(1);
      }
      break;
    case -2:
      fprintf(stderr, "ncdu: %s.\n", val);
      exit(1);
    }
  }

  if(export) {
    if(dir_export_init(export)) {
      fprintf(stderr, "Can't open %s: %s\n", export, strerror(errno));
      exit(1);
    }
    if(strcmp(export, "-") == 0)
      ncurses_tty = 1;
  } else
    dir_mem_init(NULL);

  if(import) {
    if(dir_import_init(import)) {
      fprintf(stderr, "Can't open %s: %s\n", import, strerror(errno));
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

  uic_init();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  if(ncresize(min_rows, min_cols))
    min_rows = min_cols = 0;
}


/* main program */
int main(int argc, char **argv) {
  read_locale();
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

