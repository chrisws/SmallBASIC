// This file is part of SmallBASIC
//
// Copyright(C) 2001-2014 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <fltk/ask.h>
#include <fltk/Cursor.h>
#include <fltk/Font.h>
#include <fltk/Image.h>
#include <fltk/Monitor.h>
#include <fltk/draw.h>
#include <fltk/events.h>
#include <fltk/Group.h>
#include <fltk/layout.h>
#include <fltk/run.h>

#include <time.h>
#include "platform/fltk/display.h"
#include "platform/fltk/utils.h"
#include "platform/fltk/system.h"
#include "common/device.h"
#include "ui/ansiwidget.h"

using namespace fltk;

#define SIZE_LIMIT 4

DisplayWidget *widget;
Canvas *drawTarget;
bool mouseActive;
Color drawColor;
int drawColorRaw;
extern System *g_system;

//
// Canvas
//
Canvas::Canvas(int size) :
  _img(NULL),
  _clip(NULL),
  _size(size),
  _style(0),
  _isScreen(false) {
}

Canvas::~Canvas() {
  if (_img) {
    _img->destroy();
  }
  delete _img;
  delete _clip;
}

void Canvas::beginDraw() {
  _img->make_current();
  setcolor(drawColor);
  if (_clip) {
    push_clip(*_clip);
  }
}

void Canvas::create(int w, int h) {
  _img = new Image(w, h);
  _img->set_forceARGB32();

  GSave gsave;
  beginDraw();
  setcolor(drawColor);
  fillrect(0, 0, _img->w(), _img->h());
  endDraw();
}

void Canvas::drawImageRegion(Canvas *dst, const MAPoint2d *dstPoint, const MARect *srcRect) {
  fltk::Rectangle from = fltk::Rectangle(srcRect->left, srcRect->top, srcRect->width, srcRect->height);
  fltk::Rectangle to = fltk::Rectangle(dstPoint->x, dstPoint->y, srcRect->width, srcRect->height);
  GSave gsave;
  dst->beginDraw();
  _img->draw(from, to);
  dst->endDraw();
}

void Canvas::drawLine(int startX, int startY, int endX, int endY) {
  if (_isScreen) {
    fltk::setcolor(drawColor);
    fltk::drawline(startX, startY, endX, endY);
  } else {
    GSave gsave;
    beginDraw();
    fltk::drawline(startX, startY, endX, endY);
    endDraw();
  }
}

void Canvas::drawPixel(int posX, int posY) {
  if (posX > -1 && posY > -1
      && posX < _img->buffer_width()
      && posY < _img->buffer_height() - 1) {
    int delta = _img->buffer_linedelta();
    U32 *row = (U32 *)(_img->buffer() + (posY * delta));
    row[posX] = drawColorRaw;
  }

#if !defined(_Win32)
  GSave gsave;
  beginDraw();
  setcolor(drawColorRaw);
  drawpoint(posX, posY);
  endDraw();
#endif
}

void Canvas::drawRectFilled(int left, int top, int width, int height) {
  if (_isScreen) {
    fltk::setcolor(drawColor);
    fltk::fillrect(left, top, width, height);
  } else {
#if !defined(_Win32)
    GSave gsave;
    beginDraw();
    fltk::fillrect(left, top, width, height);
    endDraw();
#endif
    int w = _img->buffer_width();
    int h = _img->buffer_height() - 1;
    for (int y = 0; y < height; y++) {
      int yPos = y + top;
      if (yPos > -1 && yPos < h) {
        U32 *row = (U32 *)_img->linebuffer(yPos);
        for (int x = 0; x < width; x++) {
          int xPos = x + left;
          if (xPos > -1 && xPos < w) {
            row[xPos] = drawColorRaw;
          }
        }
      }
    }
  }
}

