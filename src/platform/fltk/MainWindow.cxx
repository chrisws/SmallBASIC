// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <config.h>
#include <FL/fl_ask.H>
#include "platform/fltk/MainWindow.h"
#include "platform/fltk/EditorWidget.h"
#include "platform/fltk/HelpWidget.h"
#include "platform/fltk/FileWidget.h"
#include "platform/fltk/utils.h"
#include "ui/strlib.h"
#include "common/sbapp.h"
#include "common/sys.h"
#include "common/fs_socket_client.h"
#include "common/keymap.h"

#define DEF_FONT_SIZE 12
#define TAB_BORDER 4

char *packageHome;
char *runfile = 0;
int recentIndex = 0;
int restart = 0;
bool opt_interactive;
int recentMenu[NUM_RECENT_ITEMS];
strlib::String recentPath[NUM_RECENT_ITEMS];
int recentPosition[NUM_RECENT_ITEMS];
MainWindow *wnd;
ExecState runMode = init_state;

const char *fontCache = "fonts.txt";
const char *endFont = ".";
const char *untitledFile = "untitled.bas";
const char *fileTabName = "File";
const char *helpTabName = "Help";
const char *pluginHome = "plugins";
const char *historyFile = "history.txt";
const char *keywordsFile = "keywords.txt";
const char *aboutText =
  "<b>About SmallBASIC...</b><br><br>"
  "Version " SB_STR_VER "<br>"
  "Copyright (c) 2002-2019 Chris Warren-Smith.<br><br>"
  "Copyright (c) 2000-2006 Nicholas Christopoulos.<br><br>"
  "<a href=https://smallbasic.github.io>"
  "https://smallbasic.github.io</a><br><br>"
  "SmallBASIC comes with ABSOLUTELY NO WARRANTY. "
  "This program is free software; you can use it "
  "redistribute it and/or modify it under the terms of the "
  "GNU General Public License version 2 as published by "
  "the Free Software Foundation.<br><br>" "<i>Press F1 for help";

void vsncat(char *buffer, size_t size, ...) {
  va_list args;
  va_start(args, size);
  strlcpy(buffer, va_arg(args, char *), size);
  for (char *next = va_arg(args, char *);
       next != NULL;
       next = va_arg(args, char *)) {
    strlcat(buffer, next, size);
  }
  va_end(args);
}

//--EditWindow functions--------------------------------------------------------

void MainWindow::statusMsg(RunMessage runMessage, const char *statusMessage) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    editWidget->statusMsg(statusMessage);
    editWidget->runState(runMessage);
  }
}

void MainWindow::busyMessage() {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    editWidget->statusMsg("Selection unavailable while program is running.");
  }
}

void MainWindow::pathMessage(const char *file) {
  char message[PATH_MAX];
  snprintf(message, sizeof(message), "File not found: %s", file);
  statusMsg(rs_err, message);
}

void MainWindow::showEditTab(EditorWidget *editWidget) {
  if (editWidget) {
    _tabGroup->value(editWidget->parent());
    editWidget->take_focus();
  }
}

/**
 * run the give file. returns whether break was hit
 */
bool MainWindow::basicMain(EditorWidget *editWidget,
                           const char *fullpath, bool toolExec) {
  int len = strlen(fullpath);
  char path[PATH_MAX];
  bool breakToLine = false;     // whether to restore the editor cursor

  if (strcasecmp(fullpath + len - 4, ".htm") == 0 ||
      strcasecmp(fullpath + len - 5, ".html") == 0) {
    // render html edit buffer
    snprintf(path, sizeof(path), "file:%s", fullpath);
    if (editWidget) {
      editWidget->take_focus();
    }
    return false;
  }
  if (access(fullpath, 0) != 0) {
    pathMessage(fullpath);
    runMode = edit_state;
    return false;
  }
  // start in the directory of the bas program
  const char *filename = FileWidget::splitPath(fullpath, path);

  if (editWidget) {
    _runEditWidget = editWidget;
    if (!toolExec) {
      editWidget->readonly(true);
      editWidget->runState(rs_run);
      breakToLine = editWidget->isBreakToLine();
      opt_ide = editWidget->isHideIDE()? IDE_NONE : IDE_INTERNAL;
    }
  }

  opt_pref_width = 0;
  opt_pref_height = 0;

  Fl_Window *fullScreen = NULL;
  Fl_Group *oldOutputGroup = _outputGroup;
  int old_w = _out->w();
  int old_h = _out->h();
  int interactive = opt_interactive;

  if (!toolExec) {
    if (opt_ide == IDE_NONE) {
      // run in a separate window with the ide hidden
      fullScreen = new BaseWindow(w(), h());
      _profile->restoreAppPosition(fullScreen);

      fullScreen->callback(quit_cb);
      fullScreen->add(_out);
      fullScreen->resizable(fullScreen);
      setTitle(fullScreen, filename);
      _outputGroup = fullScreen;
      resizeDisplay(w(), h());
      hide();
    } else {
      setTitle(this, filename);
    }
  } else {
    opt_interactive = false;
  }

  int success;
  do {
    restart = false;
    runMode = run_state;
    chdir(path);
    success = sbasic_main(filename);
  }
  while (restart);

  opt_interactive = interactive;
  bool was_break = (runMode == break_state);

  if (fullScreen != NULL) {
    _profile->_appPosition = *fullScreen;
    fullScreen->remove(_out);
    delete fullScreen;

    _outputGroup = oldOutputGroup;
    _outputGroup->add(_out);
    resizeDisplay(old_w, old_h);
    show();
  } else {
    copy_label("SmallBASIC");
  }

  if (runMode == quit_state) {
    exit(0);
  }

  if (!success || was_break) {
    if (!toolExec && editWidget && (!was_break || breakToLine)) {
      editWidget->gotoLine(gsb_last_line);
    }
    if (editWidget) {
      showEditTab(editWidget);
      editWidget->runState(was_break ? rs_ready : rs_err);
    }
  } else if (!toolExec && editWidget) {
    // normal termination
    editWidget->runState(rs_ready);
  }

  if (!toolExec && editWidget) {
    editWidget->readonly(false);
  }

  runMode = edit_state;
  _runEditWidget = 0;
  return was_break;
}

//--Menu callbacks--------------------------------------------------------------

void MainWindow::close_tab(Fl_Widget *w, void *eventData) {
  if (_tabGroup->children() > 1) {
    Fl_Group *group = getSelectedTab();
    if (group && group != _outputGroup) {
      if (gw_editor == getGroupWidget(group)) {
        EditorWidget *editWidget = (EditorWidget *)group->child(0);
        if (!editWidget->checkSave(true)) {
          return;
        }
        // check whether the editor is a running program
        if (editWidget == _runEditWidget) {
          setBreak();
          return;
        }
      }
      _tabGroup->remove(group);
      delete group;
    }
  }
}

