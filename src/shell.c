/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2018 Yoran Heling
  Shell support: Copyright (c) 2014 Thomas Jarosch

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

#include "config.h"
#include "global.h"
#include "dirlist.h"
#include "util.h"

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void shell_draw() {
  char *full_path;
  int res;

  /* suspend ncurses mode */
  def_prog_mode();
  endwin();

  full_path = getpath(dirlist_par);
  res = chdir(full_path);
  if (res != 0) {
    reset_prog_mode();
    clear();
    printw("ERROR: Can't change directory: %s (errcode: %d)\n"
           "\n"
           "Press any key to continue.",
           full_path, res);
  } else {
    char *shell = getenv("NCDU_SHELL");
    if (shell == NULL) {
      shell = getenv("SHELL");
      if (shell == NULL)
        shell = DEFAULT_SHELL;
    }

    res = system(shell);

    /* resume ncurses mode */
    reset_prog_mode();

    if (res == -1 || !WIFEXITED(res) || WEXITSTATUS(res) == 127) {
      clear();
      printw("ERROR: Can't execute shell interpreter: %s\n"
             "\n"
             "Press any key to continue.",
             shell);
    }
  }

  refresh();
  pstate = ST_BROWSE;
}

void shell_init() {
  pstate = ST_SHELL;
}
