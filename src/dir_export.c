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
#include <time.h>


static FILE *stream;

/* Stack of device IDs, also used to keep track of the level of nesting */
struct stack {
  uint64_t *list;
  int size, top;
} stack;


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


static void output_info(struct dir *d, const char *name, struct dir_ext *e) {
  if(!extended_info || !(d->flags & FF_EXT))
    e = NULL;

  fputs("{\"name\":\"", stream);
  output_string(name);
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

  if(d->dev != nstack_top(&stack, 0)) {
    fputs(",\"dev\":", stream);
    output_int(d->dev);
  }
  fputs(",\"ino\":", stream);
  output_int(d->ino);

  if(e) {
    fputs(",\"uid\":", stream);
    output_int(e->uid);
    fputs(",\"gid\":", stream);
    output_int(e->gid);
    fputs(",\"mode\":", stream);
    output_int(e->mode);
    fputs(",\"mtime\":", stream);
    output_int(e->mtime);
  }

  /* TODO: Including the actual number of links would be nicer. */
  if(d->flags & FF_HLNKC)
    fputs(",\"hlnkc\":true", stream);
  if(d->flags & FF_ERR)
    fputs(",\"read_error\":true", stream);
  /* excluded/error'd files are "unknown" with respect to the "notreg" field. */
  if(!(d->flags & (FF_DIR|FF_FILE|FF_ERR|FF_EXL|FF_OTHFS)))
    fputs(",\"notreg\":true", stream);
  if(d->flags & FF_EXL)
    fputs(",\"excluded\":\"pattern\"", stream);
  else if(d->flags & FF_OTHFS)
    fputs(",\"excluded\":\"othfs\"", stream);

  fputc('}', stream);
}


/* Note on error handling: For convenience, we just keep writing to *stream
 * without checking the return values of the functions. Only at the and of each
 * item() call do we check for ferror(). This greatly simplifies the code, but
 * assumes that calls to fwrite()/fput./etc don't do any weird stuff when
 * called with a stream that's in an error state. */
static int item(struct dir *item, const char *name, struct dir_ext *ext) {
  if(!item) {
    nstack_pop(&stack);
    if(!stack.top) { /* closing of the root item */
      fputs("]]", stream);
      return fclose(stream);
    } else /* closing of a regular directory item */
      fputs("]", stream);
    return ferror(stream);
  }

  dir_output.items++;

  /* File header.
   * TODO: Add scan options? */
  if(!stack.top) {
    fputs("[1,1,{\"progname\":\""PACKAGE"\",\"progver\":\""PACKAGE_VERSION"\",\"timestamp\":", stream);
    output_int((uint64_t)time(NULL));
    fputc('}', stream);
  }

  fputs(",\n", stream);
  if(item->flags & FF_DIR)
    fputc('[', stream);

  output_info(item, name, ext);

  if(item->flags & FF_DIR)
    nstack_push(&stack, item->dev);

  return ferror(stream);
}


static int final(int fail) {
  nstack_free(&stack);
  return fail ? 1 : 1; /* Silences -Wunused-parameter */
}


int dir_export_init(const char *fn) {
  if(strcmp(fn, "-") == 0)
    stream = stdout;
  else if((stream = fopen(fn, "w")) == NULL)
    return 1;

  nstack_init(&stack);

  pstate = ST_CALC;
  dir_output.item = item;
  dir_output.final = final;
  dir_output.size = 0;
  dir_output.items = 0;
  return 0;
}

