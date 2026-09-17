/******************************************************************************
 * $Id$
 *
 * Project:  MapServer
 * Purpose:  Regex wrapper
 * Author:   Bill Binko
 *
 ******************************************************************************
 * Copyright (c) 1996-2005 Regents of the University of Minnesota.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies of this Software or works derived from this Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

/* we can't include mapserver.h, so we need our own basics */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32) && !defined(__CYGWIN__)
#include <direct.h>
#include <memory.h>
#include <malloc.h>
#else
#include <unistd.h>
#endif

/*Need to specify this so that mapregex.h doesn't defined constants and
  doesn't #define away our ms_*/

#if defined(_WIN32) && !defined(__CYGWIN__) && _MSC_VER < 1900
#define off_t long
#endif

#include "mapserver.h"
#include "mapregex.h"

#ifdef USE_PCRE2
#include <pcre2posix.h>
#else
#include <regex.h>
#endif

#ifdef USE_PCRE2
/* PCRE2 escapes backslash inside [...]; POSIX regex doesn't. Double
 * lone backslashes in brackets so PCRE2 sees a literal one too.
 * Already-paired "\\" is left alone, so this is idempotent. */
MS_API_EXPORT(char *) msPCRE2EscapeBracketBackslashes(const char *expr) {
  size_t len = strlen(expr);
  char *out = (char *)msSmallMalloc(len * 2 + 1);
  size_t oi = 0;
  int in_bracket = 0;
  size_t bracket_start = 0;

  for (size_t i = 0; i < len;) {
    char c = expr[i];

    if (!in_bracket) {
      if (c == '[') {
        in_bracket = 1;
        out[oi++] = c;
        i++;
        if (i < len && expr[i] == '^') {
          out[oi++] = expr[i];
          i++;
        }
        bracket_start = i;
        continue;
      }
      out[oi++] = c;
      i++;
      continue;
    }

    if (c == ']' && i != bracket_start) {
      in_bracket = 0;
      out[oi++] = c;
      i++;
      continue;
    }

    /* [:class:], [.x.], [=x=] - pass through untouched */
    if (c == '[' && i + 1 < len &&
        (expr[i + 1] == ':' || expr[i + 1] == '.' || expr[i + 1] == '=')) {
      char sub = expr[i + 1];
      out[oi++] = expr[i];
      out[oi++] = expr[i + 1];
      i += 2;
      while (i + 1 < len && !(expr[i] == sub && expr[i + 1] == ']')) {
        out[oi++] = expr[i];
        i++;
      }
      if (i + 1 < len) {
        out[oi++] = expr[i];
        out[oi++] = expr[i + 1];
        i += 2;
      }
      continue;
    }

    if (c == '\\') {
      out[oi++] = '\\';
      out[oi++] = '\\';
      if (i + 1 < len && expr[i + 1] == '\\') {
        i += 2; /* already escaped */
      } else {
        i += 1; /* lone backslash - double it */
      }
      continue;
    }

    out[oi++] = c;
    i++;
  }

  out[oi] = '\0';
  return out;
}
#endif

MS_API_EXPORT(int) ms_regcomp(ms_regex_t *regex, const char *expr, int cflags) {
  /* Must free in regfree() */
  regex_t *sys_regex = (regex_t *)msSmallMalloc(sizeof(regex_t));
  regex->sys_regex = (void *)sys_regex;
  int reg_cflags = 0;
  if (cflags & MS_REG_EXTENDED)
    reg_cflags |= REG_EXTENDED;
  if (cflags & MS_REG_ICASE)
    reg_cflags |= REG_ICASE;
  if (cflags & MS_REG_NOSUB)
    reg_cflags |= REG_NOSUB;
  if (cflags & MS_REG_NEWLINE)
    reg_cflags |= REG_NEWLINE;
#ifdef USE_PCRE2
  char *escaped_expr = msPCRE2EscapeBracketBackslashes(expr);
  int ret = regcomp(sys_regex, escaped_expr, reg_cflags);
  free(escaped_expr);
#else
  int ret = regcomp(sys_regex, expr, reg_cflags);
#endif
  if (ret != 0) {
    free(regex->sys_regex);
    regex->sys_regex = NULL;
  }
  return ret;
}

MS_API_EXPORT(size_t)
ms_regerror(int errcode, const ms_regex_t *regex, char *errbuf,
            size_t errbuf_size) {
  return regerror(errcode, (regex_t *)(regex->sys_regex), errbuf, errbuf_size);
}

MS_API_EXPORT(int)
ms_regexec(const ms_regex_t *regex, const char *string, size_t nmatch,
           ms_regmatch_t pmatch[], int eflags) {
  /*This next line only works because we know that regmatch_t
    and ms_regmatch_t are exactly alike (POSIX STANDARD)*/
  return regexec((const regex_t *)(regex->sys_regex), string, nmatch,
                 (regmatch_t *)pmatch, eflags);
}

MS_API_EXPORT(void) ms_regfree(ms_regex_t *regex) {
  regfree((regex_t *)(regex->sys_regex));
  free(regex->sys_regex);
  return;
}