void Canvas::drawRGB(const MAPoint2d *dstPoint, const void *src,
                     const MARect *srcRect, int opacity) {
  unsigned char *image = (unsigned char *)src;
  size_t scale = 1;
  int w = srcRect->width;

  GSave gsave;
  beginDraw();

  for (int y = srcRect->top; y < srcRect->height; y += scale) {
    int dY = dstPoint->y + y;
    if (dY >= 0 && dY < _img->h()) {
      for (int x = srcRect->left; x < srcRect->width; x += scale) {
        int dX = dstPoint->x + x;
        if (dX >= 0 && dX < _img->w()) {
          // get RGBA components
          uint8_t r,g,b,a;
          r = image[4 * y * w + 4 * x + 0]; // red
          g = image[4 * y * w + 4 * x + 1]; // green
          b = image[4 * y * w + 4 * x + 2]; // blue
          a = image[4 * y * w + 4 * x + 3]; // alpha

          int delta = _img->buffer_linedelta();
          U32 *row = (U32 *)(_img->buffer() + (dY * delta));
          int c = row[dX];
          uint8_t dR = (c >> 16) & 0xff;
          uint8_t dG = (c >> 8) & 0xff;
          uint8_t dB = (c) & 0xff;

          if (opacity > 0 && opacity < 100 && a > 64) {
            float op = opacity / 100.0f;
            dR = ((1 - op) * r) + (op * dR);
            dG = ((1 - op) * g) + (op * dG);
            dB = ((1 - op) * b) + (op * dB);
          } else {
            dR = dR + ((r - dR) * a / 255);
            dG = dG + ((g - dG) * a / 255);
            dB = dB + ((b - dB) * a / 255);
          }
          row[dX] = (dR << 16) | (dG << 8) | (dB);

          Color px = color(dR, dG, dB);
          setcolor(px);
          drawpoint(dX, dY);
        }
      }      
    }
  }
  endDraw();
}

void Canvas::drawText(int left, int top, const char *str, int length) {
  setFont();
  if (_isScreen) {
    fltk::setcolor(drawColor);
    fltk::drawtext(str, length, left, top + (int)getascent());
  } else {
    GSave gsave;
    beginDraw();
    fltk::drawtext(str, length, left, top + (int)getascent());
    endDraw();
  }
}

void Canvas::endDraw() {
  if (_clip) {
    pop_clip();
  }
}

int Canvas::getPixel(int x, int y) {
  int result;
#if defined(WIN32)
  if (x > -1 && x < _img->w() &&
        y > -1 && y < _img->h()) {
    int delta = _img->buffer_linedelta();
    U32 *row = (U32 *)(_img->buffer() + (y * delta));
    result = row[x];
  } else {
    result = 0;
  }
#else
  GSave gsave;
  _img->make_current();
  result = ::x_get_pixel(x, y);
#endif
  return result;
}

void Canvas::resize(int w, int h) {
  if (_img) {
    Image *old = _img;
    GSave gsave;
    _img = new Image(w, h);
    _img->make_current();
    _img->set_forceARGB32();
    setcolor(DEFAULT_BACKGROUND);
    fillrect(0, 0, w, h);
    old->draw(fltk::Rectangle(old->w(), old->h()));
    old->destroy();
    delete old;
  }
}

void Canvas::setClip(int x, int y, int w, int h) {
  delete _clip;
  if (x != 0 || y != 0 || _img->w() != w || _img->h() != h) {
    _clip = new fltk::Rectangle(x, y, w, h);
  } else {
    _clip = NULL;
  }
}

void Canvas::setFont() {
  fltk::Font *font = fltk::COURIER;
  if (_size && _style) {
    if (_style & FONT_STYLE_BOLD) {
      font = font->bold();
    }
    if (_style & FONT_STYLE_ITALIC) {
      font = font->italic();
    }
  }
  setfont(font, _size);
}

