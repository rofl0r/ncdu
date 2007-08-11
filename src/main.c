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

/* check ncdu.h what these are for */
struct dir *dat;
int winrows, wincols;
char sdir[PATH_MAX], *s_export;
int sflags, bflags, sdelay, bgraph;


/* parse command line */
void parseCli(int argc, char **argv) {
  int i, j;

 /* load defaults */
  memset(sdir, 0, PATH_MAX);
  getcwd(sdir, PATH_MAX);
  sflags = 0;
  sdelay = 100;
  bflags = BF_SIZE | BF_DESC;
  s_export = NULL;
  sdir[0] = '\0';

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
        if(argv[i][1] == 'e')
          s_export = argv[++i];
        else if(strcmp(argv[i], "--exclude") == 0)
          addExclude(argv[++i]);
        else if(addExcludeFile(argv[++i])) {
          printf("Can't open %s: %s\n", argv[i], strerror(errno));
          exit(1);
        }
        continue;
      }
     /* small flags */
      for(j=1; j < strlen(argv[i]); j++)
        switch(argv[i][j]) {
          case 'x': sflags |= SF_SMFS; break;
          case 'q': sdelay = 2000;     break;
          case '?':
          case 'h':
            printf("ncdu [-ahvx] [dir]\n\n");
            printf("  -h  This help message\n");
            printf("  -q  Low screen refresh interval when calculating\n");
            printf("  -v  Print version\n");
            printf("  -x  Same filesystem\n");
            exit(0);
          case 'v':
            printf("ncdu %s\n", PACKAGE_VERSION);
            exit(0);  
          default:
            printf("Unknown option: -%c\n", argv[i][j]);
            exit(1);
        }
    } else {
      strcpy(sdir, argv[i]);
    }
  }
  if(s_export && !sdir[0]) {
    printf("Can't export file list: no directory specified!\n");
    exit(1);
  }
}


/* if path is a file: import file list
 * if path is a dir:  calculate directory */
struct dir *loadDir(char *path) {
  struct stat st;
  
  if(stat(path, &st) < 0) {
    if(s_export) {
      printf("Error: Can't open %s!", path);
      exit(1);
    }
    return(showCalc(path));
  }

  if(S_ISREG(st.st_mode))
    return(importFile(path));
  else
    return(showCalc(path));
}


/* main program */
int main(int argc, char **argv) {
  dat = NULL;

  parseCli(argc, argv);

 /* only export file, don't init ncurses */
  if(s_export) {
    dat = loadDir(sdir);
    exportFile(s_export, dat);
    exit(0);
  }

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  ncresize();
  
  if(!sdir[0] && settingsWin())
    goto mainend;

  while((dat = loadDir(sdir)) == NULL)
    if(settingsWin())
      goto mainend;

  showBrowser();

  mainend:
  erase();
  refresh();
  endwin();

  return(0);
}

