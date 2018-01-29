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

/* This JSON parser has the following limitations:
 * - No support for character encodings incompatible with ASCII (e.g.
 *   UTF-16/32)
 * - Doesn't validate UTF-8 correctness (in fact, besides the ASCII part this
 *   parser doesn't know anything about encoding).
 * - Doesn't validate that there are no duplicate keys in JSON objects.
 * - Isn't very strict with validating non-integer numbers.
 */

#include "global.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>


/* Max. length of any JSON string we're interested in. A string may of course
 * be larger, we're not going to read more than MAX_VAL in memory. If a string
 * we're interested in (e.g. a file name) is longer than this, reading the
 * import will results in an error. */
#define MAX_VAL (32*1024)

/* Minimum number of bytes we request from fread() */
#define MIN_READ_SIZE 1024

/* Read buffer size. Must be at least 2*MIN_READ_SIZE, everything larger
 * improves performance. */
#define READ_BUF_SIZE (32*1024)


int dir_import_active = 0;


/* Use a struct for easy batch-allocation and deallocation of state data. */
struct ctx {
  FILE *stream;

  int line;
  int byte;
  int eof;
  int items;
  char *buf; /* points into readbuf, always zero-terminated. */
  char *lastfill; /* points into readbuf, location of the zero terminator. */

  /* scratch space */
  struct dir    *buf_dir;
  struct dir_ext buf_ext[1];

  char buf_name[MAX_VAL];
  char val[MAX_VAL];
  char readbuf[READ_BUF_SIZE];
} *ctx;


/* Fills readbuf with data from the stream. *buf will have at least n (<
 * READ_BUF_SIZE) bytes available, unless the stream reached EOF or an error
 * occured. If the file data contains a null-type, this is considered an error.
 * Returns 0 on success, non-zero on error. */
static int fill(int n) {
  int r;

  if(ctx->eof)
    return 0;

  r = READ_BUF_SIZE-(ctx->lastfill - ctx->readbuf); /* number of bytes left in the buffer */
  if(n < r)
    n = r-1;
  if(n < MIN_READ_SIZE) {
    r = ctx->lastfill - ctx->buf; /* number of unread bytes left in the buffer */
    memcpy(ctx->readbuf, ctx->buf, r);
    ctx->lastfill = ctx->readbuf + r;
    ctx->buf = ctx->readbuf;
    n = READ_BUF_SIZE-r-1;
  }

  do {
    r = fread(ctx->lastfill, 1, n, ctx->stream);
    if(r != n) {
      if(feof(ctx->stream))
        ctx->eof = 1;
      else if(ferror(ctx->stream) && errno != EINTR) {
        dir_seterr("Read error: %s", strerror(errno));
        return 1;
      }
    }

    ctx->lastfill[r] = 0;
    if(strlen(ctx->lastfill) != (size_t)r) {
      dir_seterr("Zero-byte found in JSON stream");
      return 1;
    }
    ctx->lastfill += r;
    n -= r;
  } while(!ctx->eof && n > MIN_READ_SIZE);

  return 0;
}


/* Two macros that break function calling behaviour, but are damn convenient */
#define E(_x, _m) do {\
    if(_x) {\
      if(!dir_fatalerr)\
        dir_seterr("Line %d byte %d: %s", ctx->line, ctx->byte, _m);\
      return 1;\
    }\
  } while(0)

#define C(_x) do {\
    if(_x)\
      return 1;\
  } while(0)


/* Require at least n bytes in the buffer, throw an error on early EOF.
 * (Macro to quickly handle the common case) */
#define rfill1 (!*ctx->buf && _rfill(1))
#define rfill(_n) ((ctx->lastfill - ctx->buf < (_n)) && _rfill(_n))

static int _rfill(int n) {
  C(fill(n));
  E(ctx->lastfill - ctx->buf < n, "Unexpected EOF");
  return 0;
}


/* Consumes n bytes from the buffer. */
static inline void con(int n) {
  ctx->buf += n;
  ctx->byte += n;
}


/* Consumes any whitespace. If *ctx->buf == 0 after this function, we've reached EOF. */
static int cons() {
  while(1) {
    C(!*ctx->buf && fill(1));

    switch(*ctx->buf) {
    case 0x0A:
      /* Special-case the newline-character with respect to consuming stuff
       * from the buffer. This is the only function which *can* consume the
       * newline character, so it's more efficient to handle it in here rather
       * than in the more general con(). */
      ctx->buf++;
      ctx->line++;
      ctx->byte = 0;
      break;
    case 0x20:
    case 0x09:
    case 0x0D:
      con(1);
      break;
    default:
      return 0;
    }
  }
}