//
// DisplayWidget
//
DisplayWidget::DisplayWidget(int x, int y, int w, int h, int defsize) :
  Widget(x, y, w, h, 0),
  _ansiWidget(NULL),
  _screen(NULL),
  _resized(false),
  _defsize(defsize),
  _startW(-1),
  _startH(-1) {
  drawColorRaw = DEFAULT_BACKGROUND;
  drawColor = maSetColor(drawColorRaw);
  widget = this;
}

DisplayWidget::~DisplayWidget() {
  delete _ansiWidget;
  delete _screen;
}

void DisplayWidget::createScreen() {
  if (!_screen) {
    if (_startW != -1 && _startH != -1) {
      resize(_startW, _startH);
    }
    _screen = new Canvas(_defsize);
    _screen->create(w(), h());
    _screen->_isScreen = true;
    drawTarget = _screen;
  }
  if (!_ansiWidget) {
    _ansiWidget = new AnsiWidget(w(), h());
    _ansiWidget->construct();
    _ansiWidget->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
    _ansiWidget->setFontSize(_defsize);
  }
}

void DisplayWidget::layout() {
  if (_screen != NULL && _ansiWidget != NULL &&
      (layout_damage() & LAYOUT_WH)) {
    // can't use GSave here in X
    _resized = true;
  }
  Widget::layout();
}

void DisplayWidget::draw() {
  if (_resized) {
    // resize the backing screens
    _screen->resize(w(), h());
    if (_screen != drawTarget) {
      drawTarget->resize(w(), h());
    }
    _ansiWidget->resize(w(), h());
    _resized = false;
  }

  if (_screen->_img) {
    _screen->_img->draw(0, 0);
    // draw the overlay onto the screen
    Canvas *oldTarget = drawTarget;
    drawTarget = _screen;
    _ansiWidget->drawOverlay(mouseActive);
    drawTarget = oldTarget;
  } else {
    setcolor(drawColor);
    fillrect(fltk::Rectangle(w(), h()));
  }
}

void DisplayWidget::flush(bool force) {
  _ansiWidget->flush(force);
}

void DisplayWidget::reset() {
  createScreen();
  _ansiWidget->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
  _ansiWidget->setFontSize(_defsize);
  _ansiWidget->reset();
}

int DisplayWidget::handle(int e) {
  MAEvent event;

  switch (e) {
  case SHOW:
    createScreen();
    break;

  case FOCUS:
    return 1;

  case PUSH:
    event.point.x = fltk::event_x();
    event.point.y = fltk::event_y();
    mouseActive = _ansiWidget->pointerTouchEvent(event);
    return mouseActive;

  case DRAG:
  case MOVE:
    event.point.x = fltk::event_x();
    event.point.y = fltk::event_y();
    if (mouseActive && _ansiWidget->pointerMoveEvent(event)) {
      Widget::cursor(fltk::CURSOR_HAND);
      return 1;
    }
    break;

  case RELEASE:
    if (mouseActive) {
      mouseActive = false;
      Widget::cursor(fltk::CURSOR_DEFAULT);
      event.point.x = fltk::event_x();
      event.point.y = fltk::event_y();
      _ansiWidget->pointerReleaseEvent(event);
    }
    break;
  }

  return Widget::handle(e);
}

void DisplayWidget::buttonClicked(const char *action) {
}

void DisplayWidget::clearScreen() {
  createScreen();
  _ansiWidget->clearScreen();
}

void DisplayWidget::print(const char *str) {
  _ansiWidget->print(str);
}

void DisplayWidget::drawLine(int x1, int y1, int x2, int y2) {
  _ansiWidget->drawLine(x1, y1, x2, y2);
}

void DisplayWidget::drawRectFilled(int x1, int y1, int x2, int y2) {
  _ansiWidget->drawRectFilled(x1, y1, x2, y2);
}

void DisplayWidget::drawRect(int x1, int y1, int x2, int y2) {
  _ansiWidget->drawRect(x1, y1, x2, y2);
}

void DisplayWidget::setTextColor(long fg, long bg) {
  _ansiWidget->setTextColor(fg, bg);
}