void MainWindow::close_other_tabs(Fl_Widget *w, void *eventData) {
  Fl_Group *selected = getSelectedTab();
  int n = _tabGroup->children();
  Fl_Group *items[n];
  for (int c = 0; c < n; c++) {
    items[c] = NULL;
    Fl_Group *child = (Fl_Group *)_tabGroup->child(c);
    if (child != selected && gw_editor == getGroupWidget(child)) {
      EditorWidget *editWidget = (EditorWidget *)child->child(0);
      if (editWidget != _runEditWidget && editWidget->checkSave(true)) {
        items[c] = child;
      }
    }
  }
  for (int c = 0; c < n; c++) {
    if (items[c] != NULL) {
      _tabGroup->remove(items[c]);
      delete items[c];
    }
  }
}

void MainWindow::restart_run(Fl_Widget *w, void *eventData) {
  if (runMode == run_state) {
    setBreak();
    restart = true;
  }
}

void MainWindow::quit(Fl_Widget *w, void *eventData) {
  if (runMode == edit_state || runMode == quit_state) {
    // auto-save scratchpad
    int n = _tabGroup->children();
    for (int c = 0; c < n; c++) {
      Fl_Group *group = (Fl_Group *)_tabGroup->child(c);
      char path[PATH_MAX];
      if (gw_editor == getGroupWidget(group)) {
        EditorWidget *editWidget = (EditorWidget *)group->child(0);
        const char *filename = editWidget->getFilename();
        int offs = strlen(filename) - strlen(untitledFile);
        if (filename[0] == 0 || (offs > 0 && strcasecmp(filename + offs, untitledFile) == 0)) {
          getHomeDir(path, sizeof(path));
          strcat(path, untitledFile);
          editWidget->doSaveFile(path);
        } else if (!editWidget->checkSave(true)) {
          return;
        }
      }
    }
    exit(0);
  } else {
    switch (fl_choice("Terminate running program?", "*Exit", "Break", "Cancel")) {
    case 0:
      exit(0);
    case 1:
      setBreak();
    default:
      break;
    }
  }
}

/**
 * opens the smallbasic home page in a browser window
 */
void MainWindow::help_home(Fl_Widget *w, void *eventData) {
  browseFile("http://smallbasic.github.io");
}

/**
 * display the results of help.bas
 */
void MainWindow::showHelpPage() {
  HelpWidget *help = getHelp();
  char path[PATH_MAX];
  getHomeDir(path, sizeof(path));
  help->setDocHome(path);
  strcat(path, "help.html");
  help->loadFile(path);
}

/**
 * when opt_command is a URL, opens the URL in a browser window
 * otherwise runs help.bas with opt_command as the program argument
 */
bool MainWindow::execHelp() {
  bool result = true;
  char path[PATH_MAX];
  if (strncmp(opt_command, "http://", 7) == 0) {
    // launch in real browser
    browseFile(opt_command);
    result = false;
  } else {
    snprintf(path, sizeof(path), "%s/%s/help.bas", packageHome, pluginHome);
    basicMain(getEditor(), path, true);
  }
  return result;
}

/**
 * handle click from within help window
 */
void do_help_contents_anchor(void *){
  Fl::remove_check(do_help_contents_anchor);
  strlib::String eventName = wnd->getHelp()->getEventName();
  if (access(eventName, R_OK) == 0) {
    wnd->editFile(eventName);
  } else {
    strcpy(opt_command, eventName);
    if (wnd->execHelp()) {
      wnd->showHelpPage();
    }
  }
}

void MainWindow::help_contents_anchor(Fl_Widget *w, void *eventData) {
  if (runMode == edit_state) {
    Fl::add_check(do_help_contents_anchor);
  }
}

/**
 * handle f1 context help
 */
void MainWindow::help_contents(Fl_Widget *w, void *eventData) {
  bool showHelp = true;
  if (runMode == edit_state) {
    EditorWidget *editWidget = getEditor();
    if (editWidget && Fl::event_key() != 0) {
      // scan for help context
      int start, end;
      char *selection = editWidget->getSelection(&start, &end);
      strcpy(opt_command, selection);
      free((void *)selection);
    } else {
      opt_command[0] = 0;
    }

    showHelp = execHelp();
  }

  if (!eventData && showHelp) {
    showHelpPage();
  }
}

/**
 * displays the program help page in a browser window
 */
void MainWindow::help_app(Fl_Widget *w, void *eventData) {
  browseFile("https://smallbasic.github.io/pages/reference.html");
}

void MainWindow::help_about(Fl_Widget *w, void *eventData) {
  getHelp()->loadBuffer(aboutText);
}

void MainWindow::export_file(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  static char token[PATH_MAX];
  if (editWidget) {
    if (runMode == edit_state) {
      int handle = 1;
      char buffer[PATH_MAX];
      if (_exportFile.length()) {
        strcpy(buffer, _exportFile.c_str());
      } else {
        buffer[0] = 0;
      }
      editWidget->statusMsg("Enter file/address:  For mobile SmallBASIC use SOCL:<IP>:<Port>");
      editWidget->getInput(buffer, PATH_MAX);
      if (buffer[0]) {
        if (strncasecmp(buffer, "SOCL:", 5) == 0) {
          editWidget->statusMsg("Enter token:");
          editWidget->getInput(token, PATH_MAX);
        } else {
          token[0] = 0;
        }
        _exportFile = buffer;
        if (dev_fopen(handle, _exportFile, DEV_FILE_OUTPUT)) {
          const char *data = editWidget->data();
          if (token[0]) {
            vsncat(buffer, sizeof(buffer), "# ", token, "\n", NULL);
            dev_fwrite(handle, (byte *)buffer, strlen(buffer));
          }
          if (!dev_fwrite(handle, (byte *)data, editWidget->dataLength())) {
            vsncat(buffer, sizeof(buffer), "Failed to write: ", _exportFile.c_str(), NULL);
            statusMsg(rs_err, buffer);
          } else {
            vsncat(buffer, sizeof(buffer), "Exported", editWidget->getFilename(), " to ",
                   _exportFile.c_str(), NULL);
            statusMsg(rs_ready, buffer);
          }
        } else {
          vsncat(buffer, sizeof(buffer), "Failed to open: ", _exportFile.c_str(), NULL);
          statusMsg(rs_err, buffer);
        }
        dev_fclose(handle);
      }
      // cancel setModal() from editWidget->getInput()
      runMode = edit_state;
    } else {
      busyMessage();
    }
  }
}

void MainWindow::set_options(Fl_Widget *w, void *eventData) {
  const char *args = fl_input("Enter program command line", opt_command);
  if (args) {
    strcpy(opt_command, args);
  }
}

void MainWindow::next_tab(Fl_Widget *w, void *eventData) {
  Fl_Group *group = getNextTab(getSelectedTab());
  _tabGroup->value(group);
  EditorWidget *editWidget = getEditor(group);
  if (editWidget) {
    editWidget->take_focus();
  }
}

void MainWindow::prev_tab(Fl_Widget *w, void *eventData) {
  Fl_Group *group = getPrevTab(getSelectedTab());
  _tabGroup->value(group);
  EditorWidget *editWidget = getEditor(group);
  if (editWidget) {
    editWidget->take_focus();
  }
}

void MainWindow::copy_text(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    editWidget->copyText();
  } else {
    handle(EVENT_COPY_TEXT);
  }
}

