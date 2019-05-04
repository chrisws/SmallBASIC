// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef DISPLAY_H
#define DISPLAY_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>
#include <FL/fl_draw.H>

#include "lib/maapi.h"
#include "ui/strlib.h"
#include "ui/graphics.h"
#include "ui/ansiwidget.h"

class GraphicsWidget : public Fl_Widget, ui::Graphics {
public:
  GraphicsWidget(int x, int y, int w, int h, int defsize);
  virtual ~GraphicsWidget();

  bool construct(const char *font, const char *boldFont);
  void redraw();
  void resize(int w, int h);
  void setFontSize(int size) { _ansiWidget->setFontSize(size); }

  AnsiWidget *_ansiWidget;
  
private:
  void draw();
  void layout();
  void buttonClicked(const char *action);
  bool loadFonts(const char *font, const char *boldFont);
  bool loadFont(const char *filename, FT_Face &face);

  int _defsize;
};

#endif
