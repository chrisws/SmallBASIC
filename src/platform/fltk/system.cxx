// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <config.h>
#include <sys/socket.h>
#include <FL/fl_ask.H>
#include "platform/fltk/MainWindow.h"
#include "platform/fltk/HelpWidget.h"
#include "platform/fltk/display.h"
#include "platform/fltk/system.h"
#include "common/sbapp.h"
#include "common/sys.h"
#include "common/device.h"
#include "common/smbas.h"
#include "common/osd.h"
#include "common/keymap.h"
#include "common/fs_socket_client.h"

extern MainWindow *wnd;
System *g_system;
AnsiWidget *ansiWidget;

#define EVT_PAUSE_TIME 0.005
#define RX_BUFFER_SIZE 1024

//
// System implementation
//
System::System(int w, int h, int defsize) {
  ansiWidget = new AnsiWidget(w, h);
  ansiWidget->construct();
  ansiWidget->setTextColor(DEFAULT_FOREGROUND, DEFAULT_BACKGROUND);
  ansiWidget->setFontSize(defsize);
  g_system = this;
}

System::~System() {
  delete ansiWidget;
}

void System::alert(const char *title, const char *message) {
  //fl_alert(message);
}

int System::ask(const char *title, const char *prompt, bool cancel) {
  //if (cancel) {
  //    return Fl_choice(prompt, "Yes", "No", "Cancel");
  //  } else {
  //    return Fl_choice(prompt, "Yes", "No", "");
  //  }
  return 0;
}

void System::browseFile(const char *url) {
  ::browseFile(url);
}

char *System::getClipboardText() {
  return NULL;
}

AnsiWidget *System::getOutput() {
  return ansiWidget;
}

bool System::isBreak() {
  return wnd->isBreakExec();
}

bool System::isRunning() {
  return wnd->isRunning();
}

void System::optionsBox(StringList *items) {
  /*
  int y = 0;
  int index = 0;
  int selectedIndex = -1;
  int screenId = widget->_ansiWidget->insetTextScreen(20,20,80,80);
  List_each(String *, it, *items) {
    char *str = (char *)(* it)->c_str();
    int w = Fl_getwidth(str) + 20;
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
  */
}

