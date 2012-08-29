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


static FILE *stream;
static int level; /* Current level of nesting */


static void output_string(const char *str) {
  for(; *str; str++) {
    switch(*str) {
    case '\n': fputs("\\n", stream); break;
    case '\r': fputs("\\r", stream); break;
    case '\b': fputs("\\b", stream); break;
    case '\t': fputs("\\t", stream); break;
    case '\f': fputs("\\f", stream); break;
    case '\\': fputs("\\\\", stream); break;
    case '"':  fputs("\\\"", stream); break;
    default:
      if((unsigned char)*str <= 31 || (unsigned char)*str == 127)
        fprintf(stream, "\\u00%02x", *str);
      else
        fputc(*str, stream);
      break;
    }
  }
}


static void output_int(uint64_t n) {
  char tmp[20];
  int i = 0;

  do
    tmp[i++] = n % 10;
  while((n /= 10) > 0);

  while(i--)
    fputc(tmp[i]+'0', stream);
}


static void output_info(struct dir *d) {
  fputs("{\"name\":\"", stream);
  output_string(d->name);
  fputc('"', stream);
  /* No need for asize/dsize if they're 0 (which happens with excluded or failed-to-stat files) */
  if(d->asize) {
    fputs(",\"asize\":", stream);
    output_int((uint64_t)d->asize);
  }
  if(d->size) {
    fputs(",\"dsize\":", stream);
    output_int((uint64_t)d->size);
  }
  /* TODO: No need to include a dev is it's the same as the parent dir. */
  fputs(",\"dev\":", stream);
  output_int(d->dev);
  fputs(",\"ino\":", stream);
  output_int(d->ino);
  if(d->flags & FF_HLNKC) /* TODO: Including the actual number of links would be nicer. */
    fputs(",\"hlnkc\":true", stream);
  if(d->flags & FF_ERR)
    fputs(",\"read_error\":true", stream);
  if(!(d->flags & (FF_DIR|FF_FILE)))
    fputs(",\"notreg\":true", stream);
  if(d->flags & FF_EXL)
    fputs(",\"excluded\":\"pattern", stream);
  else if(d->flags & FF_OTHFS)
    fputs(",\"excluded\":\"othfs", stream);
  fputc('}', stream);
}


/* TODO: Error handling / reporting! */
static void item(struct dir *item) {
  if(!item) {
    if(!--level) { /* closing of the root item */
      fputs("]]", stream);
      fclose(stream);
    } else /* closing of a regular directory item */
      fputs("]", stream);
    return;
  }

  dir_output.items++;

  /* File header.
   * TODO: Add scan options? */
  if(item->flags & FF_DIR && !level++)
    fputs("[1,0,{\"progname\":\""PACKAGE"\",\"progver\":\""PACKAGE_VERSION"\"}", stream);

  fputs(",\n", stream);
  if(item->flags & FF_DIR)
    fputc('[', stream);

  output_info(item);
}


static int final(int fail) {
  return fail ? 1 : 1; /* Silences -Wunused-parameter */
}


int dir_export_init(const char *fn) {
  if(strcmp(fn, "-") == 0)
    stream = stdout;
  else if((stream = fopen(fn, "w")) == NULL)
    return 1;

  level = 0;
  pstate = ST_CALC;
  dir_output.item = item;
  dir_output.final = final;
  dir_output.size = 0;
  dir_output.items = 0;
  return 0;
}