void MainWindow::font_size_incr(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    int size = editWidget->getFontSize();
    if (size < MAX_FONT_SIZE) {
      editWidget->setFontSize(size + 1);
      updateConfig(editWidget);
      _system->setFontSize(size + 1);
    }
  } else {
    handle(EVENT_INCREASE_FONT);
  }
}

void MainWindow::font_size_decr(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    int size = editWidget->getFontSize();
    if (size > MIN_FONT_SIZE) {
      editWidget->setFontSize(size - 1);
      updateConfig(editWidget);
      _system->setFontSize(size - 1);
    }
  } else {
    handle(EVENT_DECREASE_FONT);
  }
}

void MainWindow::font_cache_clear(Fl_Widget *w, void *eventData) {
  char path[PATH_MAX];
  getHomeDir(path, sizeof(path));
  strcat(path, fontCache);
  unlink(path);
  statusMsg(rs_err, "Restart SmallBASIC to load new fonts");
}

void MainWindow::run(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    const char *filename = editWidget->getFilename();
    if (runMode == edit_state) {
      // inhibit autosave on run function with environment var
      const char *noSave = dev_getenv("NO_RUN_SAVE");
      char path[PATH_MAX];
      if (noSave == 0 || noSave[0] != '1') {
        if (filename == 0 || filename[0] == 0) {
          getHomeDir(path, sizeof(path));
          strcat(path, untitledFile);
          filename = path;
          editWidget->doSaveFile(filename);
        } else if (access(filename, W_OK) == 0) {
          editWidget->doSaveFile(filename);
        }
      }
      basicMain(editWidget, filename, false);
    } else {
      busyMessage();
    }
  }
}

void MainWindow::run_break(Fl_Widget *w, void *eventData) {
  if (runMode == modal_state && !count_tasks()) {
    // break from modal edit mode loop
    runMode = edit_state;
  } else if (runMode == run_state || runMode == modal_state) {
    setBreak();
  }
}

/**
 * run the selected text as the main program
 */
void MainWindow::run_selection(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    char path[PATH_MAX];
    getHomeDir(path, sizeof(path));
    strcat(path, "selection.bas");
    editWidget->saveSelection(path);
    basicMain(editWidget, path, false);
  }
}

/**
 * callback for editor-plug-in plug-ins. we assume the target
 * program will be changing the contents of the editor buffer
 */
void MainWindow::editor_plugin(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    Fl_Text_Editor *editor = editWidget->getEditor();
    char filename[PATH_MAX];
    char path[PATH_MAX];
    strcpy(filename, editWidget->getFilename());

    if (runMode == edit_state) {
      if (editWidget->checkSave(false) && filename[0]) {
        int pos = editor->insert_position();
        int row, col, s1r, s1c, s2r, s2c;
        editWidget->getRowCol(&row, &col);
        editWidget->getSelStartRowCol(&s1r, &s1c);
        editWidget->getSelEndRowCol(&s2r, &s2c);
        snprintf(opt_command, sizeof(opt_command), "%s|%d|%d|%d|%d|%d|%d",
                 filename, row - 1, col, s1r - 1, s1c, s2r - 1, s2c);
        runMode = run_state;
        editWidget->runState(rs_run);
        snprintf(path, sizeof(path), "%s/%s", packageHome, (const char *)eventData);
        int interactive = opt_interactive;
        opt_interactive = false;
        int success = sbasic_main(path);
        opt_interactive = interactive;
        editWidget->runState(success ? rs_ready : rs_err);
        editWidget->loadFile(filename);
        editor->insert_position(pos);
        editor->show_insert_position();
        editWidget->setRowCol(row, col + 1);
        showEditTab(editWidget);
        runMode = edit_state;
        opt_command[0] = 0;
      }
    } else {
      busyMessage();
    }
  }
}

/**
 * callback for tool-plug-in plug-ins.
 */
void MainWindow::tool_plugin(Fl_Widget *w, void *eventData) {
  if (runMode == edit_state) {
    char path[PATH_MAX];
    snprintf(opt_command, sizeof(opt_command), "%s/%s", packageHome, pluginHome);
    statusMsg(rs_ready, (const char *)eventData);
    snprintf(path, sizeof(path), "%s/%s", packageHome, (const char *)eventData);
    _tabGroup->value(_outputGroup);
    basicMain(0, path, true);
    statusMsg(rs_ready, 0);
    opt_command[0] = 0;
  } else {
    busyMessage();
  }
}

void MainWindow::load_file(Fl_Widget *w, void *eventData) {
  int pathIndex = ((intptr_t) eventData) - 1;
  const char *path = recentPath[pathIndex].c_str();
  EditorWidget *editWidget = getEditor(path);
  if (!editWidget) {
    editWidget = getEditor(createEditor(path));
  }

  if (editWidget->checkSave(true)) {
    Fl_Text_Editor *editor = editWidget->getEditor();
    // save current position
    recentPosition[recentIndex] = editor->insert_position();
    recentIndex = pathIndex;
    // load selected file
    if (access(path, 0) == 0) {
      editWidget->loadFile(path);
      // restore previous position
      editor->insert_position(recentPosition[recentIndex]);
      editWidget->fileChanged(true);
      const char *slash = strrchr(path, '/');
      editWidget->parent()->copy_label(slash ? slash + 1 : path);
      showEditTab(editWidget);
    } else {
      pathMessage(path);
    }
  }
}

//--Startup functions-----------------------------------------------------------

/**
 *Adds a plug-in to the menu
 */
void MainWindow::addPlugin(Fl_Menu_Bar *menu, const char *label, const char *filename) {
  char path[PATH_MAX];
  snprintf(path, PATH_MAX, "%s/%s", pluginHome, filename);
  if (access(path, R_OK) == 0) {
    menu->add(label, 0, editor_plugin_cb, strdup(path));
  }
}

/**
 * scan for recent files
 */
void MainWindow::scanRecentFiles(Fl_Menu_Bar *menu) {
  FILE *fp;
  char buffer[PATH_MAX];
  char path[PATH_MAX];
  char label[1024];
  int i = 0;

  getHomeDir(path, sizeof(path));
  strcat(path, historyFile);
  fp = fopen(path, "r");
  if (fp) {
    while (feof(fp) == 0 && fgets(buffer, sizeof(buffer), fp)) {
      buffer[strlen(buffer) - 1] = 0;   // trim new-line
      if (access(buffer, 0) == 0) {
        char *fileLabel = strrchr(buffer, '/');
        if (fileLabel == 0) {
          fileLabel = strrchr(buffer, '\\');
        }
        fileLabel = fileLabel ? fileLabel + 1 : buffer;
        if (fileLabel != 0 && *fileLabel == '_') {
          fileLabel++;
        }
        snprintf(label, sizeof(label), "&File/Open Recent File/%s", fileLabel);
        recentMenu[i] = menu->add(label, FL_CTRL + '1' + i, (Fl_Callback *)load_file_cb, (void *)(intptr_t)(i + 1));
        recentPath[i].append(buffer);
        if (++i == NUM_RECENT_ITEMS) {
          break;
        }
      }
    }
    fclose(fp);
  }
  while (i < NUM_RECENT_ITEMS) {
    snprintf(label, sizeof(label), "&File/Open Recent File/%s", untitledFile);
    recentMenu[i] = menu->add(label, FL_CTRL + '1' + i, (Fl_Callback *)load_file_cb, (void *)(intptr_t)(i + 1));
    recentPath[i].append(untitledFile);
    i++;
  }
}

