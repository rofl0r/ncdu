/* ncdu - NCurses Disk Usage 
    
  Copyright (c) 2007-2008 Yoran Heling

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


int settingsGet(void) {
  WINDOW *set;
  FORM *setf;
  FIELD *fields[11];
  int w, h, cx, cy, i, j, ch;
  int fw, fh, fy, fx, fnrow, fnbuf;
  char tmp[10], *buf = "", rst = 0;

  if(!sdir[0])
    getcwd(sdir, PATH_MAX);

  erase();
  refresh();
                    /*  h,  w, y,  x  */
  fields[0] = new_field(1, 10, 0,  0, 0, 0);
  fields[1] = new_field(1, 43, 0, 11, 0, 0);
  fields[2] = new_field(1, 16, 1, 11, 0, 0);
  fields[3] = new_field(1,  1, 1, 27, 0, 0);
  fields[4] = new_field(1,  1, 1, 28, 0, 0);
  fields[5] = new_field(1,  6, 3, 11, 0, 0);
  fields[6] = new_field(1,  9, 3, 19, 0, 0);
  fields[7] = NULL;

 /* Directory */
  field_opts_off(fields[0], O_ACTIVE);
  set_field_buffer(fields[0], 0, "Directory:");
  set_field_back(fields[1], A_UNDERLINE);
  field_opts_off(fields[1], O_STATIC);
  field_opts_off(fields[1], O_AUTOSKIP);
  set_max_field(fields[1], PATH_MAX);
  set_field_buffer(fields[1], 0, sdir);
 /* One filesystem */
  field_opts_off(fields[2], O_ACTIVE);
  set_field_buffer(fields[2], 0, "One filesystem [");
  field_opts_off(fields[3], O_AUTOSKIP);
  set_field_back(fields[3], A_UNDERLINE);
  set_field_buffer(fields[3], 0, sflags & SF_SMFS ? "X" : " ");
  field_opts_off(fields[4], O_ACTIVE);
  set_field_buffer(fields[4], 0, "]");
 /* buttons */
  set_field_buffer(fields[5], 0, "[OK]");
  set_field_buffer(fields[6], 0, "[CLOSE]");

  setf = new_form(fields);
  h=8;w=60;

  set = newwin(h, w, winrows/2 - h/2, wincols/2 - w/2);
  keypad(stdscr, TRUE);
  keypad(set, TRUE);
  box(set, 0, 0);
  curs_set(1);

  set_form_win(setf, set);
  set_form_sub(setf, derwin(set, h-3, w-4, 2, 2));
  
  wattron(set, A_BOLD);
  mvwaddstr(set, 0, 4, "Calculate disk space usage...");
  wattroff(set, A_BOLD);
  post_form(setf);
  refresh();
  wrefresh(set);

  while((ch = wgetch(set))) {
    getyx(set, cy, cx);
    cy-=2; cx-=2;
    for(i=field_count(setf); --i>=0; ) {
      field_info(fields[i], &fh, &fw, &fy, &fx, &fnrow, &fnbuf);
      if(cy >= fy && cy < fy+fh && cx >= fx && cx < fx+fw) {
        buf = field_buffer(fields[i], 0);
        break;
      }
    }
    switch(ch) {
      case KEY_BACKSPACE:
      case 127:           form_driver(setf, REQ_DEL_PREV);   break;
      case KEY_LL: 
      case KEY_END:       form_driver(setf, REQ_END_LINE);   break;
      case KEY_HOME:      form_driver(setf, REQ_BEG_LINE);   break;
      case KEY_LEFT:      form_driver(setf, REQ_LEFT_CHAR);  break;
      case KEY_RIGHT:
        if(i == 1) {
          for(j=strlen(buf);--j>i;)
            if(buf[j] != ' ')
              break;
          if(j < fw && cx > fx+j)
            break;
        }
        form_driver(setf, REQ_RIGHT_CHAR);
        break;
      case KEY_DC:        form_driver(setf, REQ_DEL_CHAR);   break;
      case KEY_DOWN:      form_driver(setf, REQ_NEXT_FIELD); break;
      case KEY_UP:        form_driver(setf, REQ_PREV_FIELD); break;
      case '\t':          form_driver(setf, REQ_NEXT_FIELD); break;
      case KEY_RESIZE:    rst = 1; goto setend; break;
      default:
        if(i == 6) {
          rst = 2;
          goto setend;
        }
        if(i == 5 || ch == '\n')
          goto setend;
        if(i == 3)
          set_field_buffer(fields[i], 0, buf[0] == ' ' ? "X" : " ");
        else if(!isprint(ch)) break;
        else if(i == 6) {
          if(!isdigit(ch)) strcpy(tmp, " 0");
          else if(buf[0] != ' ' || buf[1] == ' ' || buf[1] == '0') sprintf(tmp, " %c", ch);
          else sprintf(tmp, "%c%c", buf[1], ch);
          set_field_buffer(fields[i], 0, tmp);
        } else
          form_driver(setf, ch);
        break;
    }
    wrefresh(set);
  }
  setend:
 /* !!!WARNING!!! ugly hack !!!WARNING!!! */
  set_current_field(setf, fields[1]);
  form_driver(setf, REQ_END_LINE);
  for(i=0; i<40; i++)
    form_driver(setf, ' ');
  dynamic_field_info(fields[1], &fh, &fw, &fx);
  memcpy(sdir, field_buffer(fields[1], 0), fw);
  for(i=strlen(sdir); --i>=0;)
    if(sdir[i] != ' ' && (sdir[i] != '/' || i == 0)) {
      sdir[i+1] = 0;
      break;
    }
 /* EOW */
  sflags = sflags & SF_IGNS;
  buf = field_buffer(fields[3], 0);
  if(buf[0] != ' ') sflags |= SF_SMFS;
  
  unpost_form(setf);
  for(i=7;--i>=0;)
    free_field(fields[i]);
  werase(set);
  delwin(set);
  erase();
  refresh();
  curs_set(0);
  return(rst);
}

int settingsWin(void) {
  int r;
  while((r = settingsGet()) == 1) {
    ncresize();
    return(settingsWin());
  }
  return(r);
}
