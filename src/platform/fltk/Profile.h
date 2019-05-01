// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef PROFILE_H
#define PROFILE_H

#include <FL/Fl_Rect.H>

#include "ui/strlib.h"

using namespace strlib;

struct MainWindow;
struct EditorWidget;

struct Profile {
  Profile();

  void loadConfig(EditorWidget *editor);
  void restore(MainWindow *wnd);
  void restoreAppPosition(Fl_Window *wnd);
  void save(MainWindow *wnd);

  Fl_Color _color;
  Fl_Font _font;
  Fl_Rect _appPosition;

  int _fontSize;
  int _indentLevel;
  int _createBackups;
  int _lineNumbers;

private:
  bool _loaded;
  int nextInteger(const char *s, int len, int &index);
  Fl_Rect restoreRect(Properties<String *> *profile, const char *key);
  void restoreStyles(Properties<String *> *profile);
  void restoreTabs(MainWindow *wnd, Properties<String *> *profile);
  void restoreValue(Properties<String *> *profile, const char *key, int *value);
  void restoreWindowPos(MainWindow *wnd, Fl_Rect &rc);
  void saveRect(FILE *fp, const char *key, Fl_Rect *wnd);
  void saveStyles(FILE *fp);
  void saveTabs(FILE *fp, MainWindow *wnd);
  void saveValue(FILE *fp, const char *key, const char *value);
  void saveValue(FILE *fp, const char *key, int value);
};

#endif

