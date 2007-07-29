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


void drawHelp(int page) {
  WINDOW *hlp;

  hlp = newwin(15, 60, winrows/2-7, wincols/2-30);
  box(hlp, 0, 0);
  wattron(hlp, A_BOLD);
  mvwaddstr(hlp, 0, 4, "ncdu help");
  wattroff(hlp, A_BOLD);
  mvwaddstr(hlp, 13, 32, "Press any key to continue");

  if(page == 1)
    wattron(hlp, A_REVERSE);
  mvwaddstr(hlp, 0, 30, "1:Keys");
  wattroff(hlp, A_REVERSE);
  if(page == 2)
    wattron(hlp, A_REVERSE);
  mvwaddstr(hlp, 0, 39, "2:Format");
  wattroff(hlp, A_REVERSE);
  if(page == 3)
    wattron(hlp, A_REVERSE);
  mvwaddstr(hlp, 0, 50, "3:About");
  wattroff(hlp, A_REVERSE);

  switch(page) {
    case 1:
      wattron(hlp, A_BOLD);
      mvwaddstr(hlp, 2,  7, "up/down");
      mvwaddstr(hlp, 3,  3, "right/enter");
      mvwaddstr(hlp, 4, 10, "left");
      mvwaddstr(hlp, 5, 11, "n/s");
      mvwaddch( hlp, 6, 13, 'd');
      mvwaddch( hlp, 7, 13, 't');
      mvwaddch( hlp, 8, 13, 'g');
      mvwaddch( hlp, 9, 13, 'p');
      mvwaddch( hlp,10, 13, 'h');
      mvwaddch( hlp,11, 13, 'r');
      mvwaddch( hlp,12, 13, 'q');
      wattroff(hlp, A_BOLD);
      mvwaddstr(hlp, 2, 16, "Cycle through the items");
      mvwaddstr(hlp, 3, 16, "Open directory");
      mvwaddstr(hlp, 4, 16, "Previous directory");
      mvwaddstr(hlp, 5, 16, "Sort by name or size (asc/desc)");
      mvwaddstr(hlp, 6, 16, "Delete selected file or directory");
      mvwaddstr(hlp, 7, 16, "Toggle dirs before files when sorting");
      mvwaddstr(hlp, 8, 16, "Show percentage and/or graph");
      mvwaddstr(hlp, 9, 16, "Toggle between powers of 1000 and 1024");
      mvwaddstr(hlp,10, 16, "Show/hide hidden or excluded files");
      mvwaddstr(hlp,11, 16, "Recalculate the current directory");
      mvwaddstr(hlp,12, 16, "Quit ncdu");
      break;
    case 2:
      wattron(hlp, A_BOLD);
      mvwaddstr(hlp, 2, 3, "X  [size] [graph] [file or directory]");
      wattroff(hlp, A_BOLD);
      mvwaddstr(hlp, 3, 4, "The X is only present in the following cases:");
      wattron(hlp, A_BOLD);
      mvwaddch(hlp, 5, 4, '!');
      mvwaddch(hlp, 6, 4, '.');
      mvwaddch(hlp, 7, 4, '<');
      mvwaddch(hlp, 8, 4, '>');
      mvwaddch(hlp, 9, 4, '@');
      mvwaddch(hlp,10, 4, 'e');
      wattroff(hlp, A_BOLD);
      mvwaddstr(hlp, 5, 7, "An error occured while reading this directory");
      mvwaddstr(hlp, 6, 7, "An error occured while reading a subdirectory");
      mvwaddstr(hlp, 7, 7, "File or directory is excluded from the statistics");
      mvwaddstr(hlp, 8, 7, "Directory was on an other filesystem");
      mvwaddstr(hlp, 9, 7, "This is not a file nor a dir (symlink, socket, ...)");
      mvwaddstr(hlp,10, 7, "Empty directory");
      break;
    case 3:
      /* Indeed, too much spare time */
      wattron(hlp, A_REVERSE);
#define x 12
#define y 3
      /* N */
      mvwaddstr(hlp, y+0, x+0, "      ");
      mvwaddstr(hlp, y+1, x+0, "  ");
      mvwaddstr(hlp, y+2, x+0, "  ");
      mvwaddstr(hlp, y+3, x+0, "  ");
      mvwaddstr(hlp, y+4, x+0, "  ");
      mvwaddstr(hlp, y+1, x+4, "  ");
      mvwaddstr(hlp, y+2, x+4, "  ");
      mvwaddstr(hlp, y+3, x+4, "  ");
      mvwaddstr(hlp, y+4, x+4, "  ");
      /* C */
      mvwaddstr(hlp, y+0, x+8, "     ");
      mvwaddstr(hlp, y+1, x+8, "  ");
      mvwaddstr(hlp, y+2, x+8, "  ");
      mvwaddstr(hlp, y+3, x+8, "  ");
      mvwaddstr(hlp, y+4, x+8, "     ");
      /* D */
      mvwaddstr(hlp, y+0, x+19, "  ");
      mvwaddstr(hlp, y+1, x+19, "  ");
      mvwaddstr(hlp, y+2, x+15, "      ");
      mvwaddstr(hlp, y+3, x+15, "  ");
      mvwaddstr(hlp, y+3, x+19, "  ");
      mvwaddstr(hlp, y+4, x+15, "      ");
      /* U */
      mvwaddstr(hlp, y+0, x+23, "  ");
      mvwaddstr(hlp, y+1, x+23, "  ");
      mvwaddstr(hlp, y+2, x+23, "  ");
      mvwaddstr(hlp, y+3, x+23, "  ");
      mvwaddstr(hlp, y+0, x+27, "  ");
      mvwaddstr(hlp, y+1, x+27, "  ");
      mvwaddstr(hlp, y+2, x+27, "  ");
      mvwaddstr(hlp, y+3, x+27, "  ");
      mvwaddstr(hlp, y+4, x+23, "      ");
      wattroff(hlp, A_REVERSE);
      mvwaddstr(hlp, y+0, x+30, "NCurses");
      mvwaddstr(hlp, y+1, x+30, "Disk");
      mvwaddstr(hlp, y+2, x+30, "Usage");
      mvwprintw(hlp, y+4, x+30, "%s", PACKAGE_VERSION);
      mvwaddstr(hlp, 9,  7, "Written by Yoran Heling <projects@yorhel.nl>");
      mvwaddstr(hlp,10, 16, "http://dev.yorhel.nl/ncdu/");
      break;
  }
  wrefresh(hlp);
  delwin(hlp); /* no need to use it anymore - free it */
}


void showHelp(void) {
  int p = 1, ch;

  drawHelp(p);
  while((ch = getch())) {
    switch(ch) {
      case ERR:
        break;
      case '1':
        p = 1;
        break;
      case '2':
        p = 2;
        break;
      case '3':
        p = 3;
        break;
      case KEY_RIGHT:
      case KEY_NPAGE:
        if(++p > 3)
          p = 3;
        break;
      case KEY_LEFT:
      case KEY_PPAGE:
        if(--p < 1)
          p = 1;
        break;
      case KEY_RESIZE:
        ncresize();
        drawBrowser(0);
        break;
      default:
        return;
    }
    drawHelp(p);
  }
}