MAEvent System::processEvents(bool wait) {
  osd_events(wait);
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

void System::setClipboardText(const char *text) {
  Fl::copy(text, strlen(text), true);
}

void System::setFontSize(int size) {
  ansiWidget->setFontSize(size);
}

void System::setWindowSize(int width, int height) {
  // TODO
}

void System::systemPrint(const char *message, ...) {
  wnd->tty()->print(message);
}

//
// maapi implementation
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

int maShowVirtualKeyboard(void) {
  return 0;
}

int maGetMilliSecondCount(void) {
  return dev_get_millisecond_count();
}

void maWait(int timeout) {
  Fl::wait(timeout);
}

//
// common device implementation
//
int osd_devinit() {
  wnd->resetPen();

  // allow the application to set the preferred width and height
  if ((opt_pref_width || opt_pref_height) && wnd->isIdeHidden()) {
    int delta_x = wnd->w() - ansiWidget->getWidth();
    int delta_y = wnd->h() - ansiWidget->getHeight();
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

  dev_fgcolor = -DEFAULT_FOREGROUND;
  dev_bgcolor = -DEFAULT_BACKGROUND;
  os_graf_mx = ansiWidget->getWidth();
  os_graf_my = ansiWidget->getHeight();
  setsysvar_int(SYSVAR_XMAX, os_graf_mx - 1);
  setsysvar_int(SYSVAR_YMAX, os_graf_my - 1);
  osd_cls();
  dev_clrkb();
  ansiWidget->reset();
  ansiWidget->setAutoflush(!opt_show_page);
  return 1;
}

int osd_devrestore() {
  ansiWidget->setAutoflush(true);
  return 1;
}

void osd_cls(void) {
  logEntered();
  ansiWidget->clearScreen();

  // send reset and clear screen codes
  if (wnd->isInteractive()) {
    ansiWidget->clearScreen();
    TtyWidget *tty = wnd->tty();
    if (tty) {
      tty->clearScreen();
    }
  }
}

int osd_events(int wait_flag) {
  switch (wait_flag) {
  case 1:
    // wait for an event
    ansiWidget->flush(true);
    Fl::wait();
    break;
  case 2:
    // pause
    Fl::wait(EVT_PAUSE_TIME);
    break;
  default:
    // pump messages without pausing
    Fl::check();
  }

  if (wnd->isBreakExec()) {
    return -2;
  }

  ansiWidget->flush(false);

  return 0;
}

/**
 * sets the current mouse position and returns whether the mouse is within the output window
 */
// bool get_mouse_xy() {
//   Fl_Rect rc;
//   int x, y;

//   Fl_get_mouse(x, y);
//   ansiWidget->get_absolute_rect(&rc);

//   // convert mouse screen rect to out-client rect
//   wnd->_penDownX = x - rc.x();
//   wnd->_penDownY = y - rc.y();

//   return rc.contains(x, y);
// }

int osd_getpen(int code) {
  if (wnd->isBreakExec()) {
    brun_break();
    return 0;
  }

  switch (code) {
  case 0:
    // UNTIL PEN(0) - wait until click or move
    ansiWidget->flush(true);
    Fl::wait();               // fallthru to re-test

  case 3:                      // returns true if the pen is down (and save curpos)
    /*
      TODO: fixme
    if (event_state() & ANY_BUTTON) {
      if (get_mouse_xy()) {
        return 1;
      }
    }
    */
    return 0;

  case 1:                      // last pen-down x
    return wnd->_penDownX;

  case 2:                      // last pen-down y
    return wnd->_penDownY;

  case 4:                      // cur pen-down x
  case 10:
    //get_mouse_xy();
    return wnd->_penDownX;

  case 5:                      // cur pen-down y
  case 11:
    //get_mouse_xy();
    return wnd->_penDownY;

  case 12:                     // true if left button pressed
    return (Fl::event_state() & FL_BUTTON1);

  case 13:                     // true if right button pressed
    return (Fl::event_state() & FL_BUTTON3);

  case 14:                     // true if middle button pressed
    return (Fl::event_state() & FL_BUTTON2);
  }
  return 0;
}

long osd_getpixel(int x, int y) {
  return ansiWidget->getPixel(x, y);
}

int osd_getx() {
  return ansiWidget->getX();
}

int osd_gety() {
  return ansiWidget->getY();
}

void osd_line(int x1, int y1, int x2, int y2) {
  ansiWidget->drawLine(x1, y1, x2, y2);
}

void osd_arc(int xc, int yc, double r, double start, double end, double aspect) {
  ansiWidget->drawArc(xc, yc, r, start, end, aspect);
}

void osd_ellipse(int xc, int yc, int xr, int yr, int fill) {
  ansiWidget->drawEllipse(xc, yc, xr, yr, fill);
}

void osd_rect(int x1, int y1, int x2, int y2, int fill) {
  if (fill) {
    ansiWidget->drawRectFilled(x1, y1, x2, y2);
  } else {
    ansiWidget->drawRect(x1, y1, x2, y2);
  }
}

void osd_refresh(void) {
  ansiWidget->flush(true);
}

void osd_setcolor(long color) {
  ansiWidget->setColor(color);
}

void osd_setpenmode(int enable) {
  fl_cursor(enable ? FL_CURSOR_DEFAULT : FL_CURSOR_NONE);
}

void osd_setpixel(int x, int y) {
  ansiWidget->setPixel(x, y, dev_fgcolor);
}

void osd_settextcolor(long fg, long bg) {
  ansiWidget->setTextColor(fg, bg);
}

void osd_setxy(int x, int y) {
  ansiWidget->setXY(x, y);
}

int osd_textheight(const char *str) {
  return ansiWidget->textHeight();
}

int osd_textwidth(const char *str) {
  MAExtent textSize = maGetTextSize(str);
  return EXTENT_X(textSize);
}

void osd_write(const char *str) {
  if (wnd->tty() && wnd->logPrint()) {
    wnd->tty()->print(str);
  }

  ansiWidget->print(str);
}

void lwrite(const char *s) {
  TtyWidget *tty = wnd->tty();
  if (tty) {
    tty->print(s);
  }
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

void timeout_callback(void *data) {
  if (wnd->isModal()) {
    wnd->setModal(false);
  }
}

//
// device methods
//
void dev_delay(uint32_t ms) {
  ansiWidget->flush(true);

  if (!wnd->isBreakExec()) {
    Fl::add_timeout(((float)ms) / 1000, timeout_callback, 0);
    wnd->setModal(true);
    ansiWidget->flush(true);
    while (wnd->isModal()) {
      Fl::wait(0.1);
    }
  }
}

void enter_cb(Fl_Widget *, void *v) {
  wnd->setModal(false);
}

char *dev_gets(char *dest, int size) {
  if (!wnd->isInteractive() || wnd->logPrint()) {
    EditorWidget *editor = wnd->_runEditWidget;
    if (!editor) {
      editor = wnd->getEditor(false);
    }
    if (editor) {
      editor->getInput(dest, size);
    }
    return dest;
  }

  ansiWidget->flush(true);
  // TODO: fixme
  //wnd->_tabGroup->selected_child(wnd->_outputGroup);
  wnd->_outputGroup->begin();

  LineInput *in = new LineInput(ansiWidget->getX() + 2,
                                ansiWidget->getY() + 1,
                                20, ansiWidget->textHeight() + 4);
  wnd->_outputGroup->end();
  in->callback(enter_cb);
  //in->reserve(size);
  //in->textfont(ansiWidget->labelfont());
  //in->textsize(ansiWidget->labelsize());

  wnd->setModal(true);

  while (wnd->isModal()) {
    Fl::wait();
  }

  if (wnd->isBreakExec()) {
    brun_break();
  }

  wnd->_outputGroup->remove(in);
  int len = in->size() < size ? in->size() : size;
  strncpy(dest, in->value(), len);
  dest[len] = 0;
  delete in;

  // reposition x to adjust for input box
  ansiWidget->setXY(ansiWidget->getX() + 4, ansiWidget->getY());
  ansiWidget->print(dest);

  return dest;
}

//
// utils
//
void getHomeDir(char *fileName, size_t size, bool appendSlash) {
  const int homeIndex = 1;
  static const char *envVars[] = {
    "APPDATA", "HOME", "TMP", "TEMP", "TMPDIR", ""
  };

  fileName[0] = '\0';

  for (int i = 0; envVars[i][0] != '\0' && fileName[0] == '\0'; i++) {
    const char *home = getenv(envVars[i]);
    if (home && access(home, R_OK) == 0) {
      strlcpy(fileName, home, size);
      if (i == homeIndex) {
        strlcat(fileName, "/.config", size);
        makedir(fileName);
      }
      strlcat(fileName, "/SmallBASIC", size);
      if (appendSlash) {
        strlcat(fileName, "/", size);
      }
      makedir(fileName);
      break;
    }
  }
}

// copy the url into the local cache
bool cacheLink(dev_file_t *df, char *localFile, size_t size) {
  char rxbuff[RX_BUFFER_SIZE];
  FILE *fp;
  const char *url = df->name;
  const char *pathBegin = strchr(url + 7, '/');
  const char *pathEnd = strrchr(url + 7, '/');
  const char *pathNext;
  bool inHeader = true;
  bool httpOK = false;

  getHomeDir(localFile, size, true);
  strcat(localFile, "/cache/");
  makedir(localFile);

  // create host name component
  strncat(localFile, url + 7, pathBegin - url - 7);
  strcat(localFile, "/");
  makedir(localFile);

  if (pathBegin != 0 && pathBegin < pathEnd) {
    // re-create the server path in cache
    int level = 0;
    pathBegin++;
    do {
      pathNext = strchr(pathBegin, '/');
      strncat(localFile, pathBegin, pathNext - pathBegin + 1);
      makedir(localFile);
      pathBegin = pathNext + 1;
    }
    while (pathBegin < pathEnd && ++level < 20);
  }
  if (pathEnd == 0 || pathEnd[1] == 0 || pathEnd[1] == '?') {
    strcat(localFile, "index.html");
  } else {
    strcat(localFile, pathEnd + 1);
  }

  fp = fopen(localFile, "wb");
  if (fp == 0) {
    if (df->handle != -1) {
      shutdown(df->handle, df->handle);
    }
    return false;
  }

  if (df->handle == -1) {
    // pass the cache file modified time to the HTTP server
    struct stat st;
    if (stat(localFile, &st) == 0) {
      df->drv_dw[2] = st.st_mtime;
    }
    if (http_open(df) == 0) {
      return false;
    }
  }

  while (true) {
    int bytes = recv(df->handle, (char *)rxbuff, sizeof(rxbuff), 0);
    if (bytes == 0) {
      break;                    // no more data
    }
    // assumes http header < 1024 bytes
    if (inHeader) {
      int i = 0;
      while (true) {
        int iattr = i;
        while (rxbuff[i] != 0 && rxbuff[i] != '\n') {
          i++;
        }
        if (rxbuff[i] == 0) {
          inHeader = false;
          break;                // no end delimiter
        }
        if (rxbuff[i + 2] == '\n') {
          if (!fwrite(rxbuff + i + 3, bytes - i - 3, 1, fp)) {
            break;
          }
          inHeader = false;
          break;                // found start of content
        }
        // null terminate attribute (in \r)
        rxbuff[i - 1] = 0;
        i++;
        if (strstr(rxbuff + iattr, "200 OK") != 0) {
          httpOK = true;
        }
        if (strncmp(rxbuff + iattr, "Location: ", 10) == 0) {
          // handle redirection
          shutdown(df->handle, df->handle);
          strcpy(df->name, rxbuff + iattr + 10);
          if (http_open(df) == 0) {
            fclose(fp);
            return false;
          }
          break;                // scan next header
        }
      }
    } else if (!fwrite(rxbuff, bytes, 1, fp)) {
      break;
    }
  }

  // cleanup
  fclose(fp);
  shutdown(df->handle, df->handle);
  return httpOK;
}

//
// not implemented
//
void open_audio() {}
void close_audio() {}
void osd_audio(const char *path) {}
void osd_clear_sound_queue() {}
void dev_log_stack(const char *keyword, int type, int line) {}