/**
 * scan for optional plugins
 */
void MainWindow::scanPlugIns(Fl_Menu_Bar *menu) {
  FILE *file;
  char buffer[PATH_MAX];
  char path[PATH_MAX];
  char label[1024];
  DIR *dp;
  struct dirent *e;

  snprintf(path, sizeof(path), "%s/%s", packageHome, pluginHome);
  dp = opendir(path);
  while (dp != NULL) {
    e = readdir(dp);
    if (e == NULL) {
      break;
    }
    const char *filename = e->d_name;
    int len = strlen(filename);
    if (strcasecmp(filename + len - 4, ".bas") == 0) {
      snprintf(path, sizeof(path), "%s/%s/%s", packageHome, pluginHome, filename);
      file = fopen(path, "r");
      if (!file) {
        continue;
      }

      if (!fgets(buffer, PATH_MAX, file)) {
        fclose(file);
        continue;
      }
      bool editorTool = false;
      FileWidget::trimEOL(buffer);
      if (strcmp("'tool-plug-in", buffer) == 0) {
        editorTool = true;
      } else if (strcmp("'app-plug-in", buffer) != 0) {
        fclose(file);
        continue;
      }

      if (fgets(buffer, PATH_MAX, file) && strncmp("'menu", buffer, 5) == 0) {
        FileWidget::trimEOL(buffer);
        int offs = 6;
        while (buffer[offs] && (buffer[offs] == '\t' || buffer[offs] == ' ')) {
          offs++;
        }
        snprintf(label, sizeof(label), (editorTool ? "&Edit/Basic/%s" : "&Basic/%s"), buffer + offs);
        // use an absolute path
        snprintf(path, sizeof(path), "%s/%s", pluginHome, filename);
        menu->add(label, 0, (Fl_Callback *)
                  (editorTool ? editor_plugin_cb : tool_plugin_cb), strdup(path));
      }
      fclose(file);
    }
  }
  // cleanup
  closedir(dp);
}

/**
 * process the program command line arguments
 */
int arg_cb(int argc, char **argv, int &i) {
  const char *s = argv[i];
  int len = strlen(s);
  int c;

  if (strcasecmp(s + len - 4, ".bas") == 0 && access(s, 0) == 0) {
    runfile = strdup(s);
    runMode = run_state;
    i += 1;
    return 1;
  }

  if (argv[i][0] == '-') {
    if (!argv[i][2] && argv[i + 1]) {
      // commands that take an additional file name argument
      switch (argv[i][1]) {
      case 'e':
        runfile = strdup(argv[i + 1]);
        runMode = edit_state;
        i += 2;
        return 1;

      case 'r':
        runfile = strdup(argv[i + 1]);
        runMode = run_state;
        i += 2;
        return 1;

      case 'm':
        opt_loadmod = 1;
        strcpy(opt_modpath, argv[i + 1]);
        i += 2;
        return 1;
      }
    }

    switch (argv[i][1]) {
    case 'n':
      i += 1;
      opt_interactive = true;
      return 1;

    case 'v':
      i += 1;
      opt_verbose = 1;
      opt_quiet = 0;
      return 1;

    case '-':
      // echo foo | sbasic foo.bas --
      while ((c = fgetc(stdin)) != EOF) {
        int len = strlen(opt_command);
        opt_command[len] = c;
        opt_command[len + 1] = 0;
      }
      i++;
      return 1;
    }
  }

  if (runMode == run_state) {
    // remaining text is .bas program argument
    if (opt_command[0]) {
      strcat(opt_command, " ");
    }
    strcat(opt_command, s);
    i++;
    return 1;
  }

  return 0;
}

/**
 * start the application in run mode
 */
void run_mode_startup(void *data) {
  if (data) {
    Fl_Window *w = (Fl_Window *)data;
    delete w;
  }

  EditorWidget *editWidget = wnd->getEditor(true);
  if (editWidget) {
    editWidget->setHideIde(true);
    editWidget->loadFile(runfile);

    opt_ide = IDE_NONE;
    if (!wnd->basicMain(0, runfile, false)) {
      exit(0);
    }
    editWidget->getEditor()->take_focus();
    opt_ide = IDE_INTERNAL;
  }
}

/**
 * prepare to start the application
 */
bool initialise(int argc, char **argv) {
  opt_graphics = 1;
  opt_quiet = 1;
  opt_verbose = 0;
  opt_nosave = 1;
  opt_ide = IDE_INTERNAL;
  opt_interactive = true;
  opt_file_permitted = 1;
  os_graphics = 1;
  opt_mute_audio = 1;

  int i = 0;
  if (Fl::args(argc, argv, i, arg_cb) < argc) {
    fl_message("Options are:\n"
               " -e[dit] file.bas\n"
               " -r[un] file.bas\n"
               " -v[erbose]\n"
               " -n[on]-interactive\n"
               " -m[odule]-home");
    return false;
  }

  // package home contains installed components
#if defined(WIN32)
  packageHome = strdup(argv[0]);
  char *slash = (char *)FileWidget::forwardSlash(packageHome);
  if (slash) {
    *slash = 0;
  }
#else
  packageHome = (char *)PACKAGE_DATA_DIR;
#endif
  dev_setenv("PKG_HOME", packageHome);

  // bas_home contains user editable files along with generated help
  char path[PATH_MAX];
  getHomeDir(path, sizeof(path), false);
  dev_setenv("BAS_HOME", path);

  wnd = new MainWindow(800, 650);

  // load startup editors
  wnd->new_file(0, 0);
  wnd->_profile->restore(wnd);

  Fl::scheme("gtk+");
  Fl_Window::default_xclass("smallbasic");
  fl_message_title("SmallBASIC");
  wnd->loadIcon(PACKAGE_PREFIX, 101);
  Fl::wait(0);

  Fl_Window *run_wnd;

  switch (runMode) {
  case run_state:
    run_wnd = new Fl_Window(0, 0);
    run_wnd->show();
    Fl::add_timeout(0.5f, run_mode_startup, run_wnd);
    break;

  case edit_state:
    wnd->getEditor(true)->loadFile(runfile);
    break;

  default:
    runMode = edit_state;
  }

  if (runMode != run_state) {
    wnd->show(argc, argv);
  }

  return true;
}

/**
 * save application state at program exit
 */
void save_profile(void) {
  wnd->_profile->save(wnd);
}

/**
 * application entry point
 */
int main(int argc, char **argv) {
  int result;
  if (initialise(argc, argv)) {
    atexit(save_profile);
    Fl::run();
    result = 0;
  } else {
    result = 1;
  }
  return result;
}

//--MainWindow methods----------------------------------------------------------