static int rstring_esc(char **dest, int *destlen) {
  unsigned int n;

  C(rfill1);

#define ap(c) if(*destlen > 1) { *((*dest)++) = c; (*destlen)--; }
  switch(*ctx->buf) {
  case '"':  ap('"');  con(1); break;
  case '\\': ap('\\'); con(1); break;
  case '/':  ap('/');  con(1); break;
  case 'b':  ap(0x08); con(1); break;
  case 'f':  ap(0x0C); con(1); break;
  case 'n':  ap(0x0A); con(1); break;
  case 'r':  ap(0x0D); con(1); break;
  case 't':  ap(0x09); con(1); break;
  case 'u':
    C(rfill(5));
#define hn(n) (n >= '0' && n <= '9' ? n-'0' : n >= 'A' && n <= 'F' ? n-'A'+10 : n >= 'a' && n <= 'f' ? n-'a'+10 : 1<<16)
    n = (hn(ctx->buf[1])<<12) + (hn(ctx->buf[2])<<8) + (hn(ctx->buf[3])<<4) + hn(ctx->buf[4]);
#undef hn
    if(n <= 0x007F) {
      ap(n);
    } else if(n <= 0x07FF) {
      ap(0xC0 | (n>>6));
      ap(0x80 | (n & 0x3F));
    } else if(n <= 0xFFFF) {
      ap(0xE0 | (n>>12));
      ap(0x80 | ((n>>6) & 0x3F));
      ap(0x80 | (n & 0x3F));
    } else /* this happens if there was an invalid character (n >= (1<<16)) */
      E(1, "Invalid character in \\u escape");
    con(5);
    break;
  default:
    E(1, "Invalid escape sequence");
  }
#undef ap
  return 0;
}


/* Parse a JSON string and write it to *dest (max. destlen). Consumes but
 * otherwise ignores any characters if the string is longer than destlen. *dest
 * will be null-terminated, dest[destlen-1] = 0 if the string was cut just long
 * enough of was cut off. That byte will be left untouched if the string is
 * small enough. */
static int rstring(char *dest, int destlen) {
  C(rfill1);
  E(*ctx->buf != '"', "Expected string");
  con(1);

  while(1) {
    C(rfill1);
    if(*ctx->buf == '"')
      break;
    if(*ctx->buf == '\\') {
      con(1);
      C(rstring_esc(&dest, &destlen));
      continue;
    }
    E((unsigned char)*ctx->buf <= 0x1F || (unsigned char)*ctx->buf == 0x7F, "Invalid character");
    if(destlen > 1) {
      *(dest++) = *ctx->buf;
      destlen--;
    }
    con(1);
  }
  con(1);
  if(destlen > 0)
    *dest = 0;
  return 0;
}


/* Parse and consume a JSON integer. Throws an error if the value does not fit
 * in an uint64_t, is not an integer or is larger than 'max'. */
static int rint64(uint64_t *val, uint64_t max) {
  uint64_t v;
  int haschar = 0;
  *val = 0;
  while(1) {
    C(!*ctx->buf && fill(1));
    if(*ctx->buf == '0' && !haschar) {
      con(1);
      break;
    }
    if(*ctx->buf >= '0' && *ctx->buf <= '9') {
      haschar = 1;
      v = (*val)*10 + (*ctx->buf-'0');
      E(v < *val, "Invalid (positive) integer");
      *val = v;
      con(1);
      continue;
    }
    E(!haschar, "Invalid (positive) integer");
    break;
  }
  E(*val > max, "Integer out of range");
  return 0;
}


/* Parse and consume a JSON number. The result is discarded.
 * TODO: Improve validation. */
static int rnum() {
  int haschar = 0;
  C(rfill1);
  while(1) {
    C(!*ctx->buf && fill(1));
    if(*ctx->buf == 'e' || *ctx->buf == 'E' || *ctx->buf == '-' || *ctx->buf == '+' || *ctx->buf == '.' || (*ctx->buf >= '0' && *ctx->buf <= '9')) {
      haschar = 1;
      con(1);
    } else {
      E(!haschar, "Invalid JSON value");
      break;
    }
  }
  return 0;
}


static int rlit(const char *v, int len) {
  C(rfill(len));
  E(strncmp(ctx->buf, v, len) != 0, "Invalid JSON value");
  con(len);
  return 0;
}


/* Parse the "<space> <string> <space> : <space>" part of an object key. */
static int rkey(char *dest, int destlen) {
  C(cons() || rstring(dest, destlen) || cons());
  E(*ctx->buf != ':', "Expected ':'");
  con(1);
  return cons();
}


