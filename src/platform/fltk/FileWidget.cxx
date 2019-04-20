// This file is part of SmallBASIC
//
// Copyright(C) 2001-2014 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
// 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fltk/ask.h>
#include <fltk/events.h>
#include <fltk/run.h>

#include "platform/fltk/MainWindow.h"
#include "platform/fltk/HelpWidget.h"
#include "platform/fltk/FileWidget.h"
#include "ui/strlib.h"
#include "common/device.h"

FileWidget *fileWidget;
String click;
enum SORT_BY { e_name, e_size, e_time } sortBy;
bool sortDesc;

struct FileNode {
  FileNode(const char *arg_name, time_t arg_m_time,
           off_t arg_size, bool arg_isdir) : 
    name(arg_name, strlen(arg_name)),
    m_time(arg_m_time), 
    size(arg_size), 
    isdir(arg_isdir) {
  } 
  String name;
  time_t m_time;
  off_t size;
  bool isdir;
};

int stringCompare(const void *a, const void *b) {
  String *s1 = ((String **) a)[0];
  String *s2 = ((String **) b)[0];
  return strcasecmp(s1->c_str(), s2->c_str());
}

int fileNodeCompare(const void *a, const void *b) {
  FileNode *n1 = ((FileNode **) a)[0];
  FileNode *n2 = ((FileNode **) b)[0];
  int result = 0;
  switch (sortBy) {
  case e_name:
    if (n1->isdir && !n2->isdir) {
      result = -1;
    } else if (!n1->isdir && n2->isdir) {
      result = 1;
    } else {
      result = strcasecmp(n1->name.c_str(), n2->name.c_str());
    }
    break;
  case e_size:
    result = n1->size < n2->size ? -1 : n1->size > n2->size ? 1 : 0;
    break;
  case e_time:
    result = n1->m_time < n2->m_time ? -1 : n1->m_time > n2->m_time ? 1 : 0;
    break;
  }
  if (sortDesc) {
    result = -result;
  }
  return result;
}

void updateSortBy(SORT_BY newSort) {
  if (sortBy == newSort) {
    sortDesc = !sortDesc;
  } else {
    sortBy = newSort;
    sortDesc = false;
  }
}

static void anchorClick_event(void *) {
  fltk::remove_check(anchorClick_event);
  fileWidget->anchorClick();
}

static void anchorClick_cb(Widget *w, void *v) {
  if (fileWidget) {
    click.empty();
    click.append((char *)v);
    fltk::add_check(anchorClick_event); // post message
  }
}

const char CMD_SET_DIR = '+';
const char CMD_CHG_DIR = '!';
const char CMD_ENTER_PATH = '@';
const char CMD_SAVE_AS = '~';
const char CMD_SORT_DATE = '#';
const char CMD_SORT_SIZE = '^';
const char CMD_SORT_NAME = '$';

FileWidget::FileWidget(int x, int y, int w, int h) : 
  HelpWidget(x, y, w, h),
  _saveEditorAs(0),
  _recentPaths(NULL) {
  callback(anchorClick_cb);
  fileWidget = this;
  sortDesc = false;
  sortBy = e_name;
}

FileWidget::~FileWidget() {
  fileWidget = NULL;
  delete _recentPaths;
}

//
// convert slash chars in filename to forward slashes
//
const char *FileWidget::forwardSlash(char *filename) {
  const char *result = 0;
  int len = filename ? strlen(filename) : 0;
  for (int i = 0; i < len; i++) {
    if (filename[i] == '\\') {
      filename[i] = '/';
      result = &filename[i];
    }
  }
  return result;
}

/**
 * return the name component of the full file path
 */
const char *FileWidget::splitPath(const char *filename, char *path) {
  const char *result = strrchr(filename, '/');
  if (!result) {
    result = strrchr(filename, '\\');
  }

  if (!result) {
    result = filename;
  } else {
    // skip slash
    result++; 
  }

  if (path) {
    // return the path component
    int len = result - filename - 1;
    if (len > 0) {
      strncpy(path, filename, len);
      // snip any double slashes
      while (len > 0 && path[len - 1] == '/') {
        len--;
      }
      path[len] = 0;
    } else {
      path[0] = 0;
    }
  }

  return result;
}

//
// removes CRLF line endings
//
const char *FileWidget::trimEOL(char *buffer) {
  int index = strlen(buffer) - 1;
  const char *result = buffer;
  while (index > 0 && (buffer[index] == '\r' || buffer[index] == '\n')) {
    buffer[index] = 0;
    index--;
  }
  return result;
}