MainWindow::MainWindow(int w, int h) :
  BaseWindow(w, h) {
  _runEditWidget = 0;
  _profile = new Profile();
  size_range(250, 250);

  FileWidget::forwardSlash(runfile);
  begin();
  Fl_Menu_Bar *m = _menuBar = new Fl_Menu_Bar(0, 0, w, MENU_HEIGHT);
  m->add("&File/&New File", FL_CTRL + 'n', new_file_cb);
  m->add("&File/&Open File", FL_CTRL + 'o', open_file_cb);
  m->add("&File/_Open Recent File/", 0, (Fl_Callback *)NULL);
  scanRecentFiles(m);
  m->add("&File/&Close", FL_CTRL + FL_F+4, close_tab_cb);
  m->add("&File/_&Close Others", 0, close_other_tabs_cb);
  m->add("&File/&Save File", FL_CTRL + 's', EditorWidget::save_file_cb);
  m->add("&File/_Save File &As", FL_CTRL + FL_SHIFT + 'S', save_file_as_cb);
  addPlugin(m, "&File/Publish Online", "publish.bas");
  m->add("&File/_Export", FL_CTRL + FL_F+9, export_file_cb);
  m->add("&File/E&xit", FL_CTRL + 'q', quit_cb);
  m->add("&Edit/_&Undo", FL_CTRL + 'z', EditorWidget::undo_cb);
  m->add("&Edit/Cu&t", FL_CTRL + 'x', EditorWidget::cut_text_cb);
  m->add("&Edit/&Copy", FL_CTRL + 'c', copy_text_cb);
  m->add("&Edit/_&Paste", FL_CTRL + 'v', EditorWidget::paste_text_cb);
  m->add("&Edit/_&Select All", FL_CTRL + 'a', EditorWidget::select_all_cb);
  m->add("&Edit/&Change Case", FL_ALT + 'c', EditorWidget::change_case_cb);
  m->add("&Edit/&Expand Word", FL_ALT + '/', EditorWidget::expand_word_cb);
  m->add("&Edit/_&Rename Word", FL_CTRL + FL_SHIFT + 'r', EditorWidget::rename_word_cb);
  m->add("&Edit/&Find", FL_CTRL + 'f', EditorWidget::find_cb);
  m->add("&Edit/&Replace", FL_CTRL + 'r', EditorWidget::show_replace_cb);
  m->add("&Edit/_&Goto Line", FL_CTRL + 'g', EditorWidget::goto_line_cb);
  m->add("&View/&Next Tab", FL_F+6, next_tab_cb);
  m->add("&View/_&Prev Tab", FL_CTRL + FL_F+6, prev_tab_cb);
  m->add("&View/Text Size/&Increase", FL_CTRL + ']', font_size_incr_cb);
  m->add("&View/Text Size/&Decrease", FL_CTRL + '[', font_size_decr_cb);
  m->add("&View/Text Color/Background Def", 0, EditorWidget::set_color_cb, (void *)st_background_def);
  m->add("&View/Text Color/_Background", 0, EditorWidget::set_color_cb, (void *)st_background);
  m->add("&View/Text Color/Text", 0, EditorWidget::set_color_cb, (void *)st_text);
  m->add("&View/Text Color/Comments", 0, EditorWidget::set_color_cb, (void *)st_comments);
  m->add("&View/Text Color/_Strings", 0, EditorWidget::set_color_cb, (void *)st_strings);
  m->add("&View/Text Color/Keywords", 0, EditorWidget::set_color_cb, (void *)st_keywords);
  m->add("&View/Text Color/Funcs", 0, EditorWidget::set_color_cb, (void *)st_funcs);
  m->add("&View/Text Color/_Subs", 0, EditorWidget::set_color_cb, (void *)st_subs);
  m->add("&View/Text Color/Numbers", 0, EditorWidget::set_color_cb, (void *)st_numbers);
  m->add("&View/Text Color/Operators", 0, EditorWidget::set_color_cb, (void *)st_operators);
  m->add("&View/Text Color/Find Matches", 0, EditorWidget::set_color_cb, (void *)st_findMatches);

  scanPlugIns(m);

  m->add("&Program/&Run", FL_F+9, run_cb);
  m->add("&Program/_&Run Selection", FL_F+8, run_selection_cb);
  m->add("&Program/&Break", FL_CTRL + 'b', run_break_cb);
  m->add("&Program/_&Restart", FL_CTRL + 'r', restart_run_cb);
  m->add("&Program/&Command", FL_F+10, set_options_cb);
  m->add("&Help/_&Help Contents", FL_F+1, help_contents_cb);
  m->add("&Help/&Program Help", FL_F+11, help_app_cb);
  m->add("&Help/_&Home Page", 0, help_home_cb);
  m->add("&Help/&About SmallBASIC", FL_F+12, help_about_cb);

  callback(quit_cb);

  int x1 = 0;
  int y1 = MENU_HEIGHT + TAB_BORDER;
  int x2 = w;
  int y2 = h - MENU_HEIGHT - TAB_BORDER;

  // group for all tabs
  _tabGroup = new Fl_Tabs(x1, y1, x2, y2);

  // create the output tab
  y1 += MENU_HEIGHT;
  y2 -= MENU_HEIGHT;
  _outputGroup = new Fl_Group(x1, y1, x2, y2, "Output");
  _outputGroup->labelfont(FL_HELVETICA);
  _outputGroup->user_data((void *)gw_output);
  _out = new GraphicsWidget(x1, y1 + 1, x2, y2 -1);
  _system = new Runtime(x2, y2 - 1, DEF_FONT_SIZE);
  _outputGroup->resizable(_out);
  _outputGroup->end();
  _tabGroup->resizable(_outputGroup);
  _tabGroup->end();

  end();
  resizable(_tabGroup);
}

MainWindow::~MainWindow() {
  delete _system;
}

Fl_Group *MainWindow::createTab(GroupWidgetEnum groupWidgetEnum, const char *label) {
  _tabGroup->begin();
  Fl_Group * result = new Fl_Group(_out->x(), _out->y(), _out->w(), _out->h(), label);
  result->box(FL_NO_BOX);
  result->labelfont(FL_HELVETICA);
  result->user_data((void *)groupWidgetEnum);
  result->begin();
  return result;
}

/**
 * create a new help widget and add it to the tab group
 */
Fl_Group *MainWindow::createEditor(const char *title) {
  const char *slash = strrchr(title, '/');
  Fl_Group *editGroup = createTab(gw_editor, slash ? slash + 1 : title);
  editGroup->resizable(new EditorWidget(_out, _menuBar));
  editGroup->end();
  _tabGroup->add(editGroup);
  _tabGroup->value(editGroup);
  _tabGroup->end();
  return editGroup;
}

void MainWindow::new_file(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = 0;
  Fl_Group *untitledEditor = findTab(untitledFile);
  char path[PATH_MAX];

  if (untitledEditor) {
    _tabGroup->value(untitledEditor);
    editWidget = getEditor(untitledEditor);
  }
  if (!editWidget) {
    editWidget = getEditor(createEditor(untitledFile));

    // preserve the contents of any existing untitled.bas
    getHomeDir(path, sizeof(path));
    strcat(path, untitledFile);
    if (access(path, 0) == 0) {
      editWidget->loadFile(path);
    }
  }

  editWidget->selectAll();
}

