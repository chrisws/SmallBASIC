// This file is part of SmallBASIC
//
// Copyright(C) 2001-2018 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#ifndef UI_CANVAS
#define UI_CANVAS

#if defined(_SDL)
#include <SDL.h>
#define MAX_CANVAS_SIZE 20

struct Canvas {
  Canvas();
  virtual ~Canvas();

  bool create(int w, int h);
  void drawRegion(Canvas *src, const MARect *srcRect, int dstx, int dsty);
  void fillRect(int x, int y, int w, int h, pixel_t color);
  void setClip(int x, int y, int w, int h);
  void setSurface(SDL_Surface *surface, int w, int h);
  pixel_t *getLine(int y) { return _pixels + (y * _w); }
  int x() { return _clip ? _clip->x : 0; }
  int y() { return _clip ? _clip->y : 0; }
  int w() { return _clip ? _clip->w : _w; }
  int h() { return _clip ? _clip->h : _h; }

  int _w;
  int _h;
  pixel_t *_pixels;
  SDL_Surface *_surface;
  SDL_Rect *_clip;
  bool _ownerSurface;
};

#elif defined(_FLTK)
#include <FL/Fl.H>
#include <FL/Fl_Rect.H>

struct Canvas {
  Canvas(int size);
  ~Canvas();

  void beginDraw();
  void create(int w, int h);
  void drawImageRegion(Canvas *dst, const MAPoint2d *dstPoint, const MARect *srcRect);
  void drawLine(int startX, int startY, int endX, int endY);
  void drawPixel(int posX, int posY);
  void drawRectFilled(int left, int top, int width, int height);
  void drawRGB(const MAPoint2d *dstPoint, const void *src,
               const MARect *srcRect, int scanlength);
  void drawText(int left, int top, const char *str, int length);
  void endDraw();
  int  getPixel(int x, int y);
  void resize(int w, int h);
  void setClip(int x, int y, int w, int h);
  void setFont();

  Fl_Offscreen _offscreen;
  Fl_Rect *_clip;
  int _w;
  int _h;
  int _size;
  int _style;
  bool _isScreen;
};

#else
#include <android/rect.h>
#define MAX_CANVAS_SIZE 20

struct Canvas {
  Canvas();
  virtual ~Canvas();

  bool create(int w, int h);
  void drawRegion(Canvas *src, const MARect *srcRect, int dstx, int dsty);
  void fillRect(int x, int y, int w, int h, pixel_t color);
  void setClip(int x, int y, int w, int h);
  pixel_t *getLine(int y) { return _pixels + (y * _w); }
  int x() { return _clip ? _clip->left : 0; }
  int y() { return _clip ? _clip->top : 0; }
  int w() { return _clip ? _clip->right : _w; }
  int h() { return _clip ? _clip->bottom : _h; }

  int _w;
  int _h;
  pixel_t *_pixels;
  ARect *_clip;
};

#endif
#endif

