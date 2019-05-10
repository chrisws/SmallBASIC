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

extern System *g_system;
GraphicsWidget *graphics;

#define MAX_CANVAS_SIZE 20
#define DEF_BACKGROUND fl_rgb_color(31, 28, 31)

//
// Canvas implementation
//
Canvas::Canvas() :
  _w(0),
  _h(0),
  _scale(0),
  _offscreen(0),
  _clip(NULL),
  _drawColor(FL_BLACK),
  _font(NULL) {
}

Canvas::~Canvas() {
  if (_offscreen) {
    fl_delete_offscreen(_offscreen);
    _offscreen = 0;
  }
  delete _clip;
  _clip = NULL;
}

bool Canvas::create(int w, int h) {
  logEntered();
  _w = w;
  _h = h;
  _offscreen = fl_create_offscreen(_w, _h);
  _scale = Fl_Graphics_Driver::default_driver().scale();
  fillRect(x(), y(), w, h, DEF_BACKGROUND);
  return _offscreen != 0;
}

void Canvas::deleteFont(Font *font) {
  if (font == _font) {
    _font = NULL;
  }
  delete font;
}

void Canvas::drawArc(int xc, int yc, double r, double start, double end, double aspect) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawEllipse(int xc, int yc, int rx, int ry, bool fill) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawLine(int startX, int startY, int endX, int endY) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawPixel(int posX, int posY) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawRGB(const MAPoint2d *dstPoint, const void *src, const MARect *srcRect, int opacity, int bytesPerLine) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawRegion(Canvas *src, const MARect *srcRect, int destX, int destY) {
  fl_begin_offscreen(_offscreen);
  int width = MIN(_w, srcRect->width);
  int height = MIN(_h, srcRect->height);
  fl_copy_offscreen(destX, destY, width, height, src->_offscreen, srcRect->left, srcRect->top);
  fl_end_offscreen();
}

void Canvas::drawText(int left, int top, const char *str, int len) {
  fl_begin_offscreen(_offscreen);
  fl_color(_drawColor);
  fl_draw(str, len, x() + left, y() + top);
  fl_end_offscreen();
}

void Canvas::fillRect(int left, int top, int width, int height, Fl_Color color) {
  fl_begin_offscreen(_offscreen);
  fl_color(color);
  fl_rectf(left, top, width, height);
  fl_end_offscreen();
}

void Canvas::getImageData(Canvas *canvas, uint8_t *image, const MARect *srcRect, int bytesPerLine) {

}

int Canvas::getPixel(int x, int y) {
  return 0;
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
  Fl_Widget(x, y, w, h),
  _defsize(defsize),
  _screen(NULL),
  _drawTarget(NULL) {
  logEntered();
  graphics = this;
  _screen = new Canvas();
  _ansiWidget = new AnsiWidget(w, h);
  g_system = new System(_ansiWidget);
  if (_screen != NULL && _ansiWidget != NULL && g_system != NULL) {
    _screen->create(w, h);
    _ansiWidget->construct();
    _ansiWidget->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
    _ansiWidget->setFontSize(_defsize);
  }
}

GraphicsWidget::~GraphicsWidget() {
  delete _ansiWidget;
  delete _screen;
}

void GraphicsWidget::draw() {
  if (_screen && _screen->_offscreen) {
    if (_screen->_scale != Fl_Graphics_Driver::default_driver().scale()) {
      fl_rescale_offscreen(_screen->_offscreen);
      _screen->_scale = Fl_Graphics_Driver::default_driver().scale();
    }
    fl_copy_offscreen(x(), y(), w(), h(), _screen->_offscreen, 0, 0);
  }
}

void GraphicsWidget::resize(int w, int h) {
  logEntered();
  bool drawScreen = (_drawTarget == _screen);
  delete _screen;
  _screen = new ::Canvas();
  _screen->create(w, h);
  _drawTarget = drawScreen ? _screen : NULL;
}

MAHandle GraphicsWidget::setDrawTarget(MAHandle maHandle) {
  MAHandle result = (MAHandle) _drawTarget;
  if (maHandle == (MAHandle) HANDLE_SCREEN ||
      maHandle == (MAHandle) HANDLE_SCREEN_BUFFER) {
    _drawTarget = _screen;
  } else {
    _drawTarget = (Canvas *)maHandle;
  }
  delete _drawTarget->_clip;
  _drawTarget->_clip = NULL;
  return result;
}

//
// maapi implementation
//
MAHandle maCreatePlaceholder(void) {
  return (MAHandle) new Canvas();
}

int maFontDelete(MAHandle maHandle) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas != NULL && maHandle != -1) {
    canvas->deleteFont((Font *)maHandle);
  }
  return RES_FONT_OK;
}

