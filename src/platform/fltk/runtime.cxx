// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <config.h>
#include "common/osd.h"
#include "lib/maapi.h"
#include "platform/fltk/MainWindow.h"
#include "platform/fltk/runtime.h"
#include <FL/fl_ask.H>
#include <FL/Fl_Menu_Button.H>

extern MainWindow *wnd;
extern System *g_system;

#define OPTIONS_BOX_WIDTH_EXTRA 4

int event_x() {
  return Fl::event_x();
}

int event_y() {
  return Fl::event_y() - wnd->_out->y() - 2;
}

void setMotionEvent(MAEvent &event, int type) {
  event.type = type;
  event.point.x = event_x();
  event.point.y = event_y();;
}

//
// Runtime implementation
//
Runtime::Runtime(int w, int h, int defsize) : System() {
  _output = new AnsiWidget(w, h - 1);
  _output->construct();
  _output->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
  _output->setFont(defsize, false, false);
}

Runtime::~Runtime() {
  // empty
}

void Runtime::alert(const char *title, const char *message) {
  fl_alert("%s", message);
}

int Runtime::ask(const char *title, const char *prompt, bool cancel) {
  int result;
  if (cancel) {
    result = fl_choice(prompt, "Yes", "No", "Cancel", NULL);
  } else {
    result = fl_choice(prompt, "Yes", "No", "", NULL);
  }
  return result;
}

void Runtime::browseFile(const char *url) {
  ::browseFile(url);
}

void Runtime::enableCursor(bool enabled) {
  fl_cursor(enabled ? FL_CURSOR_DEFAULT : FL_CURSOR_NONE);
}

char *Runtime::getClipboardText() {
  return NULL;
}

int Runtime::handle(int e) {
  MAEvent event;

  switch (e) {
  case FL_PUSH:
    setMotionEvent(event, EVENT_TYPE_POINTER_PRESSED);
    if (event.point.y >= 0) {
      handleEvent(event);
    }
    break;

  case FL_DRAG:
    setMotionEvent(event, EVENT_TYPE_POINTER_DRAGGED);
    handleEvent(event);
    break;

  case FL_RELEASE:
    setMotionEvent(event, EVENT_TYPE_POINTER_RELEASED);
    handleEvent(event);
    break;

  default:
    break;
  }

  return 0;
}

void Runtime::optionsBox(StringList *items) {
  Fl_Menu_Item menu[items->size() + 1];
  int index = 0;
  int width = 0;
  int height = 0;
  int charWidth = _output->getCharWidth();

  List_each(String *, it, *items) {
    int w, h;
    char *str = (char *)(* it)->c_str();
    menu[index].text = str;
    menu[index].shortcut_ = 0;
    menu[index].callback_ = 0;
    menu[index].user_data_ = (void *)(intptr_t)index;
    menu[index].flags = 0;
    menu[index].labeltype_ = 0;
    menu[index].labelfont_ = FL_HELVETICA;
    menu[index].labelsize_ = FL_NORMAL_SIZE;
    menu[index].labelcolor_ = FL_FOREGROUND_COLOR;
    menu[index].measure(&h, NULL);

    height += h + 1;
    w = (strlen(str) * charWidth);
    if (w > width) {
      width = w;
    }
    index++;
  }

  menu[index].flags = 0;
  menu[index].text = NULL;
  width += (charWidth * OPTIONS_BOX_WIDTH_EXTRA);

  int menuX = event_x();
  int menuY = event_y();
  int maxWidth = wnd->_out->w() - wnd->_out->x();
  int maxHeight = wnd->h() - height - MENU_HEIGHT;

  if (menuX + width >= maxWidth) {
    menuX = maxWidth - width;
  }

  if (menuY + height >= maxHeight) {
    menuY = maxHeight - height;
  } else {
    menuY -= wnd->_out->y();
  }

  Fl_Menu_Button popup(menuX, menuY, width, height, "&popup");
  popup.menu(menu);

  const Fl_Menu_Item *result = popup.popup();
  if (result && result->text) {
    MAEvent event;
    event.type = EVENT_TYPE_OPTIONS_BOX_BUTTON_CLICKED;
    event.optionsBoxButtonIndex = (intptr_t)(void *)result->user_data_;
    handleEvent(event);
  }
}

