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


/* !WARNING! There is currently *NO* error handling at all */


/*
 *   E X P O R T I N G
*/


unsigned int ilevel;


#define writeInt(hl, word, bytes) _writeInt(hl, (unsigned char *) &word, bytes, sizeof(word))

/* Write any unsigned integer in network byte order.
 * This function always writes the number of bytes specified by storage to the
 * file, disregarding the actual size of word. If the actual size is smaller
 * than the storage, the number will be preceded by null-bytes. If the actual
 * size is greater than the storage, we assume the value we want to store fits
 * in the storage size, disregarding any overflow.
*/
void _writeInt(FILE *wr, unsigned char *word, int storage, int size) {
  unsigned char buf[8]; /* should be able to store any integer type */
  int i;

 /* clear the buffer */
  memset(buf, 0, 8);

 /* store the integer in the end of the buffer, in network byte order */
  if(IS_BIG_ENDIAN)
    memcpy(buf+(8-size), word, size);
  else
    for(i=0; i<size; i++)
      buf[8-size+i] = word[size-1-i];

 /* write only the last bytes of the buffer (as specified by storage) to the file */
  fwrite(buf+(8-storage), 1, storage, wr);
}


/* recursive */
long calcItems(struct dir *dr) {
  int count = 0;
  do {
    if(dr->sub)
      count += calcItems(dr->sub);
    count++;
  } while((dr = dr->next) != NULL);
  return(count);
}


/* also recursive */
void writeDirs(FILE *wr, struct dir *dr) {
  unsigned char f;
  
  do {
   /* flags - the slow but portable way */
    f = 0;
    if(dr->flags & FF_DIR)   f += EF_DIR;
    if(dr->flags & FF_FILE)  f += EF_FILE;
    if(dr->flags & FF_ERR)   f += EF_ERR;
    if(dr->flags & FF_OTHFS) f += EF_OTHFS;
    if(dr->flags & FF_EXL)   f += EF_EXL;

    writeInt(wr, ilevel, 2);
    fwrite(&f, 1, 1, wr);
    writeInt(wr, dr->size, 8);
    writeInt(wr, dr->asize, 8);
    fprintf(wr, "%s%c", dr->name, 0);

    if(dr->sub != NULL) {
      ilevel++;
      writeDirs(wr, dr->sub);
      ilevel--;
    }

  } while((dr = dr->next) != NULL);
}


void exportFile(char *dest, struct dir *src) {
  FILE *wr;
  struct timeval tv;
  long items;

  wr = fopen(dest, "w");

  /* header */
  fprintf(wr, "ncdu%c%s%c", 1, PACKAGE_STRING, 0);

 /* we assume timestamp > 0 */
  gettimeofday(&tv, NULL);
  writeInt(wr, tv.tv_sec, 8);

  items = calcItems(src);
  writeInt(wr, items, 4);

  /* the directory items */
  ilevel = 0;
  writeDirs(wr, src);

  fclose(wr);
}




/*
 *  I M P O R T I N G
*/


#define readInt(hl, word, bytes) if(!_readInt(hl, (unsigned char *) &word, bytes, sizeof(word))) return(NULL)

/* reverse of writeInt */
int _readInt(FILE *rd, unsigned char *word, int storage, int size) {
  unsigned char buf[8];
  int i;

 /* clear the buffer */
  memset(buf, 0, 8);

 /* read integer to the end of the buffer */
  if(fread(buf+(8-storage), 1, storage, rd) != storage)
    return(0);

 /* copy buf to word, in host byte order */
  if(IS_BIG_ENDIAN)
    memcpy(word, buf+(8-size), size);
  else
    for(i=0; i<size; i++)
      word[i] = buf[7-i];
  return(1);
}


struct dir *importFile(char *filename) {
  unsigned char buf[8];
  FILE *rd;
  int i, item;
  unsigned long items;
  unsigned int level;
  struct dir *prev, *cur, *tmp, *parent;

  if(!(rd = fopen(filename, "r")))
    return(NULL);

 /* check filetype */
  if(fread(buf, 1, 5, rd) != 5)
    return(NULL);
  
  if(buf[0] != 'n' || buf[1] != 'c' || buf[2] != 'd'
      || buf[3] != 'u' || buf[4] != (char) 1)
    return(NULL);

 /* package name, version and timestamp are ignored for now */
  for(i=0; i<=64 && fgetc(rd) != 0; i++) ;
  if(fread(buf, 1, 8, rd) != 8)
    return(NULL);

 /* number of items is not ignored */
  readInt(rd, items, 4);

 /* and now start reading the directory items */
  level = 0;
  prev = NULL;
  for(item=0; item<items; item++) {
    unsigned int curlev;
    unsigned char name[8192];
    int ch, flags;
    
    readInt(rd, curlev, 2);
    flags = fgetc(rd);

    if(flags == EOF || (prev && curlev == 0) || (!prev && curlev != 0) || curlev > level+1)
      return(NULL);

    cur = calloc(sizeof(struct dir), 1);
    if(!prev)
      parent = cur;
    else if(curlev > level) {
      prev->sub = cur;
      cur->parent = prev;
    } else {
      for(i=level; i>curlev; i--)
        prev = prev->parent;
      prev->next = cur;
      cur->parent = prev->parent;
    }

   /* flags - again, the slow but portable way */
    if(flags & EF_DIR)   cur->flags |= FF_DIR;
    if(flags & EF_FILE)  cur->flags |= FF_FILE;
    if(flags & EF_OTHFS) cur->flags |= FF_OTHFS;
    if(flags & EF_EXL)   cur->flags |= FF_EXL;
    if(flags & EF_ERR) {
      cur->flags |= FF_ERR;
      for(tmp = cur->parent; tmp != NULL; tmp = tmp->parent)
        tmp->flags |= FF_SERR;
    }

   /* sizes */
    readInt(rd, cur->size, 8);
    readInt(rd, cur->asize, 8);

   /* name */
    for(i=0; i<8192; i++) {
      ch = fgetc(rd);
      if(ch == EOF)
        return(NULL);
      name[i] = (unsigned char) ch;
      if(ch == 0)
        break;
    }
    cur->name = malloc(i+1);
    strcpy(cur->name, name);

   /* update 'items' of parent dirs */
    if(!(cur->flags & FF_EXL))
      for(tmp = cur->parent; tmp != NULL; tmp = tmp->parent)
        tmp->items++;

    level = curlev;
    prev = cur;
  }
  
  return(parent);
}


struct dir *showImport(char *path) {
  struct dir *ret;

  nccreate(3, 60, "Importing...");
  ncprint(1, 2, "Importing '%s'...", cropdir(path, 43));
  refresh();
  sleep(2);

  ret = importFile(path);
  if(ret)
    return(ret);

  if(s_export) {
    printf("Error importing '%s'\n", path);
    exit(1);
  }

 /* show an error message */
  nccreate(5, 60, "Error...");
  ncprint(1, 2, "Can't import '%s'", cropdir(path, 43));
  ncprint(3, 3, "press any key to continue...");
  getch();

  return(NULL);
}

