// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "utils.h"
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#if defined(WIN32)
#include <windows.h>
#endif

extern "C" void trace(const char *format, ...) {
  char buf[4096], *p = buf;
  va_list args;

  va_start(args, format);
  p += vsnprintf(p, sizeof buf - 1, format, args);
  va_end(args);

  while (p > buf && isspace(p[-1])) {
    *--p = '\0';
  }

  *p++ = '\r';
  *p++ = '\n';
  *p = '\0';
#if defined(WIN32)
  OutputDebugString(buf);
#else
  fprintf(stderr, buf, 0);
#endif
}

/**
 * Set r,g,b to the 8-bit components of this color. If it is an indexed
 * color they are looked up in the table, otherwise they are simply copied
 * out of the color number.
 */
void split_color(Fl_Color i, uint8_t& r, uint8_t& g, uint8_t& b) {
  uint8_t c = Fl::get_color(i);
  r = c >> 24;
  g = c >> 16;
  b = c >> 8;
}
