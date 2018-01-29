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

#ifndef _dir_h
#define _dir_h

/* The dir_* functions and files implement the SCAN state and are organized as
 * follows:
 *
 * Input:
 *   Responsible for getting a directory structure into ncdu. Will call the
 *   Output functions for data and the UI functions for feedback. Currently
 *   there is only one input implementation: dir_scan.c
 * Output:
 *   Called by the Input handling code when there's some new file/directory
 *   information. The Output code is responsible for doing something with it
 *   and determines what action should follow after the Input is done.
 *   Currently there is only one output implementation: dir_mem.c.
 * Common:
 *   Utility functions and UI code for use by the Input handling code to draw
 *   progress/error information on the screen, handle any user input and misc.
 *   stuff.
 */


/* "Interface" that Input code should call and Output code should implement. */
struct dir_output {
  /* Called when there is new file/dir info. Call stack for an example
   * directory structure:
   *   /           item('/')
   *   /subdir     item('subdir')
   *   /subdir/f   item('f')
   *   ..          item(NULL)
   *   /abc        item('abc')
   *   ..          item(NULL)
   * Every opened dir is followed by a call to NULL.  There is only one top-level
   * dir item. The name of the top-level dir item is the absolute path to the
   * scanned directory.
   *
   * The *item struct has the following fields set when item() is called:
   *   size, asize, ino, dev, flags (only DIR,FILE,ERR,OTHFS,EXL,HLNKC).
   * All other fields/flags should be initialzed to NULL or 0.
   * The name and dir_ext fields are given separately.
   * All pointers may be overwritten or freed in subsequent calls, so this
   * function should make a copy if necessary.
   *
   * The function should return non-zero on error, at which point errno is
   * assumed to be set to something sensible.
   */
  int (*item)(struct dir *, const char *, struct dir_ext *);

  /* Finalizes the output to go to the next program state or exit ncdu. Called
   * after item(NULL) has been called for the root item or before any item()
   * has been called at all.
   * Argument indicates success (0) or failure (1).
   * Failure happens when the root directory couldn't be opened, chdir, lstat,
   * read, when it is empty, or when the user aborted the operation.
   * Return value should be 0 to continue running ncdu, 1 to exit.
   */
  int (*final)(int);

  /* The output code is responsible for updating these stats. Can be 0 when not
   * available. */
  int64_t size;
  int items;
};


/* Initializes the SCAN state and dir_output for immediate browsing.
 * On success:
 *   If a dir item is given, overwrites it with the new dir struct.
 *   Then calls browse_init(new_dir_struct->sub).
 * On failure:
 *   If a dir item is given, will just call browse_init(orig).
 *   Otherwise, will exit ncdu.
 */
void dir_mem_init(struct dir *);

/* Initializes the SCAN state and dir_output for exporting to a file. */
int dir_export_init(const char *fn);


/* Function set by input code. Returns dir_output.final(). */
int (*dir_process)();

/* Scanning a live directory */
extern int dir_scan_smfs;
void dir_scan_init(const char *path);

/* Importing a file */
extern int dir_import_active;
int dir_import_init(const char *fn);


/* The currently configured output functions. */
extern struct dir_output dir_output;

/* Current path that we're working with. These are defined in dir_common.c. */
extern char *dir_curpath;
void dir_curpath_set(const char *);
void dir_curpath_enter(const char *);
void dir_curpath_leave();

/* Sets the path where the last error occured, or reset on NULL. */
void dir_setlasterr(const char *);

/* Error message on fatal error, or NULL if there hasn't been a fatal error yet. */
extern char *dir_fatalerr;
void dir_seterr(const char *, ...);

extern int dir_ui;
int dir_key(int);
void dir_draw();

#endif