/* (Recursively) parse and consume any JSON value. The result is discarded. */
static int rval() {
  C(rfill1);
  switch(*ctx->buf) {
  case 't': /* true */
    C(rlit("true", 4));
    break;
  case 'f': /* false */
    C(rlit("false", 5));
    break;
  case 'n': /* null */
    C(rlit("null", 4));
    break;
  case '"': /* string */
    C(rstring(NULL, 0));
    break;
  case '{': /* object */
    con(1);
    while(1) {
      C(cons());
      if(*ctx->buf == '}')
        break;
      C(rkey(NULL, 0) || rval() || cons());
      if(*ctx->buf == '}')
        break;
      E(*ctx->buf != ',', "Expected ',' or '}'");
      con(1);
    }
    con(1);
    break;
  case '[': /* array */
    con(1);
    while(1) {
      C(cons());
      if(*ctx->buf == ']')
        break;
      C(cons() || rval() || cons());
      if(*ctx->buf == ']')
        break;
      E(*ctx->buf != ',', "Expected ',' or ']'");
      con(1);
    }
    con(1);
    break;
  default: /* assume number */
    C(rnum());
    break;
  }

  return 0;
}


/* Consumes everything up to the root item, and checks that this item is a dir. */
static int header() {
  uint64_t v;

  C(cons());
  E(*ctx->buf != '[', "Expected JSON array");
  con(1);
  C(cons() || rint64(&v, 10000) || cons());
  E(v != 1, "Incompatible major format version");
  E(*ctx->buf != ',', "Expected ','");
  con(1);
  C(cons() || rint64(&v, 10000) || cons()); /* Ignore the minor version for now */
  E(*ctx->buf != ',', "Expected ','");
  con(1);
  /* Metadata block is currently ignored */
  C(cons() || rval() || cons());
  E(*ctx->buf != ',', "Expected ','");
  con(1);

  C(cons());
  E(*ctx->buf != '[', "Top-level item must be a directory");

  return 0;
}


static int item(uint64_t);

/* Read and add dir contents */
static int itemdir(uint64_t dev) {
  while(1) {
    C(cons());
    if(*ctx->buf == ']')
      break;
    E(*ctx->buf != ',', "Expected ',' or ']'");
    con(1);
    C(cons() || item(dev));
  }
  con(1);
  C(cons());
  return 0;
}


/* Reads a JSON object representing a struct dir/dir_ext item. Writes to
 * ctx->buf_dir, ctx->buf_ext and ctx->buf_name. */
static int iteminfo() {
  uint64_t iv;

  E(*ctx->buf != '{', "Expected JSON object");
  con(1);

  while(1) {
    C(rkey(ctx->val, MAX_VAL));
    /* TODO: strcmp() in this fashion isn't very fast. */
    if(strcmp(ctx->val, "name") == 0) {              /* name */
      ctx->val[MAX_VAL-1] = 1;
      C(rstring(ctx->val, MAX_VAL));
      E(ctx->val[MAX_VAL-1] != 1, "Too large string value");
      strcpy(ctx->buf_name, ctx->val);
    } else if(strcmp(ctx->val, "asize") == 0) {      /* asize */
      C(rint64(&iv, INT64_MAX));
      ctx->buf_dir->asize = iv;
    } else if(strcmp(ctx->val, "dsize") == 0) {      /* dsize */
      C(rint64(&iv, INT64_MAX));
      ctx->buf_dir->size = iv;
    } else if(strcmp(ctx->val, "dev") == 0) {        /* dev */
      C(rint64(&iv, UINT64_MAX));
      ctx->buf_dir->dev = iv;
    } else if(strcmp(ctx->val, "ino") == 0) {        /* ino */
      C(rint64(&iv, UINT64_MAX));
      ctx->buf_dir->ino = iv;
    } else if(strcmp(ctx->val, "uid") == 0) {        /* uid */
      C(rint64(&iv, INT32_MAX));
      ctx->buf_dir->flags |= FF_EXT;
      ctx->buf_ext->uid = iv;
    } else if(strcmp(ctx->val, "gid") == 0) {        /* gid */
      C(rint64(&iv, INT32_MAX));
      ctx->buf_dir->flags |= FF_EXT;
      ctx->buf_ext->gid = iv;
    } else if(strcmp(ctx->val, "mode") == 0) {       /* mode */
      C(rint64(&iv, UINT16_MAX));
      ctx->buf_dir->flags |= FF_EXT;
      ctx->buf_ext->mode = iv;
    } else if(strcmp(ctx->val, "mtime") == 0) {      /* mtime */
      C(rint64(&iv, UINT64_MAX));
      ctx->buf_dir->flags |= FF_EXT;
      ctx->buf_ext->mtime = iv;
    } else if(strcmp(ctx->val, "hlnkc") == 0) {      /* hlnkc */
      if(*ctx->buf == 't') {
        C(rlit("true", 4));
        ctx->buf_dir->flags |= FF_HLNKC;
      } else
        C(rlit("false", 5));
    } else if(strcmp(ctx->val, "read_error") == 0) { /* read_error */
      if(*ctx->buf == 't') {
        C(rlit("true", 4));
        ctx->buf_dir->flags |= FF_ERR;
      } else
        C(rlit("false", 5));
    } else if(strcmp(ctx->val, "excluded") == 0) {   /* excluded */
      C(rstring(ctx->val, 8));
      if(strcmp(ctx->val, "otherfs") == 0)
        ctx->buf_dir->flags |= FF_OTHFS;
      else
        ctx->buf_dir->flags |= FF_EXL;
    } else if(strcmp(ctx->val, "notreg") == 0) {     /* notreg */
      if(*ctx->buf == 't') {
        C(rlit("true", 4));
        ctx->buf_dir->flags &= ~FF_FILE;
      } else
        C(rlit("false", 5));
    } else
      C(rval());
    /* TODO: Extended attributes */

    C(cons());
    if(*ctx->buf == '}')
      break;
    E(*ctx->buf != ',', "Expected ',' or '}'");
    con(1);
  }
  con(1);

  E(!*ctx->buf_name, "No name field present in item information object");
  ctx->items++;
  /* Only call input_handle() once for every 32 items. Importing items is so
   * fast that the time spent in input_handle() dominates when called every
   * time. Don't set this value too high, either, as feedback should still be
   * somewhat responsive when our import data comes from a slow-ish source. */
  return !(ctx->items & 31) ? input_handle(1) : 0;
}


