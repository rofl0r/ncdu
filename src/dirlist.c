/* ncdu - NCurses Disk Usage

  Copyright (c) 2007-2010 Yoran Heling

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
#include <string.h>


/* public variables */
compll_t dirlist_parent    = (compll_t)0;
off_t    dirlist_maxs      = 0,
         dirlist_maxa      = 0;

int      dirlist_sort_desc = 1,
         dirlist_sort_col  = DL_COL_SIZE,
         dirlist_sort_df   = 0,
         dirlist_hidden    = 0;

/* private state vars */
compll_t dirlist_parent_alloc = (compll_t)0;
compll_t head, head_real, selected, top = (compll_t)0;



#define ISHIDDEN(d) (dirlist_hidden && d != dirlist_parent && (\
    DR(d)->flags & FF_EXL || DR(d)->name[0] == '.' || DR(d)->name[strlen(DR(d)->name)-1] == '~'\
  ))



int dirlist_cmp(compll_t x, compll_t y) {
  int r;

  /* dirs are always before files when that option is set */
  if(dirlist_sort_df) {
    if(DR(y)->flags & FF_DIR && !(DR(x)->flags & FF_DIR))
      return 1;
    else if(!(DR(y)->flags & FF_DIR) && DR(x)->flags & FF_DIR)
      return -1;
  }

  /* sort columns:
   *           1   ->   2   ->   3
   *   NAME: name  -> size  -> asize
   *   SIZE: size  -> asize -> name
   *  ASIZE: asize -> size  -> name
   *
   * Note that the method used below is supposed to be fast, not readable :-)
   */
#define CMP_NAME  strcmp(DR(x)->name, DR(y)->name)
#define CMP_SIZE  (DR(x)->size  > DR(y)->size  ? 1 : (DR(x)->size  == DR(y)->size  ? 0 : -1))
#define CMP_ASIZE (DR(x)->asize > DR(y)->asize ? 1 : (DR(x)->asize == DR(y)->asize ? 0 : -1))

  /* try 1 */
  r = dirlist_sort_col == DL_COL_NAME ? CMP_NAME : dirlist_sort_col == DL_COL_SIZE ? CMP_SIZE : CMP_ASIZE;
  /* try 2 */
  if(!r)
    r = dirlist_sort_col == DL_COL_SIZE ? CMP_ASIZE : CMP_SIZE;
  /* try 3 */
  if(!r)
    r = dirlist_sort_col == DL_COL_NAME ? CMP_ASIZE : CMP_NAME;

  /* reverse when sorting in descending order */
  if(dirlist_sort_desc && r != 0)
    r = r < 0 ? 1 : -1;

  return r;
}


compll_t dirlist_sort(compll_t list) {
  compll_t p, q, e, tail;
  int insize, nmerges, psize, qsize, i;

  insize = 1;
  while(1) {
    p = list;
    list = (compll_t)0;
    tail = (compll_t)0;
    nmerges = 0;
    while(p) {
      nmerges++;
      q = p;
      psize = 0;
      for(i=0; i<insize; i++) {
        psize++;
        q = DR(q)->next;
        if(!q) break;
      }
      qsize = insize;
      while(psize > 0 || (qsize > 0 && q)) {
        if(psize == 0) {
          e = q; q = DR(q)->next; qsize--;
        } else if(qsize == 0 || !q) {
          e = p; p = DR(p)->next; psize--;
        } else if(dirlist_cmp(p,q) <= 0) {
          e = p; p = DR(p)->next; psize--;
        } else {
          e = q; q = DR(q)->next; qsize--;
        }
        if(tail) DW(tail)->next = e;
        else     list = e;
        DW(e)->prev = tail;
        tail = e;
      }
      p = q;
    }
    DW(tail)->next = (compll_t)0;
    if(nmerges <= 1) {
      if(DR(list)->parent)
        DW(DR(list)->parent)->sub = list;
      return list;
    }
    insize *= 2;
  }
}


/* passes through the dir listing once and:
 * - makes sure one, and only one, visible item is selected
 * - updates the dirlist_(maxs|maxa) values
 * - makes sure that the FF_BSEL bits are correct */
void dirlist_fixup() {
  compll_t t;

  /* we're going to determine the selected items from the list itself, so reset this one */
  selected = (compll_t)0;

  for(t=head; t; t=DR(t)->next) {
    /* not visible? not selected! */
    if(ISHIDDEN(t)) {
      if(DR(t)->flags & FF_BSEL)
        DW(t)->flags &= ~FF_BSEL;
    } else {
      /* visible and selected? make sure only one item is selected */
      if(DR(t)->flags & FF_BSEL) {
        if(!selected)
          selected = t;
        else
          DW(t)->flags &= ~FF_BSEL;
      }
    }

    /* update dirlist_(maxs|maxa) */
    if(DR(t)->size > dirlist_maxs)
      dirlist_maxs = DR(t)->size;
    if(DR(t)->asize > dirlist_maxa)
      dirlist_maxa = DR(t)->asize;
  }

  /* no selected items found after one pass? select the first visible item */
  if(!selected)
    if((selected = dirlist_next((compll_t)0)))
      DW(selected)->flags |= FF_BSEL;
}


