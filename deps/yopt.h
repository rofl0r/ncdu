/* Copyright (c) 2012-2013 Yoran Heling

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

/* This is a simple command-line option parser. Operation is similar to
 * getopt_long(), except with a cleaner API.
 *
 * This is implemented in a single header file, as it's pretty small and you
 * generally only use an option parser in a single .c file in your program.
 *
 * Supports (examples from GNU tar(1)):
 *   "--gzip"
 *   "--file <arg>"
 *   "--file=<arg>"
 *   "-z"
 *   "-f <arg>"
 *   "-f<arg>"
 *   "-zf <arg>"
 *   "-zf<arg>"
 *   "--" (To stop looking for futher options)
 *   "<arg>" (Non-option arguments)
 *
 * Issues/non-features:
 * - An option either requires an argument or it doesn't.
 * - No way to specify how often an option can/should be used.
 * - No way to specify the type of an argument (filename/integer/enum/whatever)
 */


#ifndef YOPT_H
#define YOPT_H


#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>


typedef struct {
	/* Value yopt_next() will return for this option */
	int val;
	/* Whether this option needs an argument */
	int needarg;
	/* Name(s) of this option, prefixed with '-' or '--' and separated by a
	 * comma. E.g. "-z", "--gzip", "-z,--gzip".
	 * An option can have any number of aliasses.
	 */
	const char *name;
} yopt_opt_t;


typedef struct {
	int argc;
	int cur;
	int argsep; /* '--' found */
	char **argv;
	char *sh;
	const yopt_opt_t *opts;
	char errbuf[128];
} yopt_t;


/* opts must be an array of options, terminated with an option with val=0 */
static inline void yopt_init(yopt_t *o, int argc, char **argv, const yopt_opt_t *opts) {
	o->argc = argc;
	o->argv = argv;
	o->opts = opts;
	o->cur = 0;
	o->argsep = 0;
	o->sh = NULL;
}


static inline const yopt_opt_t *_yopt_find(const yopt_opt_t *o, const char *v) {
	const char *tn, *tv;

	for(; o->val; o++) {
		tn = o->name;
		while(*tn) {
			tv = v;
			while(*tn && *tn != ',' && *tv && *tv != '=' && *tn == *tv) {
				tn++;
				tv++;
			}
			if(!(*tn && *tn != ',') && !(*tv && *tv != '='))
				return o;
			while(*tn && *tn != ',')
				tn++;
			while(*tn == ',')
				tn++;
		}
	}

	return NULL;
}


static inline int _yopt_err(yopt_t *o, char **val, const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vsnprintf(o->errbuf, sizeof(o->errbuf), fmt, va);
	va_end(va);
	*val = o->errbuf;
	return -2;
}


/* Return values:
 *  0 -> Non-option argument, val is its value
 * -1 -> Last argument has been processed
 * -2 -> Error, val will contain the error message.
 *  x -> Option with val = x found. If the option requires an argument, its
 *       value will be in val.
 */
static inline int yopt_next(yopt_t *o, char **val) {
	const yopt_opt_t *opt;
	char sh[3];

	*val = NULL;
	if(o->sh)
		goto inshort;

	if(++o->cur >= o->argc)
		return -1;

	if(!o->argsep && o->argv[o->cur][0] == '-' && o->argv[o->cur][1] == '-' && o->argv[o->cur][2] == 0) {
		o->argsep = 1;
		if(++o->cur >= o->argc)
			return -1;
	}

	if(o->argsep || *o->argv[o->cur] != '-') {
		*val = o->argv[o->cur];
		return 0;
	}

	if(o->argv[o->cur][1] != '-') {
		o->sh = o->argv[o->cur]+1;
		goto inshort;
	}

	/* Now we're supposed to have a long option */
	if(!(opt = _yopt_find(o->opts, o->argv[o->cur])))
		return _yopt_err(o, val, "Unknown option '%s'", o->argv[o->cur]);
	if((*val = strchr(o->argv[o->cur], '=')) != NULL)
		(*val)++;
	if(!opt->needarg && *val)
		return _yopt_err(o, val, "Option '%s' does not accept an argument", o->argv[o->cur]);
	if(opt->needarg && !*val) {
		if(o->cur+1 >= o->argc)
			return _yopt_err(o, val, "Option '%s' requires an argument", o->argv[o->cur]);
		*val = o->argv[++o->cur];
	}
	return opt->val;

	/* And here we're supposed to have a short option */
inshort:
	sh[0] = '-';
	sh[1] = *o->sh;
	sh[2] = 0;
	if(!(opt = _yopt_find(o->opts, sh)))
		return _yopt_err(o, val, "Unknown option '%s'", sh);
	o->sh++;
	if(opt->needarg && *o->sh)
		*val = o->sh;
	else if(opt->needarg) {
		if(++o->cur >= o->argc)
			return _yopt_err(o, val, "Option '%s' requires an argument", sh);
		*val = o->argv[o->cur];
	}
	if(!*o->sh || opt->needarg)
		o->sh = NULL;
	return opt->val;
}


#endif

/* vim: set noet sw=4 ts=4: */