MAEvent Runtime::processEvents(int waitFlag) {
  MAEvent event;
  event.type = 0;
  event.key = 0;
  event.nativeKey = 0;

  if (keymap_kbhit()) {
    event.type = EVENT_TYPE_KEY_PRESSED;
    event.key = keymap_kbpeek();
  }
  return event;
}

void Runtime::setClipboardText(const char *text) {
  Fl::copy(text, strlen(text), true);
}

void Runtime::setFontSize(int size) {
  _output->setFontSize(size);
}

void Runtime::setWindowSize(int width, int height) {
  wnd->size(width, height);
}

void Runtime::setWindowTitle(const char *title) {
  wnd->label(title);
}

void Runtime::showCursor(CursorType cursorType) {
  switch (cursorType) {
  case kHand:
    fl_cursor(FL_CURSOR_HAND);
    break;
  case kArrow:
    fl_cursor(FL_CURSOR_ARROW);
    break;
  case kIBeam:
    fl_cursor(FL_CURSOR_INSERT);
    break;
  }
}

//
// System platform methods
//
void System::editSource(strlib::String loadPath, bool restoreOnExit) {
  // empty
}

bool System::getPen3() {
  //SDL_PumpEvents();
  //return (SDL_BUTTON(SDL_BUTTON_LEFT) && SDL_GetMouseState(&_touchCurX, &_touchCurY));
  return 0;
}

void System::completeKeyword(int index) {
  // empty
}

//
// ma event handling
//
int maGetEvent(MAEvent *event) {
  int result = 0;
  if (Fl::check()) {
    switch (Fl::event()) {
    case FL_PUSH:
      event->type = EVENT_TYPE_POINTER_PRESSED;
      result = 1;
      break;
    case FL_DRAG:
      event->type = EVENT_TYPE_POINTER_DRAGGED;
      result = 1;
      break;
    case FL_RELEASE:
      event->type = EVENT_TYPE_POINTER_RELEASED;
      result = 1;
      break;
    }
  }
  return result;
}

void maWait(int timeout) {
  Fl::wait(timeout);
}

//
// sbasic implementation
//
int osd_devinit() {
  // allow the application to set the preferred width and height
  if ((opt_pref_width || opt_pref_height) && wnd->isIdeHidden()) {
    int delta_x = wnd->w() - g_system->getOutput()->getWidth();
    int delta_y = wnd->h() - g_system->getOutput()->getHeight();
    if (opt_pref_width < 10) {
      opt_pref_width = 10;
    }
    if (opt_pref_height < 10) {
      opt_pref_height = 10;
    }
    int w = opt_pref_width + delta_x;
    int h = opt_pref_height + delta_y;
    wnd->_outputGroup->resize(0, 0, w, h);
  }

  // show the output-group in case it's the full-screen container.
  if (wnd->isInteractive() && !wnd->logPrint()) {
    wnd->_outputGroup->show();
  }

  g_system->setRunning(true);
  return 1;
}

int osd_devrestore() {
  g_system->setRunning(false);
  return 1;
}

void osd_beep() {
  fl_beep();
}

void osd_sound(int frq, int ms, int vol, int bgplay) {
#if defined(WIN32)
  if (!bgplay) {
    ::Beep(frq, ms);
  }
#endif
}

//
// utils
//
void appLog(const char *format, ...) {
  va_list args;
  va_start(args, format);
  unsigned size = vsnprintf(NULL, 0, format, args);
  va_end(args);

  if (size) {
    char *buf = (char *)malloc(size + 3);
    buf[0] = '\0';
    va_start(args, format);
    vsnprintf(buf, size + 1, format, args);
    va_end(args);
    buf[size] = '\0';

    int i = size - 1;
    while (i >= 0 && isspace(buf[i])) {
      buf[i--] = '\0';
    }
    strcat(buf, "\r\n");
    if (wnd && wnd->tty()) {
      wnd->tty()->print(buf);
    } else {
      fprintf(stderr, "%s", buf);
    }
    free(buf);
  }
}

//
// not implemented
//
void open_audio() {}
void close_audio() {}
void osd_audio(const char *path) {}
void osd_clear_sound_queue() {}