//
// anchor link clicked
//
void FileWidget::anchorClick() {
  const char *target = click.c_str();

  switch (target[0]) {
  case CMD_CHG_DIR:
    changeDir(target + 1);
    return;

  case CMD_SET_DIR:
    setDir(target + 1);
    return;

  case CMD_SAVE_AS:
    saveAs();
    return;

  case CMD_ENTER_PATH:
    enterPath();
    return;

  case CMD_SORT_NAME:
    updateSortBy(e_name);
    displayPath();
    return;

  case CMD_SORT_SIZE:
    updateSortBy(e_size);
    displayPath();
    return;

  case CMD_SORT_DATE:
    updateSortBy(e_time);
    displayPath();
    return;
  }

  String docHome;
  if (target[0] == '/') {
    const char *base = getDocHome();
    if (base && base[0]) {
      // remove any overlapping string segments between the docHome
      // of the index page, eg c:/home/cache/smh/handheld/ and the
      // anchored sub-page, eg, "/handheld/articles/2006/... "
      // ie, remove the URL component from the file name
      int len = strlen(base);
      const char *p = strchr(base + 1, '/');
      while (p && *p && *(p + 1)) {
        if (strncmp(p, target, len - (p - base)) == 0) {
          len = p - base;
          break;
        }
        p = strchr(p + 1, '/');
      }
      docHome.append(base, len);
    }
  } else {
    docHome.append(_path);
  }

  if (_saveEditorAs) {
    Input *input = (Input *) getInput("saveas");
    input->value(target);
  } else {
    setDocHome(docHome);
    String fullPath;
    fullPath.append(docHome.c_str());
    fullPath.append("/");
    fullPath.append(target[0] == '/' ? target + 1 : target);
    wnd->editFile(fullPath.c_str());
  }
}

//
// open file
//
void FileWidget::fileOpen(EditorWidget *_saveEditorAs) {
  this->_saveEditorAs = _saveEditorAs;
  displayPath();
}

//
// display the given path
//
void FileWidget::openPath(const char *newPath, StringList *recentPaths) {
  if (newPath && access(newPath, R_OK) == 0) {
    strcpy(_path, newPath);
  } else {
    getcwd(_path, sizeof(_path));
  }

  delete _recentPaths;
  _recentPaths = recentPaths;

  forwardSlash(_path);
  displayPath();
  redraw();
}

//
// change to the given dir
//
void FileWidget::changeDir(const char *target) {
  char newPath[PATH_MAX + 1];

  strcpy(newPath, _path);

  // file browser window
  if (strcmp(target, "..") == 0) {
    // go up a level c:/src/foo or /src/foo
    char *p = strrchr(newPath, '/');
    if (strchr(newPath, '/') != p) {
      *p = 0;                   // last item not first
    } else {
      *(p + 1) = 0;             // found root
    }
  } else {
    // go down a level
    if (newPath[strlen(newPath) - 1] != '/') {
      strcat(newPath, "/");
    }
    strcat(newPath, target);
  }
  setDir(newPath);
}

//
// set to the given dir
//
void FileWidget::setDir(const char *target) {
  if (chdir(target) == 0) {
    strcpy(_path, target);
    displayPath();
  } else {
    message("Invalid path '%s'", target);
  }
}

