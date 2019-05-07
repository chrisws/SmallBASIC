// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include <time.h>
#include "ui/utils.h"
#include "platform/fltk/display.h"
#include "platform/fltk/system.h"

extern ui::Graphics *graphics;
extern System *g_system;

// TODO: can this be replaced with: Fl_Image_Surface.H

//
// Canvas implementation
//
Canvas::Canvas() :
  _w(0),
  _h(0),
  _pixels(NULL),
  _img(NULL),
  _clip(NULL) {
}

Canvas::~Canvas() {
  delete [] _pixels;
  delete _img;
  delete _clip;
  _pixels = NULL;
  _img = NULL;
  _clip = NULL;
}

bool Canvas::create(int w, int h) {
  logEntered();
  bool result;
  _w = w;
  _h = h;
  _pixels = new pixel_t[w * h];
  _img = new Fl_RGB_Image((const uchar *)_pixels, w, h, 4, 0);
  if (_pixels && _img) {
    memset(_pixels, 0, w * h);
    result = true;
  } else {
    result = false;
  }
  return result;
}

void Canvas::drawRegion(Canvas *src, const MARect *srcRect, int destX, int destY) {
  int srcH = srcRect->height;
  if (srcRect->top + srcRect->height > src->_h) {
    srcH = src->_h - srcRect->top;
  }
  for (int y = 0; y < srcH && destY < _h; y++, destY++) {
    pixel_t *line = src->getLine(y + srcRect->top) + srcRect->left;
    pixel_t *dstLine = getLine(destY) + destX;
    memcpy(dstLine, line, srcRect->width * sizeof(pixel_t));
  }
}

void Canvas::fillRect(int left, int top, int width, int height, pixel_t drawColor) {
  int dtX = x();
  int dtY = y();
  uint8_t dR, dG, dB;

  GET_RGB(drawColor, dR, dG, dB);
  if (left == 0 && _w == width && top < _h && top > -1 &&
      dR == dG && dR == dB) {
    // contiguous block of uniform colour
    unsigned blockH = height;
    if (top + height > _h) {
      blockH = height - top;
    }
    memset(getLine(top), drawColor, 4 * width * blockH);
  } else {
    for (int y = 0; y < height; y++) {
      int posY = y + top;
      if (posY == _h) {
        break;
      } else if (posY >= dtY) {
        pixel_t *line = getLine(posY);
        for (int x = 0; x < width; x++) {
          int posX = x + left;
          if (posX == _w) {
            break;
          } else if (posX >= dtX) {
            line[posX] = drawColor;
          }
        }
      }
    }
  }
}

void Canvas::setClip(int x, int y, int w, int h) {
  delete _clip;
  if (x != 0 || y != 0 || _w != w || _h != h) {
    _clip = new Fl_Rect(x, y, x + w, y + h);
  } else {
    _clip = NULL;
  }
}

//
// Graphics implementation
//
GraphicsWidget::GraphicsWidget(int x, int y, int w, int h, int defsize) :
  Fl_Widget(x, y, w, h), ui::Graphics(), _defsize(defsize) {
}

GraphicsWidget::~GraphicsWidget() {
  delete _ansiWidget;
  delete _screen;
}

bool GraphicsWidget::construct(const char *font, const char *boldFont) {
  logEntered();

  bool result = loadFonts(font, boldFont);
  if (result) {
    _screen = new Canvas();
    _ansiWidget = new AnsiWidget(w(), h());
    g_system = new System(_ansiWidget);
    if (_screen != NULL && _ansiWidget != NULL && g_system != NULL) {
      result = _screen->create(w(), h()) && _ansiWidget->construct();
      _ansiWidget->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
      _ansiWidget->setFontSize(_defsize);
      maSetColor(DEFAULT_BACKGROUND);
    }
  } else {
    result = false;
  }
  return result;
}

void GraphicsWidget::draw() {
  if (_screen && _screen->_img) {
    _screen->_img->draw(x(), y(), w(), h());
  } else {
    fl_color(fl_rgb_color(128, 128, 200));
    fl_rectf(x(), y(), w(), h());
  }
}

void GraphicsWidget::redraw() {
  Fl_Widget::redraw();
}

void GraphicsWidget::resize(int w, int h) {
  logEntered();
  bool drawScreen = (_drawTarget == _screen);
  delete _screen;
  _screen = new ::Canvas();
  _screen->create(w, h);
  _drawTarget = drawScreen ? _screen : NULL;
}

bool GraphicsWidget::loadFonts(const char *font, const char *boldFont) {
  return (!FT_Init_FreeType(&_fontLibrary) &&
          loadFont(font, _fontFace) &&
          loadFont(boldFont, _fontFaceB));
}

bool GraphicsWidget::loadFont(const char *filename, FT_Face &face) {
  bool result = !FT_New_Face(_fontLibrary, filename, 0, &face);
  trace("load font %s = %d", filename, result);
  return result;
}
