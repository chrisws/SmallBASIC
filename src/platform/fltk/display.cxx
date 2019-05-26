// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "config.h"
#include "ui/utils.h"
#include "platform/fltk/display.h"

GraphicsWidget *graphics;

#define MAX_CANVAS_SIZE 20

//
// Canvas implementation
//
Canvas::Canvas() :
  _w(0),
  _h(0),
  _scale(0),
  _offscreen(0),
  _clip(NULL),
  _drawColor(fl_rgb_color(31, 28, 31)) {
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
  return _offscreen != 0;
}

void Canvas::drawArc(int xc, int yc, double r, double start, double end, double aspect) {
  fl_begin_offscreen(_offscreen);
  // fl_arc(int x, int y, int w, int h, double a1, double a2)
  fl_end_offscreen();
}

void Canvas::drawEllipse(int xc, int yc, int rx, int ry, bool fill) {
  fl_begin_offscreen(_offscreen);
  // fl_arc(int x, int y, int w, int h, double a1, double a2)
  fl_end_offscreen();
}

void Canvas::drawLine(int startX, int startY, int endX, int endY) {
  fl_begin_offscreen(_offscreen);
  fl_push_clip(x(), y(), w(), h());
  fl_color(_drawColor);
  fl_line(startX, startY, endX, endY);
  fl_pop_clip();
  fl_end_offscreen();
}

void Canvas::drawPixel(int posX, int posY) {
  fl_begin_offscreen(_offscreen);
  fl_color(_drawColor);
  fl_line(posX, posY, posX + 1, posY + 1);
  fl_end_offscreen();
}

void Canvas::drawRGB(const MAPoint2d *dstPoint, const void *src, const MARect *srcRect, int opacity, int bytesPerLine) {
  fl_begin_offscreen(_offscreen);
  fl_end_offscreen();
}

void Canvas::drawRegion(Canvas *src, const MARect *srcRect, int destX, int destY) {
  int width = MIN(_w, srcRect->width);
  int height = MIN(_h, srcRect->height);
  fl_begin_offscreen(_offscreen);
  fl_copy_offscreen(destX, destY, width, height, src->_offscreen, srcRect->left, srcRect->top);
  fl_end_offscreen();
}

void Canvas::drawText(Font *font, int left, int top, const char *str, int len) {
  fl_begin_offscreen(_offscreen);
  if (font) {
    font->setCurrent();
  }
  fl_push_clip(x(), y(), w(), h());
  fl_color(_drawColor);
  fl_draw(str, len, x() + left, y() + top);
  fl_pop_clip();
  fl_end_offscreen();
}

void Canvas::fillRect(int left, int top, int width, int height, Fl_Color color) {
  fl_begin_offscreen(_offscreen);
  fl_color(color);
  fl_push_clip(x(), y(), w(), h());
  fl_rectf(left, top, width, height);
  fl_pop_clip();
  fl_end_offscreen();
}

void Canvas::getImageData(uint8_t *image, const MARect *srcRect, int bytesPerLine) {
  int width = MIN(_w, srcRect->width);
  int height = MIN(_h, srcRect->height);
  Fl_Image_Surface *surface = new Fl_Image_Surface(width, height, 0, _offscreen);
  Fl_RGB_Image *rgbImage = surface->image();
  const uchar *data = rgbImage->array;
  int x_end = srcRect->left + srcRect->width;
  int y_end = srcRect->top + srcRect->height;
  int depth = bytesPerLine / width;

  for (int dy = 0, yi = srcRect->top; yi < y_end; yi += 1, dy++) {
    if (yi >= y() && yi < h()) {
      int yoffs = (dy * bytesPerLine);
      int src_line = yi * width * rgbImage->d();
      int src_offs = 0;
      for (int dx = 0, xi = srcRect->left; xi < x_end; xi += 1, dx++) {
        if (xi >= x() && xi < w()) {
          int offs = yoffs + (dx * depth);
          const uchar *px = data + src_line + src_offs;
          src_offs += rgbImage->d();
          image[offs + 0] = px[0];
          image[offs + 1] = px[1];
          image[offs + 2] = px[2];
          image[offs + 3] = 255;
        }
      }
    }
  }

  delete surface;
  delete rgbImage;
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
GraphicsWidget::GraphicsWidget(int xx, int yy, int ww, int hh) :
  Fl_Widget(xx, yy, ww, hh),
  _screen(NULL),
  _drawTarget(NULL),
  _font(NULL),
  _textOffset(0) {
  logEntered();
  _screen = new Canvas();
  _screen->create(ww, hh);
  graphics = this;
}

GraphicsWidget::~GraphicsWidget() {
  delete _screen;
}

void GraphicsWidget::deleteFont(Font *font) {
  if (font == _font) {
    _font = NULL;
  }
  delete font;
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

void GraphicsWidget::drawText(int left, int top, const char *str, int length) {
  if (_drawTarget) {
    _drawTarget->drawText(_font, left, top + _textOffset, str, length);
  }
}

MAExtent GraphicsWidget::getTextSize(const char *str) {
  if (_font) {
    _font->setCurrent();
  }
  int height = fl_height();
  int width = fl_width(str);
  _textOffset = height - fl_descent();
  return (MAExtent)((width << 16) + height);
}

void GraphicsWidget::resize(int x, int y, int w, int h) {
  Fl_Widget::resize(x, y, w, h);
  layout();
}

void GraphicsWidget::layout() {
  if (_screen->_w != w() || _screen->_h != h()) {
    logEntered();
    bool drawScreen = (_drawTarget == _screen);
    delete _screen;
    _screen = new Canvas();
    _screen->create(w(), h());
    _drawTarget = drawScreen ? _screen : NULL;
  }
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
  if (maHandle != -1) {
    graphics->deleteFont((Font *)maHandle);
  }
  return RES_FONT_OK;
}

int maSetColor(int c) {
  Canvas *canvas = graphics->getDrawTarget();
  if (canvas) {
    // Fl_Color => 0xrrggbbii
    canvas->setColor((c << 8) & 0xffffff00);
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
  if (str && str[0]) {
    graphics->drawText(left, top, str, length);
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
    result = graphics->getTextSize(str);
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
  graphics->setFont((Font *)maHandle);
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
  if (canvas == HANDLE_SCREEN) {
    canvas = graphics->getScreen();
  }
  canvas->getImageData((uint8_t *)dst, srcRect, bytesPerLine);
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

int maShowVirtualKeyboard(void) {
  return 0;
}