void DisplayWidget::setColor(long color) {
  _ansiWidget->setColor(color);
}

int DisplayWidget::getX(bool offset) {
  int result = _ansiWidget->getX();
  if (offset) {
    int x, y;
    _ansiWidget->getScroll(x, y);
    result -= x;
  }
  return result;
}

int DisplayWidget::getY(bool offset) {
  int result = _ansiWidget->getY();
  if (offset) {
    int x, y;
    _ansiWidget->getScroll(x, y);
    result -= y;
  }
  return result;
}

void DisplayWidget::setPixel(int x, int y, int c) {
  _ansiWidget->setPixel(x, y, c);
}

void DisplayWidget::setXY(int x, int y) {
  _ansiWidget->setXY(x, y);
}

int DisplayWidget::textHeight(void) {
  return _ansiWidget->textHeight();
}

int DisplayWidget::textWidth(const char *str) {
  int result;
  if (drawTarget && str && str[0]) {
    drawTarget->setFont();
    result = (int)getwidth(str);
  } else {
    result = 0;
  }
  return result;
}

void DisplayWidget::setFontSize(float i) {
  createScreen();
  _ansiWidget->setFontSize(i);
}

int DisplayWidget::getFontSize() {
  return _ansiWidget->getFontSize();
}

int DisplayWidget::getBackgroundColor() {
  return _ansiWidget->getBackgroundColor();
}

void DisplayWidget::setAutoflush(bool autoflush) {
  _ansiWidget->setAutoflush(autoflush);
}

void DisplayWidget::flushNow() {
  _ansiWidget->flushNow();
}


extern "C" long osd_getpixel(int x, int y) {
  return drawTarget->getPixel(x, y);
}

//
// maapi implementation
//
int maFontDelete(MAHandle maHandle) {
  return RES_FONT_OK;
}

int maSetColor(int c) {
  int r = (c >> 16) & 0xFF;
  int g = (c >> 8) & 0xFF;
  int b = (c) & 0xFF;
  drawColor = color(r,g,b);
  drawColorRaw = c;
  return drawColor;
}

void maSetClipRect(int left, int top, int width, int height) {
  if (drawTarget) {
    drawTarget->setClip(left, top, width, height);
  }
}

void maPlot(int posX, int posY) {
  if (drawTarget) {
    drawTarget->drawPixel(posX, posY);
  }
}

void maLine(int startX, int startY, int endX, int endY) {
  if (drawTarget) {
    drawTarget->drawLine(startX, startY, endX, endY);
  }
}

void maFillRect(int left, int top, int width, int height) {
  if (drawTarget) {
    drawTarget->drawRectFilled(left, top, width, height);
  }
}

void maDrawRGB(const MAPoint2d *dstPoint, const void *src, 
               const MARect *srcRect, int scanlength, int bytesPerLine) {
  if (drawTarget) {
    drawTarget->drawRGB(dstPoint, src, srcRect, scanlength);
  }
}

void maDrawText(int left, int top, const char *str, int length) {
  if (drawTarget && str && str[0]) {
    drawTarget->drawText(left, top, str, length);
  }
}

void maUpdateScreen(void) {
  widget->redraw();
}

void maResetBacklight(void) {
}

MAExtent maGetTextSize(const char *str) {
  MAExtent result;
  if (drawTarget && str && str[0]) {
    drawTarget->setFont();
    short width = (int)getwidth(str);
    short height = (int)(getascent() + getdescent());
    result = (MAExtent)((width << 16) + height);
  } else {
    result = 0;
  }
  return result;
}

MAExtent maGetScrSize(void) {
  short width = widget->w();
  short height = widget->h();
  return (MAExtent)((width << 16) + height);
}

MAHandle maFontLoadDefault(int type, int style, int size) {
  MAHandle result;
  if (drawTarget) {
    drawTarget->_style = style;
    drawTarget->_size = size;
    result = (MAHandle)drawTarget;
  } else {
    result = (MAHandle)NULL;
  }
  return result;
}