//
// display the path
//
void FileWidget::displayPath() {
  dirent *entry;
  struct stat stbuf;
  strlib::List<FileNode *> files;
  char modifedTime[100];
  String html;

  if (chdir(_path) != 0) {
    return;
  }

  DIR *dp = opendir(_path);
  if (dp == 0) {
    return;
  }

  while ((entry = readdir(dp)) != 0) {
    char *name = entry->d_name;
    int len = strlen(name);
    if (strcmp(name, ".") == 0) {
      continue;
    }

    if (strcmp(name, "..") == 0) {
      if (strcmp(_path, "/") != 0 && strcmp(_path + 1, ":/") != 0) {
        // not "/" or "C:/"
        files.add(new FileNode("..", stbuf.st_mtime, stbuf.st_size, true));
      }
    } else if (stat(name, &stbuf) != -1 && stbuf.st_mode & S_IFDIR) {
      files.add(new FileNode(name, stbuf.st_mtime, stbuf.st_size, true));
    } else if (strncasecmp(name + len - 4, ".htm", 4) == 0 ||
               strncasecmp(name + len - 5, ".html", 5) == 0 ||
               strncasecmp(name + len - 4, ".bas", 4) == 0 || 
               strncasecmp(name + len - 4, ".txt", 4) == 0) {
      files.add(new FileNode(name, stbuf.st_mtime, stbuf.st_size, false));
    }
  }
  closedir(dp);

  files.sort(fileNodeCompare);

  if (_saveEditorAs) {
    const char *path = _saveEditorAs->getFilename();
    const char *slash = strrchr(path, '/');
    html.append("<b>Save ").append(slash ? slash + 1 : path).append(" as:<br>")
        .append("<input size=220 type=text value='").append(slash ? slash + 1 : path)
        .append("' name=saveas>&nbsp;<input type=button onclick='")
        .append(CMD_SAVE_AS).append("' value='Save As'><br><br>");
  }

  _recentPaths->sort(stringCompare);
  html.append("<b>Recent places:</b><br>");
  List_each(String*, it, *_recentPaths) {
    String *nextPath = (*it);
    if (!nextPath->equals(_path)) {
      html.append("<a href='")
          .append(CMD_SET_DIR)
          .append(nextPath)
          .append("'> [ ")
          .append(nextPath)
          .append(" ]</a><br>");
    }
  }

  html.append("<br><b>Files in: <a href=")
      .append(CMD_ENTER_PATH).append(">").append(_path)
      .append("</a></b>");

  html.append("<table><tr bgcolor=#e1e1e1>")
      .append("<td><a href=").append(CMD_SORT_NAME).append("><b><u>Name</u></b></a></td>")
      .append("<td><a href=").append(CMD_SORT_SIZE).append("><b><u>Size</u></b></a></td>")
      .append("<td><a href=").append(CMD_SORT_DATE).append("><b><u>Date</u></b></a></td></tr>");

  List_each(FileNode*, it, files) {
    FileNode *fileNode = (*it);
    html.append("<tr bgcolor=#f1f1f1>").append("<td><a href='");
    if (fileNode->isdir) {
      html.append(CMD_CHG_DIR);
    }
    html.append(fileNode->name).append("'>");
    if (fileNode->isdir) {
      html.append("[ ");
    }
    html.append(fileNode->name);
    if (fileNode->isdir) {
      html.append(" ]");
    }
    html.append("</a></td>");
    html.append("<td>");
    if (fileNode->isdir) {
      html.append(0);
    } else {
      html.append(fileNode->isdir ? 0 : (int)fileNode->size);
    }
    html.append("</td>");
    strftime(modifedTime, sizeof(modifedTime), "%Y-%m-%d %I:%M %p",
             localtime(&fileNode->m_time));
    html.append("<td>").append(modifedTime).append("</td></tr>");
  }

  html.append("</table>");
  loadBuffer(html);
  take_focus();
}

//
// open the path
//
void FileWidget::enterPath() {
  const char *newPath = fltk::input("Enter path:", _path);
  if (newPath != 0) {
    if (chdir(newPath) == 0) {
      strcpy(_path, newPath);
      displayPath();
    } else {
      message("Invalid path '%s'", newPath);
    }
  }
}

//
// event handler
//
int FileWidget::handle(int e) {
  static char buffer[PATH_MAX];
  static int dnd_active = 0;

  switch (e) {
  case SHOW:
    if (_saveEditorAs) {
      _saveEditorAs = 0;
      displayPath();
    }
    break;

  case DND_LEAVE:
    dnd_active = 0;
    return 1;

  case DND_DRAG:
  case DND_RELEASE:
  case DND_ENTER:
    dnd_active = 1;
    return 1;

  case MOVE:
    if (dnd_active) {
      return 1;                 // return 1 to become drop-target
    }
    break;

  case PASTE:
    strncpy(buffer, fltk::event_text(), fltk::event_length());
    buffer[fltk::event_length()] = 0;
    forwardSlash(buffer);
    wnd->editFile(buffer);
    dnd_active = 0;
    return 1;
  }

  return HelpWidget::handle(e);
}

//
// save the buffer with a new name
//
void FileWidget::saveAs() {
  if (_saveEditorAs) {
    const char *enteredPath = getInputValue(getInput("saveas"));
    if (enteredPath && enteredPath[0]) {
      // a path has been entered
      char savepath[PATH_MAX + 1];
      if (enteredPath[0] == '~') {
        // substitute ~ for $HOME contents
        const char *home = dev_getenv("HOME");
        if (home) {
          strcpy(savepath, home);
        } else {
          savepath[0] = 0;
        }
        strcat(savepath, enteredPath + 1);
      } else if (enteredPath[0] == '/' || enteredPath[1] == ':') {
        // absolute path given
        strcpy(savepath, enteredPath);
      } else {
        strcpy(savepath, _path);
        strcat(savepath, "/");
        strcat(savepath, enteredPath);
      }
      const char *msg = "%s\n\nFile already exists.\nDo you want to replace it?";
      if (access(savepath, 0) != 0 || ask(msg, savepath)) {
        _saveEditorAs->doSaveFile(savepath);
      }
    }
  }
}

