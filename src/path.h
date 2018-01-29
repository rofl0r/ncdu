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
/*
 path.c reimplements realpath() and chdir(), both functions accept
 arbitrary long path names not limited by PATH_MAX.

 Caveats/bugs:
  - path_real uses chdir(), so it's not thread safe
  - Process requires +x access for all directory components
  - Potentionally slow
  - Doesn't check return value of malloc() and realloc()
  - path_real doesn't check for the existance of the last component
  - cwd is unreliable after path_real
*/

#ifndef _path_h
#define _path_h

/* path_real reimplements realpath(). The returned string is allocated
   by malloc() and should be manually free()d by the programmer. */
extern char *path_real(const char *);

/* works exactly the same as chdir() */
extern int   path_chdir(const char *);

#endif
