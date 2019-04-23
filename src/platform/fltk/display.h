// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
// 

#ifndef DISPLAY_WIDGET_H
#define DISPLAY_WIDGET_H

#include <FL/Fl_Widget.H>
#include <FL/Fl_Image.H>
#include <FL/fl_draw.H>

#include "lib/maapi.h"
#include "ui/inputs.h"
#include "ui/canvas.h"

struct AnsiWidget;

class DisplayWidget : public Fl_Widget {
public:
  DisplayWidget(int x, int y, int w, int h, int defsize);
  virtual ~DisplayWidget();

  void clearScreen();
  void print(const char *str);
  void drawLine(int x1, int y1, int x2, int y2);
  void drawRectFilled(int x1, int y1, int x2, int y2);
  void drawRect(int x1, int y1, int x2, int y2);
  void setTextColor(long fg, long bg);
  void setColor(long color);
  int  getX(bool offset=false);
  int  getY(bool offset=false);
  void setPixel(int x, int y, int c);
  void setXY(int x, int y);
  void setStartSize(int w, int h) { _startW=w; _startH=h; }
  int  textWidth(const char *s);
  int  textHeight(void);
  void setFontSize(float i);
  int  getFontSize();
  int  getBackgroundColor();
  void flush(bool force);
  void reset();
  void setAutoflush(bool autoflush);
  void flushNow();

  Canvas *getScreen() { return _screen; }
  int getDefaultSize() { return _defsize; }
  AnsiWidget *_ansiWidget;

private:
  // inherited methods
  void draw();
  void layout();
  int handle(int e);
  void createScreen();

  void buttonClicked(const char *action);
  Canvas *_screen;
  bool _resized;
  int _defsize;
  int _startW, _startH;
};

#endif
