// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
// 

#ifndef Fl_TTY_WIDGET
#define Fl_TTY_WIDGET

#include <stdlib.h>
#include <string.h>
#include <FL/fl_draw.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Scrollbar.H>

#include "ui/strlib.h"
#include "platform/fltk/utils.h"

#define SCROLL_W 15
#define SCROLL_H 15
#define HSCROLL_W 80

using namespace strlib;

struct Point {
  int x;
  int y;
};

struct TtyTextSeg {
  enum {
    BOLD = 0x00000001,
    ITALIC = 0x00000002,
    UNDERLINE = 0x00000004,
    INVERT = 0x00000008,
  };

  // create a new segment
  TtyTextSeg() {
    this->str = 0;
    this->flags = 0;
    this->color = 0;
    this->next = 0;
  } 
  
  ~TtyTextSeg() {
    if (str) {
      delete[]str;
    }
  }

  // reset all flags
  void reset() {
    set(BOLD, false);
    set(ITALIC, false);
    set(UNDERLINE, false);
    set(INVERT, false);
  }

  void setText(const char *str, int n) {
    if ((!str || !n)) {
      this->str = 0;
    } else {
      this->str = new char[n + 1];
      strncpy(this->str, str, n);
      this->str[n] = 0;
    }
  }

  // create a string of n spaces
  void tab(int n) {
    this->str = new char[n + 1];
    memset(this->str, ' ', n);
    this->str[n] = 0;
  }

  // set the flag value
  void set(int f, bool value) {
    if (value) {
      flags |= f;
    } else {
      flags &= ~f;
    }
    flags |= (f << 16);
  }

  // return whether the flag was set (to true or false)
  bool set(int f) {
    return (flags & (f << 16));
  }

  // return the flag value if set, otherwise return value
  bool get(int f, bool *value) {
    bool result = *value;
    if (flags & (f << 16)) {
      result = (flags & f);
    }
    return result;
  }

  // width of this segment in pixels
  int width() {
    return !str ? 0 : fl_width(str);
  }

  // number of chars in this segment
  int numChars() {
    return !str ? 0 : strlen(str);
  }

  // update font and state variables when set in this segment
  bool escape(bool *bold, bool *italic, bool *underline, bool *invert) {
    *bold = get(BOLD, bold);
    *italic = get(ITALIC, italic);
    *underline = get(UNDERLINE, underline);
    *invert = get(INVERT, invert);

    if (this->color) {
      fl_color(this->color);
    }

    return set(BOLD) || set(ITALIC);
  }

  char *str;
  int flags;
  Fl_Color color;
  TtyTextSeg *next;
};

struct TtyRow {
  TtyRow() : head(0) {
  } 
  ~TtyRow() {
    clear();
  }

  // append a segment to this row
  void append(TtyTextSeg *node) {
    if (!head) {
      head = node;
    } else {
      tail(head)->next = node;
    }
    node->next = 0;
  }

  // clear the contents of this row
  void clear() {
    remove(head);
    head = 0;
  }

  // number of characters in this row
  int numChars() {
    return numChars(this->head);
  }

  int numChars(TtyTextSeg *next) {
    int n = 0;
    if (next) {
      n = next->numChars() + numChars(next->next);
    }
    return n;
  }

  void remove(TtyTextSeg *next) {
    if (next) {
      remove(next->next);
      delete next;
    }
  }

  // move to the tab position
  void tab() {
    int tabSize = 6;
    int num = numChars(this->head);
    int pos = tabSize - (num % tabSize);
    if (pos) {
      TtyTextSeg *next = new TtyTextSeg();
      next->tab(pos);
      append(next);
    }
  }

  TtyTextSeg *tail(TtyTextSeg *next) {
    return !next->next ? next : tail(next->next);
  }

  int width() {
    return width(this->head);
  }

  int width(TtyTextSeg *next) {
    int n = 0;
    if (next) {
      n = next->width() + width(next->next);
    }
    return n;
  }

  TtyTextSeg *head;
};

struct TtyWidget : public Fl_Group {
  TtyWidget(int x, int y, int w, int h, int numRows);
  virtual ~TtyWidget();

  // inherited methods
  void draw();
  int handle(int e);
  void layout();

  // public api
  void clearScreen();
  bool copySelection();
  void print(const char *str);
  void setFont(Fl_Font font) {
    fl_font(font, 0);
    redraw();
  };
  void setFontSize(int size) {
    fl_font(0, size);
    redraw();
  };
  void setScrollLock(bool b) {
    scrollLock = b;
  };

private:
  void drawSelection(TtyTextSeg *seg, strlib::String *s, int row, int x, int y);
  TtyRow *getLine(int ndx);
  int processLine(TtyRow *line, const char *linePtr);
  void setfont(bool bold, bool italic);
  void setfont(Fl_Font font, int size);
  void setGraphicsRendition(TtyTextSeg *segment, int c);

  // returns the number of display text rows held in the buffer
  int getTextRows() {
    return 1 + ((head >= tail) ? (head - tail) : head + (rows - tail));
  }

  // returns the number of rows available for display
  int getPageRows() {
    return (h() - 1) / lineHeight;
  }

  // returns the selected row within the circular buffer
  int rowEvent() {
    return (Fl::event_y() / lineHeight) + tail + vscrollbar->value();
  }

  // buffer management
  TtyRow *buffer;
  int head;                     // current head of buffer
  int tail;                     // buffer last line
  int rows;                     // total number of rows - size of buffer
  int cols;                     // maximum number of characters in a row
  int width;                    // the maximum width of the buffer text in pixels

  // scrollbars
  Fl_Scrollbar *vscrollbar;
  Fl_Scrollbar *hscrollbar;
  int lineHeight;
  bool scrollLock;

  // clipboard handling
  int markX, markY, pointX, pointY;
};

#endif