MAHandle maFontSetCurrent(MAHandle maHandle) {
  return maHandle;
}

void maDrawImageRegion(MAHandle maHandle, const MARect *srcRect,
                       const MAPoint2d *dstPoint, int transformMode) {
  Canvas *canvas = (Canvas *)maHandle;
  if (drawTarget != canvas) {
    canvas->drawImageRegion(drawTarget, dstPoint, srcRect);
  }
}

int maCreateDrawableImage(MAHandle maHandle, int width, int height) {
  int result = RES_OK;
  const fltk::Monitor &monitor = fltk::Monitor::all();
  if (height > monitor.h() * SIZE_LIMIT) {
    result -= 1;
  } else {
    Canvas *canvas = (Canvas *)maHandle;
    canvas->create(width, height);
  }
  return result;
}

MAHandle maCreatePlaceholder(void) {
  MAHandle maHandle = (MAHandle) new Canvas(widget->getDefaultSize());
  return maHandle;
}

void maDestroyPlaceholder(MAHandle maHandle) {
  Canvas *holder = (Canvas *)maHandle;
  delete holder;
}

void maGetImageData(MAHandle maHandle, void *dst, const MARect *srcRect, int scanlength) {
  Canvas *holder = (Canvas *)maHandle;
  // maGetImageData is only used for getPixel()
  *((int *)dst) = holder->getPixel(srcRect->left, srcRect->top);
}

MAHandle maSetDrawTarget(MAHandle maHandle) {
  if (maHandle == (MAHandle) HANDLE_SCREEN) {
    drawTarget = widget->getScreen();
    drawTarget->_isScreen = true;
  } else if (maHandle == (MAHandle) HANDLE_SCREEN_BUFFER) {
    drawTarget = widget->getScreen();
    drawTarget->_isScreen = false;
  } else {
    drawTarget = (Canvas *)maHandle;
    widget->getScreen()->_isScreen = true;
  }
  delete drawTarget->_clip;
  drawTarget->_clip = NULL;
  return (MAHandle) drawTarget;
}

int maGetMilliSecondCount(void) {
  return clock();
}

int maShowVirtualKeyboard(void) {
  return 0;
}

int maGetEvent(MAEvent *event) {
  int result = 0;
  if (check()) {
    switch (fltk::event()) {
    case PUSH:
      event->type = EVENT_TYPE_POINTER_PRESSED;
      result = 1;
      break;
    case DRAG:
      event->type = EVENT_TYPE_POINTER_DRAGGED;
      result = 1;
      break;
    case RELEASE:
      event->type = EVENT_TYPE_POINTER_RELEASED;
      result = 1;
      break;
    }
  }
  return result;
}

void maWait(int timeout) {
  fltk::wait(timeout);
}

void maAlert(const char *title, const char *message, const char *button1,
             const char *button2, const char *button3) {
  fltk::alert("%s\n\n%s", title, message);
}

//
// Form UI
//
void System::optionsBox(StringList *items) {
  int y = 0;
  int index = 0;
  int selectedIndex = -1;
  int screenId = widget->_ansiWidget->insetTextScreen(20,20,80,80);
  List_each(String *, it, *items) {
    char *str = (char *)(* it)->c_str();
    int w = fltk::getwidth(str) + 20;
    FormInput *item = new MenuButton(index, selectedIndex, str, 2, y, w, 22);
    widget->_ansiWidget->addInput(item);
    index++;
    y += 24;
  }

  while (g_system->isRunning() && selectedIndex == -1) {
    g_system->processEvents(true);
  }

  widget->_ansiWidget->removeInputs();
  widget->_ansiWidget->selectScreen(screenId);
  if (!g_system->isBreak()) {
    if (!form_ui::optionSelected(selectedIndex)) {
      dev_pushkey(selectedIndex);
    }
  }
  widget->redraw();
}