void dirlist_open(compll_t d) {
  /* set the head of the list */
  head_real = head = !d ? d : !DR(d)->parent ? DR(d)->sub : DR(DR(d)->parent)->sub;

  /* reset internal status */
  dirlist_maxs = dirlist_maxa = 0;

  /* stop if this is not a directory list we can work with */
  if(!head) {
    dirlist_parent = (compll_t)0;
    return;
  }

  /* sort the dir listing */
  head_real = head = dirlist_sort(head);

  /* allocate reference to parent dir if we don't have one yet */
  if(!dirlist_parent_alloc) {
    dirlist_parent_alloc = compll_alloc(SDIRSIZE+2);
    strcpy(DW(dirlist_parent_alloc)->name, "..");
  }

  /* set the reference to the parent dir */
  DW(dirlist_parent_alloc)->flags &= ~FF_BSEL;
  if(DR(DR(head)->parent)->parent) {
    dirlist_parent = dirlist_parent_alloc;
    DW(dirlist_parent)->next = head;
    DW(dirlist_parent)->parent = DR(head)->parent;
    DW(dirlist_parent)->sub = DR(head)->parent;
    head = dirlist_parent;
  } else
    dirlist_parent = (compll_t)0;

  dirlist_fixup();
}


compll_t dirlist_next(compll_t d) {
  if(!head)
    return (compll_t)0;
  if(!d) {
    if(!ISHIDDEN(head))
      return head;
    else
      d = head;
  }
  while((d = DR(d)->next)) {
    if(!ISHIDDEN(d))
      return d;
  }
  return (compll_t)0;
}


compll_t dirlist_prev(compll_t d) {
  if(!head || !d)
    return (compll_t)0;
  while((d = DR(d)->prev)) {
    if(!ISHIDDEN(d))
      return d;
  }
  if(dirlist_parent)
    return dirlist_parent;
  return (compll_t)0;
}


compll_t dirlist_get(int i) {
  compll_t t = selected, d;

  if(!head)
    return (compll_t)0;

  if(ISHIDDEN(selected)) {
    selected = dirlist_next((compll_t)0);
    return selected;
  }

  /* i == 0? return the selected item */
  if(!i)
    return selected;

  /* positive number? simply move forward */
  while(i > 0) {
    d = dirlist_next(t);
    if(!d)
      return t;
    t = d;
    if(!--i)
      return t;
  }

  /* otherwise, backward */
  while(1) {
    d = dirlist_prev(t);
    if(!d)
      return t;
    t = d;
    if(!++i)
      return t;
  }
}


void dirlist_select(compll_t d) {
  if(!d || !head || ISHIDDEN(d) || DR(d)->parent != DR(head)->parent)
    return;

  DW(selected)->flags &= ~FF_BSEL;
  selected = d;
  DW(selected)->flags |= FF_BSEL;
}



/* We need a hint in order to figure out which item should be on top:
 *  0 = only get the current top, don't set anything
 *  1 = selected has moved down
 * -1 = selected has moved up
 * -2 = selected = first item in the list (faster version of '1')
 * -3 = top should be considered as invalid (after sorting or opening an other dir)
 * -4 = an item has been deleted
 * -5 = hidden flag has been changed
 *
 * Actions:
 *  hint = -1 or -4 -> top = selected_is_visible ? top : selected
 *  hint = -2 or -3 -> top = selected-(winrows-3)/2
 *  hint =  1       -> top = selected_is_visible ? top : selected-(winrows-4)
 *  hint =  0 or -5 -> top = selected_is_visible ? top : selected-(winrows-3)/2
 *
 * Regardless of the hint, the returned top will always be chosen such that the
 * selected item is visible.
 */
compll_t dirlist_top(int hint) {
  compll_t t;
  int i = winrows-3, visible = 0;

  if(hint == -2 || hint == -3)
    top = (compll_t)0;

  /* check whether the current selected item is within the visible window */
  if(top) {
    i = winrows-3;
    t = dirlist_get(0);
    while(t && i--) {
      if(t == top) {
        visible++;
        break;
      }
      t = dirlist_prev(t);
    }
  }

  /* otherwise, get a new top */
  if(!visible)
    top = hint == -1 || hint == -4 ? dirlist_get(0) :
          hint ==  1               ? dirlist_get(-1*(winrows-4)) :
                                     dirlist_get(-1*(winrows-3)/2);

  /* also make sure that if the list is longer than the window and the last
   * item is visible, that this last item is also the last on the window */
  t = top;
  i = winrows-3;
  while(t && i--)
    t = dirlist_next(t);
  t = top;
  do {
    top = t;
    t = dirlist_prev(t);
  } while(t && i-- > 0);

  return top;
}


void dirlist_set_sort(int col, int desc, int df) {
  /* update config */
  if(col != DL_NOCHANGE)
    dirlist_sort_col = col;
  if(desc != DL_NOCHANGE)
    dirlist_sort_desc = desc;
  if(df != DL_NOCHANGE)
    dirlist_sort_df = df;

  /* sort the list (excluding the parent, which is always on top) */
  head_real = dirlist_sort(head_real);
  if(dirlist_parent)
    DW(dirlist_parent)->next = head_real;
  else
    head = head_real;
  dirlist_top(-3);
}


void dirlist_set_hidden(int hidden) {
  dirlist_hidden = hidden;
  dirlist_fixup();
  dirlist_top(-5);
}