void MainWindow::open_file(Fl_Widget *w, void *eventData) {
  FileWidget *fileWidget = NULL;
  Fl_Group *openFileGroup = findTab(gw_file);
  if (!openFileGroup) {
    openFileGroup = createTab(gw_file, fileTabName);
    fileWidget = new FileWidget(_out);
    openFileGroup->resizable(fileWidget);
    openFileGroup->end();
    _tabGroup->end();
  } else {
    fileWidget = (FileWidget *)openFileGroup->resizable();
  }

  // change to the directory of the current editor widget
  EditorWidget *editWidget = getEditor(false);
  char path[PATH_MAX];

  if (editWidget) {
    FileWidget::splitPath(editWidget->getFilename(), path);
  } else {
    Fl_Group *group = (Fl_Group *)_tabGroup->value();
    GroupWidgetEnum gw = getGroupWidget(group);
    switch (gw) {
    case gw_output:
      strcpy(path, packageHome);
      break;
    case gw_help:
      getHomeDir(path, sizeof(path));
      break;
    default:
      path[0] = 0;
    }
  }

  StringList *paths = new StringList();
  for (int i = 0; i < NUM_RECENT_ITEMS; i++) {
    char nextPath[PATH_MAX];
    FileWidget::splitPath(recentPath[i].c_str(), nextPath);
    if (nextPath[0] && !paths->contains(nextPath)) {
      paths->add(nextPath);
    }
  }

  fileWidget->openPath(path, paths);
  _tabGroup->value(openFileGroup);
}

void MainWindow::save_file_as(Fl_Widget *w, void *eventData) {
  EditorWidget *editWidget = getEditor();
  if (editWidget) {
    open_file(w, eventData);
    FileWidget *fileWidget = (FileWidget *)getSelectedTab()->resizable();
    fileWidget->fileOpen(editWidget);
  }
}

HelpWidget *MainWindow::getHelp() {
  HelpWidget *help = 0;
  Fl_Group *helpGroup = findTab(gw_help);
  if (!helpGroup) {
    helpGroup = createTab(gw_help, helpTabName);
    help = new HelpWidget(_out);
    help->callback(help_contents_anchor_cb);
    helpGroup->resizable(help);
    _tabGroup->end();
  } else {
    help = (HelpWidget *)helpGroup->resizable();
  }
  _tabGroup->value(helpGroup);
  return help;
}

EditorWidget *MainWindow::getEditor(bool select) {
  EditorWidget *result = 0;
  if (select) {
    int n = _tabGroup->children();
    for (int c = 0; c < n; c++) {
      Fl_Group *group = (Fl_Group *)_tabGroup->child(c);
      if (gw_editor == getGroupWidget(group)) {
        result = (EditorWidget *)group->child(0);
        _tabGroup->value(group);
        break;
      }
    }
  } else {
    result = getEditor(getSelectedTab());
  }
  return result;
}

EditorWidget *MainWindow::getEditor(Fl_Group *group) {
  EditorWidget *editWidget = 0;
  if (group != 0 && gw_editor == getGroupWidget(group)) {
    editWidget = (EditorWidget *)group->resizable();
  }
  return editWidget;
}

EditorWidget *MainWindow::getEditor(const char *fullpath) {
  if (fullpath != 0 && fullpath[0] != 0) {
    int n = _tabGroup->children();
    for (int c = 0; c < n; c++) {
      Fl_Group *group = (Fl_Group *)_tabGroup->child(c);
      if (gw_editor == getGroupWidget(group)) {
        EditorWidget *editWidget = (EditorWidget *)group->child(0);
        const char *fileName = editWidget->getFilename();
        if (fileName && strcmp(fullpath, fileName) == 0) {
          return editWidget;
        }
      }
    }
  }
  return NULL;
}

/**
 * called by FileWidget to open the selected file
 */
void MainWindow::editFile(const char *filePath) {
  EditorWidget *editWidget = getEditor(filePath);
  if (!editWidget) {
    editWidget = getEditor(createEditor(filePath));
    editWidget->loadFile(filePath);
  }
  showEditTab(editWidget);
}

Fl_Group *MainWindow::getSelectedTab() {
  return (Fl_Group *)_tabGroup->value();
}

/**
 * returns the tab with the given name
 */
Fl_Group *MainWindow::findTab(const char *label) {
  int n = _tabGroup->children();
  for (int c = 0; c < n; c++) {
    Fl_Group *child = (Fl_Group *)_tabGroup->child(c);
    if (strcmp(child->label(), label) == 0) {
      return child;
    }
  }
  return 0;
}

Fl_Group *MainWindow::findTab(GroupWidgetEnum groupWidget) {
  int n = _tabGroup->children();
  for (int c = 0; c < n; c++) {
    Fl_Group *child = (Fl_Group *)_tabGroup->child(c);
    if (groupWidget == getGroupWidget(child)) {
      return child;
    }
  }
  return 0;
}

/**
 * find and select the tab with the given tab label
 */
Fl_Group *MainWindow::selectTab(const char *label) {
  Fl_Group *tab = findTab(label);
  if (tab) {
    _tabGroup->value(tab);
  }
  return tab;
}

/**
 * copies the configuration from the current editor to any remaining editors
 */
void MainWindow::updateConfig(EditorWidget *current) {
  int n = _tabGroup->children();
  for (int c = 0; c < n; c++) {
    Fl_Group *group = (Fl_Group *)_tabGroup->child(c);
    if (gw_editor == getGroupWidget(group)) {
      EditorWidget *editWidget = (EditorWidget *)group->child(0);
      if (editWidget != current) {
        editWidget->updateConfig(current);
      }
    }
  }
}

/**
 * updates the names of the editor tabs based on the enclosed editing file
 */
void MainWindow::updateEditTabName(EditorWidget *editWidget) {
  int n = _tabGroup->children();
  for (int c = 0; c < n; c++) {
    Fl_Group *group = (Fl_Group *)_tabGroup->child(c);
    if (gw_editor == getGroupWidget(group) && editWidget == (EditorWidget *)group->child(0)) {
      const char *editFileName = editWidget->getFilename();
      if (editFileName && editFileName[0]) {
        const char *slash = strrchr(editFileName, '/');
        group->copy_label(slash ? slash + 1 : editFileName);
      }
    }
  }
}

/**
 * returns the tab following the given tab
 */
Fl_Group *MainWindow::getNextTab(Fl_Group *current) {
  int n = _tabGroup->children();
  for (int c = 0; c < n - 1; c++) {
    Fl_Group *child = (Fl_Group *)_tabGroup->child(c);
    if (child == current) {
      return (Fl_Group *)_tabGroup->child(c + 1);
    }
  }
  return (Fl_Group *)_tabGroup->child(0);
}

/**
 * returns the tab prior the given tab or null if not found
 */
Fl_Group *MainWindow::getPrevTab(Fl_Group *current) {
  int n = _tabGroup->children();
  for (int c = n - 1; c > 0; c--) {
    Fl_Group *child = (Fl_Group *)_tabGroup->child(c);
    if (child == current) {
      return (Fl_Group *)_tabGroup->child(c - 1);
    }
  }
  return (Fl_Group *)_tabGroup->child(n - 1);
}

/**
 * Opens the config file ready for writing
 */