/* Recursively reads a file or directory item */
static int item(uint64_t dev) {
  int isdir = 0;
  int isroot = ctx->items == 0;

  if(*ctx->buf == '[') {
    isdir = 1;
    con(1);
    C(cons());
  }

  memset(ctx->buf_dir, 0, offsetof(struct dir, name));
  memset(ctx->buf_ext, 0, sizeof(struct dir_ext));
  *ctx->buf_name = 0;
  ctx->buf_dir->flags |= isdir ? FF_DIR : FF_FILE;
  ctx->buf_dir->dev = dev;

  C(iteminfo());
  dev = ctx->buf_dir->dev;

  if(isroot)
    dir_curpath_set(ctx->buf_name);
  else
    dir_curpath_enter(ctx->buf_name);

  if(isdir) {
    if(dir_output.item(ctx->buf_dir, ctx->buf_name, ctx->buf_ext)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
    C(itemdir(dev));
    if(dir_output.item(NULL, 0, NULL)) {
      dir_seterr("Output error: %s", strerror(errno));
      return 1;
    }
  } else if(dir_output.item(ctx->buf_dir, ctx->buf_name, ctx->buf_ext)) {
    dir_seterr("Output error: %s", strerror(errno));
    return 1;
  }

  if(!isroot)
    dir_curpath_leave();

  return 0;
}


static int footer() {
  C(cons());
  E(*ctx->buf != ']', "Expected ']'");
  con(1);
  C(cons());
  E(*ctx->buf, "Trailing garbage");
  return 0;
}


static int process() {
  int fail = 0;

  header();

  if(!dir_fatalerr)
    fail = item(0);

  if(!dir_fatalerr && !fail)
    footer();

  if(fclose(ctx->stream) && !dir_fatalerr && !fail)
    dir_seterr("Error closing file: %s", strerror(errno));
  free(ctx->buf_dir);
  free(ctx);

  while(dir_fatalerr && !input_handle(0))
    ;
  return dir_output.final(dir_fatalerr || fail);
}


int dir_import_init(const char *fn) {
  FILE *stream;
  if(strcmp(fn, "-") == 0)
    stream = stdin;
  else if((stream = fopen(fn, "r")) == NULL)
    return 1;

  ctx = malloc(sizeof(struct ctx));
  ctx->stream = stream;
  ctx->line = 1;
  ctx->byte = ctx->eof = ctx->items = 0;
  ctx->buf = ctx->lastfill = ctx->readbuf;
  ctx->buf_dir = malloc(dir_memsize(""));
  ctx->readbuf[0] = 0;

  dir_curpath_set(fn);
  dir_process = process;
  dir_import_active = 1;
  return 0;
}

