// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef FLTK_SYSTEM_H
#define FLTK_SYSTEM_H

#include "ui/ansiwidget.h"

struct System {
  System(int w, int h, int defSize);
  ~System();

  void alert(const char *title, const char *message);
  int ask(const char *title, const char *prompt, bool cancel=true);
  void browseFile(const char *url);
  char *getClipboardText();
  AnsiWidget *getOutput();
  bool isBreak();
  bool isRunning();
  void optionsBox(StringList *items);
  MAEvent processEvents(bool wait);
  void setClipboardText(const char *text);
  void setFontSize(int size);
  void setLoadBreak(const char *url) {}
  void setLoadPath(const char *url) {}
  void setWindowSize(int width, int height);
  void systemPrint(const char *message, ...);
};

#endif