FILE *MainWindow::openConfig(const char *fileName, const char *flags) {
  char path[PATH_MAX];
  getHomeDir(path, sizeof(path));
  strcat(path, fileName);
  return fopen(path, flags);
}

bool MainWindow::isBreakExec(void) {
  return (runMode == break_state || runMode == quit_state);
}

bool MainWindow::isRunning(void) {
  return (runMode == run_state || runMode == modal_state);
}

void MainWindow::setModal(bool modal) {
  runMode = modal ? modal_state : run_state;
}

/**
 * sets the window title based on the filename
 */
void MainWindow::setTitle(Fl_Window *widget, const char *filename) {
  char title[PATH_MAX];
  const char *dot = strrchr(filename, '.');
  int len = (dot ? dot - filename : strlen(filename));

  strncpy(title, filename, len);
  title[len] = 0;
  title[0] = toupper(title[0]);
  strcat(title, " - SmallBASIC");
  widget->copy_label(title);
}

void MainWindow::setBreak() {
  brun_break();
  runMode = break_state;
}

bool MainWindow::isModal() {
  return (runMode == modal_state);
}

bool MainWindow::isEdit() {
  return (runMode == edit_state);
}

bool MainWindow::isInteractive() {
  return opt_interactive;
}

bool MainWindow::isIdeHidden() {
  return (opt_ide == IDE_NONE);
}

void MainWindow::resetPen() {
  _penDownX = 0;
  _penDownY = 0;
  _penMode = 0;
}

void MainWindow::resizeDisplay(int w, int h) {
  _out->resize(w, h - 1);
  _system->resize(w, h - 1);
}

/**
 * returns any active tty widget
 */
TtyWidget *MainWindow::tty() {
  TtyWidget *result = 0;
  EditorWidget *editor = _runEditWidget;
  if (!editor) {
    editor = getEditor(false);
  }
  if (editor) {
    result = editor->getTty();
  }
  return result;
}

/**
 * returns whether printing to the tty widget is active
 */
bool MainWindow::logPrint() {
  return (_runEditWidget && _runEditWidget->getTty() && _runEditWidget->isLogPrint());
}

void MainWindow::execLink(strlib::String &link) {
  if (!link.length()) {
    return;
  }

  char *file = (char *)link.c_str();
  EditorWidget *editWidget = getEditor(true);
  _siteHome.clear();
  bool execFile = false;
  if (file[0] == '!' || file[0] == '|') {
    execFile = true;            // exec flag passed with name
    file++;
  }
  // execute a link from the html window
  if (0 == strncasecmp(file, "http://", 7)) {
    char localFile[PATH_MAX];
    char path[PATH_MAX];
    dev_file_t df;

    memset(&df, 0, sizeof(dev_file_t));
    strcpy(df.name, file);
    if (http_open(&df) == 0) {
      snprintf(localFile, sizeof(localFile), "Failed to open URL: %s", file);
      statusMsg(rs_ready, localFile);
      return;
    }

    bool httpOK = cacheLink(&df, localFile, sizeof(localFile));
    char *extn = strrchr(file, '.');
    if (!editWidget) {
      editWidget = getEditor(createEditor(file));
    }

    if (httpOK && extn && 0 == strncasecmp(extn, ".bas", 4)) {
      // run the remote program
      editWidget->loadFile(localFile);
      basicMain(editWidget, localFile, false);
    } else {
      // display as html
      int len = strlen(localFile);
      if (strcasecmp(localFile + len - 4, ".gif") == 0 ||
          strcasecmp(localFile + len - 4, ".jpeg") == 0 ||
          strcasecmp(localFile + len - 4, ".jpg") == 0) {
        snprintf(path, sizeof(path), "<img src=%s>", localFile);
      } else {
        snprintf(path, sizeof(path), "file:%s", localFile);
      }
      _siteHome.append(df.name, df.drv_dw[1]);
      statusMsg(rs_ready, _siteHome.c_str());
    }
    return;
  }

  char *colon = strrchr(file, ':');
  if (colon && colon - 1 != file) {
    file = colon + 1;           // clean 'file:' but not 'c:'
  }

  char *extn = strrchr(file, '.');
  if (extn && (0 == strncasecmp(extn, ".bas ", 5) ||
               0 == strncasecmp(extn, ".sbx ", 5))) {
    strcpy(opt_command, extn + 5);
    extn[4] = 0;                // make args available to bas program
  }
  // if the extension is .sbx and this does not exists or is older
  // than the matching .bas then rename to .bas and set opt_nosave
  // to false - otherwise set execFile flag to true
  if (extn && 0 == strncasecmp(extn, ".sbx", 4)) {
    struct stat st_sbx;
    struct stat st_bas;
    bool sbxExists = (stat(file, &st_sbx) == 0);
    strcpy(extn + 1, "bas");    // remains .bas unless sbx valid
    opt_nosave = 0;             // create/use sbx files
    if (sbxExists) {
      if (stat(file, &st_bas) == 0 && st_sbx.st_mtime > st_bas.st_mtime) {
        strcpy(extn + 1, "sbx");
        // sbx exists and is newer than .bas
        execFile = true;
      }
    }
  }
  if (access(file, 0) == 0) {
    statusMsg(rs_ready, file);
    if (execFile) {
      basicMain(0, file, false);
      opt_nosave = 1;
    } else {
      if (!editWidget) {
        editWidget = getEditor(createEditor(file));
      }
      editWidget->loadFile(file);
      showEditTab(editWidget);
    }
  } else {
    pathMessage(file);
  }
}

int MainWindow::handle(int e) {
  int result;
  if (getSelectedTab() == _outputGroup && _system->handle(e)) {
    result = 1;
  } else {
    result = BaseWindow::handle(e);
  }
  return result;
}

/**
 * loads the desktop icon
 */
void MainWindow::loadIcon(const char *prefix, int resourceId) {
  if (!icon()) {
#if defined(WIN32)
    HICON ico = (HICON) wnd->icon();
    if (!ico) {
      ico = (HICON) LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(resourceId),
                              IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR | LR_SHARED);
      if (!ico) {
        ico = LoadIcon(NULL, IDI_APPLICATION);
      }
    }
    wnd->icon((char *)ico);
#else
    char buffer[PATH_MAX];
    char path[PATH_MAX];
    const char *key = "Icon=";

    // read the application desktop file, then scan for the Icon file
    snprintf(path, sizeof(path), "%s/share/applications/smallbasic.desktop", prefix);
    FILE *fp = fopen(path, "r");
    if (fp) {
      while (feof(fp) == 0 && fgets(buffer, sizeof(buffer), fp)) {
        buffer[strlen(buffer) - 1] = 0; // trim new-line
        if (strncasecmp(buffer, key, strlen(key)) == 0) {
          // found icon spec
          const char *filename = buffer + strlen(key);
          Fl_Image *ico = Fl_Shared_Image::get(filename);
          // TODO: fixme
          if (ico) {
            if (sizeof(unsigned) == ico->d()) {
              // prefix the buffer with unsigned width and height values
              unsigned size = ico->w() * ico->h() * ico->d();
              unsigned *image = (unsigned *)malloc(size + sizeof(unsigned) * 2);
              image[0] = ico->w();
              image[1] = ico->h();
              memcpy(image + 2, ico->data(), size);
              icon(image);
            }
            delete ico;
          }
          break;
        }
      }
      fclose(fp);
    }
#endif
  }
}

