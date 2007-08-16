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

#define KEYS 14

char *keys[KEYS*2] = {
/*|----key----|  |----------------description----------------|*/
      "up/down", "Cycle through the items",
  "right/enter", "Open directory",
         "left", "Previous directory",
            "n", "Sort by name (ascending/descending)",
            "s", "Sort by size (ascending/descending)",
            "d", "Delete selected file or directory",
            "t", "Toggle dirs before files when sorting",
            "g", "Show percentage and/or graph",
            "p", "Toggle between powers of 1000 and 1024",
            "a", "Toggle between apparent size and disk usage",
            "h", "Show/hide hidden or excluded files",
            "i", "Show information about selected item",
            "r", "Recalculate the current directory",
            "q", "Quit ncdu"
};



void drawHelp(int page, int start) {
  int i, line;

  nccreate(15, 60, "ncdu help");
  ncaddstr(13, 38, "Press q to continue");

  if(page == 1)
    attron(A_REVERSE);
  ncaddstr(0, 30, "1:Keys");
  attroff(A_REVERSE);
  if(page == 2)
    attron(A_REVERSE);
  ncaddstr(0, 39, "2:Format");
  attroff(A_REVERSE);
  if(page == 3)
    attron(A_REVERSE);
  ncaddstr(0, 50, "3:About");
  attroff(A_REVERSE);

  switch(page) {
    case 1:
      line = 1;
      for(i=start*2; i<start*2+20; i+=2) {
        attron(A_BOLD);
        ncaddstr(++line, 13-strlen(keys[i]), keys[i]);
        attroff(A_BOLD);
        ncaddstr(line, 15, keys[i+1]);
      }
      if(start != KEYS-10)
        ncaddstr(12, 25, "-- more --");
      break;
    case 2:
      attron(A_BOLD);
      ncaddstr(2, 3, "X  [size] [graph] [file or directory]");
      attroff(A_BOLD);
      ncaddstr(3, 4, "The X is only present in the following cases:");
      attron(A_BOLD);
      ncaddch( 5, 4, '!');
      ncaddch( 6, 4, '.');
      ncaddch( 7, 4, '<');
      ncaddch( 8, 4, '>');
      ncaddch( 9, 4, '@');
      ncaddch(10, 4, 'e');
      attroff(A_BOLD);
      ncaddstr( 5, 7, "An error occured while reading this directory");
      ncaddstr( 6, 7, "An error occured while reading a subdirectory");
      ncaddstr( 7, 7, "File or directory is excluded from the statistics");
      ncaddstr( 8, 7, "Directory was on an other filesystem");
      ncaddstr( 9, 7, "This is not a file nor a dir (symlink, socket, ...)");
      ncaddstr(10, 7, "Empty directory");
      break;
    case 3:
      /* Indeed, too much spare time */
      attron(A_REVERSE);
#define x 12
#define y 3
      /* N */
      ncaddstr(y+0, x+0, "      ");
      ncaddstr(y+1, x+0, "  ");
      ncaddstr(y+2, x+0, "  ");
      ncaddstr(y+3, x+0, "  ");
      ncaddstr(y+4, x+0, "  ");
      ncaddstr(y+1, x+4, "  ");
      ncaddstr(y+2, x+4, "  ");
      ncaddstr(y+3, x+4, "  ");
      ncaddstr(y+4, x+4, "  ");
      /* C */
      ncaddstr(y+0, x+8, "     ");
      ncaddstr(y+1, x+8, "  ");
      ncaddstr(y+2, x+8, "  ");
      ncaddstr(y+3, x+8, "  ");
      ncaddstr(y+4, x+8, "     ");
      /* D */
      ncaddstr(y+0, x+19, "  ");
      ncaddstr(y+1, x+19, "  ");
      ncaddstr(y+2, x+15, "      ");
      ncaddstr(y+3, x+15, "  ");
      ncaddstr(y+3, x+19, "  ");
      ncaddstr(y+4, x+15, "      ");
      /* U */
      ncaddstr(y+0, x+23, "  ");
      ncaddstr(y+1, x+23, "  ");
      ncaddstr(y+2, x+23, "  ");
      ncaddstr(y+3, x+23, "  ");
      ncaddstr(y+0, x+27, "  ");
      ncaddstr(y+1, x+27, "  ");
      ncaddstr(y+2, x+27, "  ");
      ncaddstr(y+3, x+27, "  ");
      ncaddstr(y+4, x+23, "      ");
      attroff(A_REVERSE);
      ncaddstr(y+0, x+30, "NCurses");
      ncaddstr(y+1, x+30, "Disk");
      ncaddstr(y+2, x+30, "Usage");
      ncprint( y+4, x+30, "%s", PACKAGE_VERSION);
      ncaddstr( 9,  7, "Written by Yoran Heling <projects@yorhel.nl>");
      ncaddstr(10, 16, "http://dev.yorhel.nl/ncdu/");
      break;
  }
  refresh();
}


void showHelp(void) {
  int p = 1, st = 0, ch;

  drawHelp(p, st);
  while((ch = getch())) {
    switch(ch) {
      case ERR:
        break;
      case '1':
      case '2':
      case '3':
        p = ch-'0';
        st = 0;
        break;
      case KEY_RIGHT:
      case KEY_NPAGE:
        if(++p > 3)
          p = 3;
        st = 0;
        break;
      case KEY_LEFT:
      case KEY_PPAGE:
        if(--p < 1)
          p = 1;
        st = 0;
        break;
      case KEY_DOWN:
      case ' ':
        if(st < KEYS-10)
          st++;
        break;
      case KEY_UP:
        if(st > 0)
          st--;
        break;
      case KEY_RESIZE:
        ncresize();
        drawBrowser(0);
        break;
      default:
        return;
    }
    drawHelp(p, st);
  }
}