int maSetColor(int c) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->setColor(GET_FROM_RGB888(c));
  }
  return c;
}

void maSetClipRect(int left, int top, int width, int height) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->setClip(left, top, width, height);
  }
}

void maPlot(int posX, int posY) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->drawPixel(posX, posY);
  }
}

void maLine(int startX, int startY, int endX, int endY) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->drawLine(startX, startY, endX, endY);
  }
}

void maFillRect(int left, int top, int width, int height) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->fillRect(left, top, width, height, canvas->_drawColor);
  }
}

void maArc(int xc, int yc, double r, double start, double end, double aspect) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->drawArc(xc, yc, r, start, end, aspect);
  }
}

void maEllipse(int xc, int yc, int rx, int ry, int fill) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->drawEllipse(xc, yc, rx, ry, fill);
  }
}

void maDrawText(int left, int top, const char *str, int length) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas && str && str[0]) {
    canvas->drawText(left, top, str, length);
  }
}

void maDrawRGB(const MAPoint2d *dstPoint, const void *src, const MARect *srcRect, int opacity, int bytesPerLine) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->drawRGB(dstPoint, src, srcRect, opacity, bytesPerLine);
  }
}

MAExtent maGetTextSize(const char *str) {
  MAExtent result;
  if (str && str[0]) {
    double width = fl_width(str);
    // TODO: fixme
  } else {
    result = 0;
  }
  return result;
}

MAExtent maGetScrSize(void) {
  short width = graphics->getWidth();
  short height = graphics->getHeight();
  return (MAExtent)((width << 16) + height);
}

MAHandle maFontLoadDefault(int type, int style, int size) {
  Fl_Font font = FL_COURIER;
  if (style & FONT_STYLE_BOLD) {
    font += FL_BOLD;
  }
  if (style & FONT_STYLE_ITALIC) {
    font += FL_ITALIC;
  }
  return (MAHandle)new Font(font, size);
}

MAHandle maFontSetCurrent(MAHandle maHandle) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    canvas->setFont((Font *)maHandle);
  }
  return maHandle;
}

void maDrawImageRegion(MAHandle maHandle, const MARect *srcRect, const MAPoint2d *dstPoint, int transformMode) {
  Canvas *canvas = graphics->getDrawTarget();
  Canvas *src = (Canvas *)maHandle;
  if (canvas && canvas != src) {
    canvas->drawRegion(src, srcRect, dstPoint->x, dstPoint->y);
  }
}

void maDestroyPlaceholder(MAHandle maHandle) {
  Canvas *canvas = (Canvas *)maHandle;
  delete canvas;
}

void maGetImageData(MAHandle maHandle, void *dst, const MARect *srcRect, int bytesPerLine) {
  Canvas *canvas = (Canvas *)maHandle;
  if (srcRect->width == 1 && srcRect->height == 1) {
    *((int *)dst) = canvas->getPixel(srcRect->left, srcRect->top);
  } else {
    canvas->getImageData(canvas, (uint8_t *)dst, srcRect, bytesPerLine);
  }
}

MAHandle maSetDrawTarget(MAHandle maHandle) {
  return graphics->setDrawTarget(maHandle);
}

int maCreateDrawableImage(MAHandle maHandle, int width, int height) {
  int result = RES_OK;
  int maxSize = MAX(graphics->getWidth(), graphics->getHeight());
  if (height > maxSize * MAX_CANVAS_SIZE) {
    result -= 1;
  } else {
    Canvas *drawable = (Canvas *)maHandle;
    result = drawable->create(width, height) ? RES_OK : -1;
  }
  return result;
}

void maUpdateScreen(void) {
  ((::GraphicsWidget *)graphics)->redraw();
}
