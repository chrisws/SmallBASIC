// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
// 

#ifndef FLTK_UTILS_H
#define FLTK_UTILS_H

int x_get_pixel(int x, int y);

#define DAMAGE_HIGHLIGHT FL_DAMAGE_USER1
#define DAMAGE_PUSHED    FL_DAMAGE_USER2

#define C_LINKAGE_BEGIN extern "C" {
#define C_LINKAGE_END }

#ifndef MAX
#define MAX(a,b) ((a<b) ? (b) : (a))
#endif
#ifndef MIN
#define MIN(a,b) ((a>b) ? (b) : (a))
#endif

#if defined(__MINGW32__)
#define makedir(f) mkdir(f)
#else
#define makedir(f) mkdir(f, 0700)
#endif

#endif