int BaseWindow::handle(int e) {
  switch (runMode) {
  case run_state:
  case modal_state:
    switch (e) {
    case FL_FOCUS:
      // accept key events into handleKeyEvent
      return 1;
    case FL_PUSH:
      if (keymap_invoke(SB_KEY_MK_PUSH)) {
        return 1;
      }
      break;
    case FL_DRAG:
      if (keymap_invoke(SB_KEY_MK_DRAG)) {
        return 1;
      }
      break;
    case FL_MOVE:
      if (keymap_invoke(SB_KEY_MK_MOVE)) {
        return 1;
      }
      break;
    case FL_RELEASE:
      if (keymap_invoke(SB_KEY_MK_RELEASE)) {
        return 1;
      }
      break;
    case FL_MOUSEWHEEL:
      if (keymap_invoke(SB_KEY_MK_WHEEL)) {
        return 1;
      }
      break;
    case FL_SHORTCUT:
    case FL_KEYBOARD:
      if (handleKeyEvent()) {
        // no default processing by Window
        return 1;
      }
      break;
    }
    break;

  case edit_state:
    switch (e) {
    case FL_SHORTCUT:
    case FL_KEYBOARD:
      if (Fl::event_state(FL_CTRL)) {
        EditorWidget *editWidget = wnd->getEditor();
        if (editWidget) {
          if (Fl::event_key() == FL_F+1) {
            // FL_CTRL + F1 key for brief log mode help
            wnd->help_contents(0, (void *)true);
            return 1;
          }
          if (editWidget->focusWidget()) {
            return 1;
          }
        }
      }
    }
    break;

  default:
    break;
  }

  return Fl_Window::handle(e);
}

bool BaseWindow::handleKeyEvent() {
  int k = Fl::event_key();
  bool key_pushed = true;

  switch (k) {
  case FL_Tab:
    dev_pushkey(SB_KEY_TAB);
    break;
  case FL_Home:
    dev_pushkey(SB_KEY_KP_HOME);
    break;
  case FL_End:
    dev_pushkey(SB_KEY_END);
    break;
  case FL_Insert:
    dev_pushkey(SB_KEY_INSERT);
    break;
  case FL_Menu:
    dev_pushkey(SB_KEY_MENU);
    break;
    //TODO: fixme
    //case FL_Multiply:
    //dev_pushkey(SB_KEY_KP_MUL);
    //break;
    //case AddKey:
    //dev_pushkey(SB_KEY_KP_PLUS);
    //break;
    //case SubtractKey:
    //dev_pushkey(SB_KEY_KP_MINUS);
    //break;
    //case DivideKey:
    //dev_pushkey(SB_KEY_KP_DIV);
    //break;
  case FL_F:
    dev_pushkey(SB_KEY_F(0));
    break;
  case FL_F+1:
    dev_pushkey(SB_KEY_F(1));
    break;
  case FL_F+2:
    dev_pushkey(SB_KEY_F(2));
    break;
  case FL_F+3:
    dev_pushkey(SB_KEY_F(3));
    break;
  case FL_F+4:
    dev_pushkey(SB_KEY_F(4));
    break;
  case FL_F+5:
    dev_pushkey(SB_KEY_F(5));
    break;
  case FL_F+6:
    dev_pushkey(SB_KEY_F(6));
    break;
  case FL_F+7:
    dev_pushkey(SB_KEY_F(7));
    break;
  case FL_F+8:
    dev_pushkey(SB_KEY_F(8));
    break;
  case FL_F+9:
    dev_pushkey(SB_KEY_F(9));
    break;
  case FL_F+10:
    dev_pushkey(SB_KEY_F(10));
    break;
  case FL_F+11:
    dev_pushkey(SB_KEY_F(11));
    break;
  case FL_F+12:
    dev_pushkey(SB_KEY_F(12));
    break;
  case FL_Page_Up:
    dev_pushkey(SB_KEY_PGUP);
    break;
  case FL_Page_Down:
    dev_pushkey(SB_KEY_PGDN);
    break;
  case FL_Up:
    dev_pushkey(SB_KEY_UP);
    break;
  case FL_Down:
    dev_pushkey(SB_KEY_DN);
    break;
  case FL_Left:
    dev_pushkey(SB_KEY_LEFT);
    break;
  case FL_Right:
    dev_pushkey(SB_KEY_RIGHT);
    break;
  case FL_BackSpace:
    dev_pushkey(SB_KEY_BACKSPACE);
    break;
  case FL_Delete:
    dev_pushkey(SB_KEY_DELETE);
    break;
  case FL_Enter:
  case FL_KP_Enter:
    dev_pushkey(13);
    break;
  case 'b':
    if (Fl::event_state(FL_CTRL)) {
      wnd->run_break();
      key_pushed = false;
      break;
    }
    dev_pushkey(Fl::event_text()[0]);
    break;
  case 'q':
    if (Fl::event_state(FL_CTRL)) {
      wnd->quit();
      key_pushed = false;
      break;
    }
    dev_pushkey(Fl::event_text()[0]);
    break;

  default:
    //TODO: fixme
    //if (k >= LeftShiftKey && k <= RightAltKey) {
    // avoid pushing meta-keys
    //key_pushed = false;
    //break;
    //}
    if (Fl::event_state(FL_CTRL & FL_ALT)) {
      dev_pushkey(SB_KEY_CTRL_ALT(k));
    } else if (Fl::event_state(FL_CTRL)) {
      dev_pushkey(SB_KEY_CTRL(k));
    } else if (Fl::event_state(FL_ALT)) {
      dev_pushkey(SB_KEY_ALT(k));
    } else {
      dev_pushkey(Fl::event_text()[0]);
    }
    break;
  }
  return key_pushed;
}

LineInput::LineInput(int x, int y, int w, int h) :
  Fl_Input(x, y, w, h),
  orig_x(x),
  orig_y(y),
  orig_w(w),
  orig_h(h) {
  when(FL_WHEN_ENTER_KEY);
  box(FL_BORDER_BOX);
  fl_color(fl_rgb_color(220, 220, 220));
  take_focus();
}

/**
 * veto the layout changes
 */
void LineInput::resize(int x, int y, int w, int h) {
  Fl_Input::resize(orig_x, orig_y, orig_w, orig_h);
}

int LineInput::handle(int event) {
  int result;
  if (event == FL_KEYBOARD) {
    if (Fl::event_state(FL_CTRL) && Fl::event_key() == 'b') {
      if (!wnd->isEdit()) {
        wnd->setBreak();
      }
    } else if (Fl::event_key(FL_Escape)) {
      do_callback();
    } else {
      // grow the input box width as text is entered
      const char *text = value();
      int strw = fl_width(text) + fl_width(value()) + 4;
      if (strw > w()) {
        w(strw);
        orig_w = strw;
        redraw();
      }
    }
    result = Fl_Input::handle(event);
  } else {
    result = Fl_Input::handle(event);
  }
  return result;
}
