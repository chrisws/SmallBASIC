// This file is part of SmallBASIC
//
// Copyright(C) 2001-2019 Chris Warren-Smith.
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <config.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <FL/fl_ask.H>
#include <FL/Fl_Color_Chooser.H>
#include "platform/fltk/MainWindow.h"
#include "platform/fltk/EditorWidget.h"
#include "platform/fltk/FileWidget.h"
#include "common/smbas.h"

#define TTY_ROWS 1000

// in MainWindow.cxx
extern String recentPath[];
extern int recentMenu[];
extern const char *historyFile;
extern const char *untitledFile;

// in BasicEditor.cxx
extern Fl_Text_Display::Style_Table_Entry styletable[];
extern Fl_Color defaultColor[];

int completionIndex = 0;

static bool rename_active = false;
const char scanLabel[] = "(Refresh)";

EditorWidget *get_editor() {
  EditorWidget *result = wnd->getEditor();
  if (!result) {
    result = wnd->getEditor(true);
  }
  return result;
}

//--EditorWidget----------------------------------------------------------------

EditorWidget::EditorWidget(Fl_Widget *rect, Fl_Menu_Bar *menuBar) :
  Fl_Group(rect->x(), rect->y(), rect->w(), rect->h()),
  editor(NULL),
  tty(NULL),
  dirty(false),
  loading(false),
  modifiedTime(0),
  commandText(NULL),
  rowStatus(NULL),
  colStatus(NULL),
  runStatus(NULL),
  modStatus(NULL),
  funcList(NULL),
  logPrintBn(NULL),
  lockBn(NULL),
  hideIdeBn(NULL),
  gotoLineBn(NULL),
  commandOpt(cmd_find),
  commandChoice(NULL),
  _menuBar(menuBar) {

  filename[0] = 0;
  box(FL_NO_BOX);
  begin();

  int tileHeight = rect->h() - (MNU_HEIGHT + 8);
  int ttyHeight = rect->h() / 8;
  int editHeight = tileHeight - ttyHeight;
  int browserWidth = rect->w() / 8;

  Fl_Group *tile = new Fl_Group(rect->x(), rect->y(), rect->w(), tileHeight);
  editor = new BasicEditor(rect->x(), rect->y(), rect->w() - browserWidth, editHeight, this);
  editor->linenumber_width(40);
  editor->wrap_mode(true, 0);
  editor->selection_color(fl_rgb_color(190, 189, 188));
  editor->_textbuf->add_modify_callback(changed_cb, this);
  editor->box(FL_NO_BOX);
  editor->take_focus();

  // sub-func jump droplist
  funcList = new Fl_Browser(editor->w(), rect->y(), browserWidth, editHeight);
  funcList->labelfont(FL_HELVETICA);
  funcList->when(FL_WHEN_RELEASE);
  funcList->add(scanLabel);

  tty = new TtyWidget(rect->x(), rect->y() + editHeight, rect->w(), ttyHeight, TTY_ROWS);
  tty->color(FL_WHITE);            // bg
  tty->labelcolor(FL_BLACK);       // fg

  tile->end();

  // editor status bar
  Fl_Group *statusBar = new Fl_Group(rect->x(), rect->y() + ttyHeight + 2, rect->w(), MNU_HEIGHT);
  statusBar->box(FL_NO_BOX);

  // widths become relative when the outer window is resized
  int st_w = 40;
  int bn_w = 18;
  int bn_y = rect->h() + 2;
  int st_h = statusBar->h() - 2;

  logPrintBn = new Fl_Toggle_Button(rect->w() - (bn_w + 6), bn_y, bn_w, st_h);
  lockBn = new Fl_Toggle_Button(logPrintBn->x() - (bn_w + 2), bn_y, bn_w, st_h);
  hideIdeBn = new Fl_Toggle_Button(lockBn->x() - (bn_w + 2), bn_y, bn_w, st_h);
  gotoLineBn = new Fl_Toggle_Button(hideIdeBn->x() - (bn_w + 2), bn_y, bn_w, st_h);

#ifdef __MINGW32__
  // fixup alignment under windows
  logPrintBn->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_CENTER);
  lockBn->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_CENTER);
  hideIdeBn->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_CENTER);
  gotoLineBn->align(FL_ALIGN_INSIDE | FL_ALIGN_LEFT | FL_ALIGN_CENTER);
#endif

  colStatus = new Fl_Button(gotoLineBn->x() - (st_w + 2), bn_y, st_w, st_h);
  rowStatus = new Fl_Button(colStatus->x() - (st_w + 2), bn_y, st_w, st_h);
  runStatus = new Fl_Button(rowStatus->x() - (st_w + 2), bn_y, st_w, st_h);
  modStatus = new Fl_Button(runStatus->x() - (st_w + 2), bn_y, st_w, st_h);

  commandChoice = new Fl_Button(rect->x(), bn_y, 80, st_h);
  commandText = new Fl_Input(commandChoice->x() + commandChoice->w() + 2, bn_y, modStatus->x() - commandChoice->w() - 4, st_h);
  commandText->align(FL_ALIGN_LEFT | FL_ALIGN_CLIP);
  commandText->when(FL_WHEN_ENTER_KEY_ALWAYS);
  commandText->labelfont(FL_HELVETICA);

  for (int n = 0; n < statusBar->children(); n++) {
    Fl_Widget *w = statusBar->child(n);
    w->labelfont(FL_HELVETICA);
    w->box(FL_BORDER_BOX);
  }

  statusBar->resizable(commandText);
  statusBar->end();

  resizable(tile);
  end();

  // command selection
  setCommand(cmd_find);
  runState(rs_ready);
  setModified(false);

  // button callbacks
  lockBn->callback(scroll_lock_cb);
  modStatus->callback(save_file_cb);
  runStatus->callback(MainWindow::run_cb);
  commandChoice->callback(command_cb, (void *)1);
  commandText->callback(command_cb, (void *)1);
  funcList->callback(func_list_cb, 0);
  logPrintBn->callback(un_select_cb, (void *)hideIdeBn);
  hideIdeBn->callback(un_select_cb, (void *)logPrintBn);
  colStatus->callback(goto_line_cb, 0);
  rowStatus->callback(goto_line_cb, 0);

  // setup icons
  logPrintBn->label("@i;@b;T"); // italic bold T
  lockBn->label("@||;");        // vertical bars
  hideIdeBn->label("@border_frame;");   // large dot
  gotoLineBn->label("@>;");     // right arrow (goto)

  // setup tooltips
  commandText->tooltip("Press Ctrl+f or Ctrl+Shift+f to find again");
  rowStatus->tooltip("Cursor row position");
  colStatus->tooltip("Cursor column position");
  runStatus->tooltip("Run or BREAK");
  modStatus->tooltip("Save file");
  logPrintBn->tooltip("Display PRINT statements in the log window");
  lockBn->tooltip("Prevent log window auto-scrolling");
  hideIdeBn->tooltip("Hide the editor while program is running");
  gotoLineBn->tooltip("Position the cursor to the last program line after BREAK");

  // setup defaults or restore settings
  if (wnd && wnd->_profile) {
    wnd->_profile->loadConfig(this);
  } else {
    setEditorColor(FL_WHITE, true);
  }
  take_focus();
}

EditorWidget::~EditorWidget() {
  delete editor;
}

//--Event handler methods-------------------------------------------------------

/**
 * change the selected text to upper/lower/camel case
 */
void EditorWidget::change_case(Fl_Widget *w, void *eventData) {
  Fl_Text_Buffer *tb = editor->_textbuf;
  int start, end;
  char *selection = getSelection(&start, &end);
  int len = strlen(selection);
  enum { up, down, mixed } curcase = isupper(selection[0]) ? up : down;

  for (int i = 1; i < len; i++) {
    if (isalpha(selection[i])) {
      bool isup = isupper(selection[i]);
      if ((curcase == up && isup == false) || (curcase == down && isup)) {
        curcase = mixed;
        break;
      }
    }
  }

  // transform pattern: Foo -> FOO, FOO -> foo, foo -> Foo
  for (int i = 0; i < len; i++) {
    selection[i] = curcase == mixed ? toupper(selection[i]) : tolower(selection[i]);
  }
  if (curcase == down) {
    selection[0] = toupper(selection[0]);
    // upcase chars following non-alpha chars
    for (int i = 1; i < len; i++) {
      if (isalpha(selection[i]) == false && i + 1 < len) {
        selection[i + 1] = toupper(selection[i + 1]);
      }
    }
  }

  if (selection[0]) {
    tb->replace_selection(selection);
    tb->select(start, end);
  }
  free((void *)selection);
}

/**
 * command handler
 */
void EditorWidget::command_opt(Fl_Widget *w, void *eventData) {
  setCommand((CommandOpt) (intptr_t) eventData);
}

/**
 * cut selected text to the clipboard
 */
void EditorWidget::cut_text(Fl_Widget *w, void *eventData) {
  Fl_Text_Editor::kf_cut(0, editor);
}

/**
 * delete selected text
 */
void EditorWidget::do_delete(Fl_Widget *w, void *eventData) {
  editor->_textbuf->remove_selection();
}

/**
 * perform keyword completion
 */
void EditorWidget::expand_word(Fl_Widget *w, void *eventData) {
  int start, end;
  const char *fullWord = 0;
  unsigned fullWordLen = 0;

  Fl_Text_Buffer *textbuf = editor->_textbuf;
  const char *text = textbuf->text();

  if (textbuf->selected()) {
    // get word before selection
    int pos1, pos2;
    textbuf->selection_position(&pos1, &pos2);
    start = textbuf->word_start(pos1 - 1);
    end = pos1;
    // get word from before selection to end of selection
    fullWord = text + start;
    fullWordLen = pos2 - start - 1;
  } else {
    // nothing selected - get word to left of cursor position
    int pos = editor->insert_position();
    end = textbuf->word_end(pos);
    start = textbuf->word_start(end - 1);
    completionIndex = 0;
  }

  if (start >= end) {
    return;
  }

  const char *expandWord = text + start;
  unsigned expandWordLen = end - start;
  int wordPos = 0;

  // scan for expandWord from within the current text buffer
  if (completionIndex != -1 && searchBackward(text, start - 1,
                                              expandWord, expandWordLen,
                                              &wordPos)) {
    int matchPos = -1;
    if (textbuf->selected() == 0) {
      matchPos = wordPos;
      completionIndex = 1;      // find next word on next call
    } else {
      // find the next word prior to the currently selected word
      int index = 1;
      while (wordPos > 0) {
        if (strncasecmp(text + wordPos, fullWord, fullWordLen) != 0 ||
            isalpha(text[wordPos + fullWordLen + 1])) {
          // isalpha - matches fullWord but word has more chars
          matchPos = wordPos;
          if (completionIndex == index) {
            completionIndex++;
            break;
          }
          // count index for non-matching fullWords only
          index++;
        }

        if (searchBackward(text, wordPos - 1, expandWord,
                           expandWordLen, &wordPos) == 0) {
          matchPos = -1;
          break;                // no more partial matches
        }
      }
      if (index == completionIndex) {
        // end of expansion sequence
        matchPos = -1;
      }
    }
    if (matchPos != -1) {
      char *word = textbuf->text_range(matchPos, textbuf->word_end(matchPos));
      if (textbuf->selected()) {
        textbuf->replace_selection(word + expandWordLen);
      } else {
        textbuf->insert(end, word + expandWordLen);
      }
      textbuf->select(end, end + strlen(word + expandWordLen));
      editor->insert_position(end + strlen(word + expandWordLen));
      free((void *)word);
      return;
    }
  }

  completionIndex = -1;         // no more buffer expansions

  strlib::List<String *> keywords;
  editor->getKeywords(keywords);

  // find the next replacement
  int firstIndex = -1;
  int lastIndex = -1;
  int curIndex = -1;
  int numWords = keywords.size();
  for (int i = 0; i < numWords; i++) {
    const char *keyword = ((String *)keywords.get(i))->c_str();
    if (strncasecmp(expandWord, keyword, expandWordLen) == 0) {
      if (firstIndex == -1) {
        firstIndex = i;
      }
      if (fullWordLen == 0) {
        if (expandWordLen == strlen(keyword)) {
          // nothing selected and word to left of cursor matches
          curIndex = i;
        }
      } else if (strncasecmp(fullWord, keyword, fullWordLen) == 0) {
        // selection+word to left of selection matches
        curIndex = i;
      }
      lastIndex = i;
    } else if (lastIndex != -1) {
      // moved beyond matching words
      break;
    }
  }

  if (lastIndex != -1) {
    if (lastIndex == curIndex || curIndex == -1) {
      lastIndex = firstIndex;   // wrap to first in subset
    } else {
      lastIndex = curIndex + 1;
    }

    const char *keyword = ((String *)keywords.get(lastIndex))->c_str();
    // updated the segment of the replacement text
    // that completes the current selection
    if (textbuf->selected()) {
      textbuf->replace_selection(keyword + expandWordLen);
    } else {
      textbuf->insert(end, keyword + expandWordLen);
    }
    textbuf->select(end, end + strlen(keyword + expandWordLen));
  }
}

/**
 * handler for find text command
 */
void EditorWidget::find(Fl_Widget *w, void *eventData) {
  setCommand(cmd_find);
}

/**
 * performs the current command
 */
void EditorWidget::command(Fl_Widget *w, void *eventData) {
  bool found = false;
  bool forward = (intptr_t) eventData;
  bool updatePos = (commandOpt != cmd_find_inc);

  if (Fl::event_button() == FL_RIGHT_MOUSE) {
    // right click
    forward = 0;
  }

  switch (commandOpt) {
  case cmd_find_inc:
  case cmd_find:
    found = editor->findText(commandText->value(), forward, updatePos);
    commandText->textcolor(found ? commandChoice->color() : FL_RED);
    commandText->redraw();
    break;

  case cmd_replace:
    commandBuffer.empty();
    commandBuffer.append(commandText->value());
    setCommand(cmd_replace_with);
    break;

  case cmd_replace_with:
    replace_next();
    break;

  case cmd_goto:
    gotoLine(atoi(commandText->value()));
    take_focus();
    break;

  case cmd_input_text:
    wnd->setModal(false);
    break;
  }
}

/**
 * font menu selection handler
 */
void EditorWidget::font_name(Fl_Widget *w, void *eventData) {
  // TODO: fixme
  //setFont(Fl_Font(w->label(), 0));
  wnd->updateConfig(this);
}

/**
 * sub/func selection list handler
 */
void EditorWidget::func_list(Fl_Widget *w, void *eventData) {
  /*
    TODO: fixme
  if (funcList && funcList->item()) {
    const char *label = funcList->item()->label();
    if (label) {
      if (strcmp(label, scanLabel) == 0) {
        funcList->clear();
        createFuncList();
        funcList->add(scanLabel);
      } else {
        gotoLine(funcList->item()->argument());
        take_focus();
      }
    }
  }
  */
}

/**
 * goto-line command handler
 */
void EditorWidget::goto_line(Fl_Widget *w, void *eventData) {
  setCommand(cmd_goto);
}

/**
 * paste clipboard text onto the buffer
 */
void EditorWidget::paste_text(Fl_Widget *w, void *eventData) {
  Fl_Text_Editor::kf_paste(0, editor);
}

/**
 * rename the currently selected variable
 */
void EditorWidget::rename_word(Fl_Widget *w, void *eventData) {
  if (rename_active) {
    rename_active = false;
  } else {
    Fl_Rect rc;
    char *selection = getSelection(&rc);
    if (selection) {
      showFindText(selection);
      begin();
      LineInput *in = new LineInput(rc.x(), rc.y(), rc.w() + 10, rc.h());
      end();
      // TODO: fixme
      //in->text(selection);
      in->callback(rename_word_cb);
      in->textfont(FL_COURIER);
      in->textsize(getFontSize());

      rename_active = true;
      // TODO: fixme
      //while (rename_active && in->focused()) {
      // Fl::wait();
      //}

      showFindText("");
      replaceAll(selection, in->value(), true, true);
      remove(in);
      take_focus();
      delete in;
      free((void *)selection);
    }
  }
}

/**
 * replace the next find occurance
 */
void EditorWidget::replace_next(Fl_Widget *w, void *eventData) {
  if (readonly()) {
    return;
  }

  const char *find = commandBuffer;
  const char *replace = commandText->value();

  Fl_Text_Buffer *textbuf = editor->_textbuf;
  int pos = editor->insert_position();
  int found = textbuf->search_forward(pos, find, &pos);

  if (found) {
    // found a match; update the position and replace text
    textbuf->select(pos, pos + strlen(find));
    textbuf->remove_selection();
    textbuf->insert(pos, replace);
    textbuf->select(pos, pos + strlen(replace));
    editor->insert_position(pos + strlen(replace));
    editor->show_insert_position();
  } else {
    setCommand(cmd_find);
    editor->take_focus();
  }
}

/**
 * save file menu command handler
 */
void EditorWidget::save_file(Fl_Widget *w, void *eventData) {
  if (filename[0] == '\0') {
    // no filename - get one!
    wnd->save_file_as();
    return;
  } else {
    doSaveFile(filename);
  }
}

/**
 * prevent the tty window from scrolling with new data
 */
void EditorWidget::scroll_lock(Fl_Widget *w, void *eventData) {
  // TODO: fixme
  //tty->setScrollLock(w->flags() & STATE);
}

/**
 * select all text
 */
void EditorWidget::select_all(Fl_Widget *w, void *eventData) {
  Fl_Text_Editor::kf_select_all(0, editor);
}

/**
 * set colour menu command handler
 */
void EditorWidget::set_color(Fl_Widget *w, void *eventData) {
  StyleField styleField = (StyleField) (intptr_t) eventData;
  if (styleField == st_background || styleField == st_background_def) {
    uint8_t r, g, b;
    // TODO: fixme
    split_color(editor->color(), r, g, b);
    if (fl_color_chooser(w->label(), r, g, b)) {
      Fl_Color c = fl_rgb_color(r, g, b);
      //set_color_index(FL_FREE_COLOR + styleField, c);
      setEditorColor(c, styleField == st_background_def);
      editor->styleChanged();
    }
    editor->take_focus();
  } else {
    setColor(w->label(), styleField);
  }
  wnd->updateConfig(this);
  wnd->show();
}

/**
 * replace text menu command handler
 */
void EditorWidget::show_replace(Fl_Widget *w, void *eventData) {
  const char *prime = editor->_search;
  if (!prime || !prime[0]) {
    // use selected text when search not available
    prime = editor->_textbuf->selection_text();
  }
  commandText->value(prime);
  setCommand(cmd_replace);
}

/**
 * undo any edit changes
 */
void EditorWidget::undo(Fl_Widget *w, void *eventData) {
  Fl_Text_Editor::kf_undo(0, editor);
}

/**
 * de-select the button specified in the eventData
 */
void EditorWidget::un_select(Fl_Widget *w, void *eventData) {
  ((Fl_Button *)eventData)->value(false);
}

//--Public methods--------------------------------------------------------------

/**
 * handles saving the current buffer
 */
bool EditorWidget::checkSave(bool discard) {
  if (!dirty) {
    return true;                // continue next operation
  }

  const char *msg = "The current file has not been saved.\n" "Would you like to save it now?";
  /*
    TODO: fixme
  int r = discard ?
          fl_choice(msg, "Save", "Discard", "Cancel") :
          fl_choice(msg, "Save", "Cancel", 0);
  if (discard ? (r == 2) : (r == 1)) {
    save_file();                // Save the file
    return !dirty;
  }
  return (discard && r == 1);
  */
  return false;
}

/**
 * copy selection text to the clipboard
 */
void EditorWidget::copyText() {
  if (!tty->copySelection()) {
    Fl_Text_Editor::kf_copy(0, editor);
  }
}

/**
 * saves the editor buffer to the given file name
 */
void EditorWidget::doSaveFile(const char *newfile) {
  if (!dirty && strcmp(newfile, filename) == 0) {
    // neither buffer or filename have changed
    return;
  }

  char basfile[PATH_MAX];
  Fl_Text_Buffer *textbuf = editor->_textbuf;

  if (wnd->_profile->_createBackups && access(newfile, 0) == 0) {
    // rename any existing file as a backup
    strcpy(basfile, newfile);
    strcat(basfile, "~");
    rename(newfile, basfile);
  }

  strcpy(basfile, newfile);
  if (strchr(basfile, '.') == 0) {
    strcat(basfile, ".bas");
  }

  if (textbuf->savefile(basfile)) {
    fl_alert("Error writing to file \'%s\':\n%s.", basfile, strerror(errno));
    return;
  }

  dirty = 0;
  strcpy(filename, basfile);
  modifiedTime = getModifiedTime();

  wnd->updateEditTabName(this);
  wnd->showEditTab(this);

  // store a copy in lastedit.bas
  if (wnd->_profile->_createBackups) {
    getHomeDir(basfile, sizeof(basfile));
    strlcat(basfile, "lastedit.bas", sizeof(basfile));
    textbuf->savefile(basfile);
  }

  textbuf->call_modify_callbacks();
  showPath();
  fileChanged(true);
  addHistory(filename);
  editor->take_focus();
}

/**
 * called when the buffer has changed
 */
void EditorWidget::fileChanged(bool loadfile) {
  funcList->clear();
  if (loadfile) {
    // update the func/sub navigator
    createFuncList();
    funcList->redraw();

    const char *filename = getFilename();
    if (filename && filename[0]) {
      // update the last used file menu
      bool found = false;

      for (int i = 0; i < NUM_RECENT_ITEMS; i++) {
        if (recentPath[i].c_str() !=  NULL &&
            strcmp(filename, recentPath[i].c_str()) == 0) {
          found = true;
          break;
        }
      }

      if (found == false) {
        // shift items downwards
        for (int i = NUM_RECENT_ITEMS - 1; i > 0; i--) {
          //_menuBar->replace(recentMenu[i], _menuBar->text(recentMenu[i]));
          recentPath[i].empty();
          recentPath[i].append(recentPath[i - 1]);
        }
        // create new item in first position
        const char *label = FileWidget::splitPath(filename, NULL);
        recentPath[0].empty();
        recentPath[0].append(filename);
        _menuBar->replace(recentMenu[0], label);
      }
    }
  }

  funcList->add(scanLabel);
}

/**
 * keyboard shortcut handler
 */
bool EditorWidget::focusWidget() {
  switch (Fl::event_key()) {
  case 'b':
    setBreakToLine(!isBreakToLine());
    return true;

  case 'e':
    setCommand(cmd_find);
    take_focus();
    return true;

  case 'f':
    if (strlen(commandText->value()) > 0 && commandOpt == cmd_find) {
      // continue search - shift -> backward else forward
      command(0, (void *)(intptr_t)(Fl::event_state(FL_SHIFT) ? 0 : 1));
    }
    setCommand(cmd_find);
    return true;

  case 'i':
    setCommand(cmd_find_inc);
    return true;

  case 't':
    setLogPrint(!isLogPrint());
    return true;

  case 'w':
    setHideIde(!isHideIDE());
    return true;
  }
  return false;
}

/**
 * returns the current font size
 */
int EditorWidget::getFontSize() {
  return editor->getFontSize();
}

/**
 * use the input control as the INPUT basic command handler
 */
void EditorWidget::getInput(char *result, int size) {
  if (result && result[0]) {
    commandText->value(result);
  }
  setCommand(cmd_input_text);
  wnd->setModal(true);
  while (wnd->isModal()) {
    Fl::wait();
  }
  if (wnd->isBreakExec()) {
    brun_break();
  } else {
    const char *value = commandText->value();
    int valueLen = strlen(value);
    int len = (valueLen < size) ? valueLen : size;
    strncpy(result, value, len);
    result[len] = 0;
  }
  setCommand(cmd_find);
}

/**
 * returns the row and col position for the current cursor position
 */
void EditorWidget::getRowCol(int *row, int *col) {
  return ((BasicEditor *)editor)->getRowCol(row, col);
}

/**
 * returns the selected text or the word around the cursor if there
 * is no current selection. caller must free the returned value
 */
char *EditorWidget::getSelection(int *start, int *end) {
  char *result = 0;

  Fl_Text_Buffer *tb = editor->_textbuf;
  if (tb->selected()) {
    result = tb->selection_text();
    tb->selection_position(start, end);
  } else {
    int pos = editor->insert_position();
    *start = tb->word_start(pos);
    *end = tb->word_end(pos);
    result = tb->text_range(*start, *end);
  }

  return result;
}

/**
 * returns where text selection ends
 */
void EditorWidget::getSelEndRowCol(int *row, int *col) {
  return ((BasicEditor *)editor)->getSelEndRowCol(row, col);
}

/**
 * returns where text selection starts
 */
void EditorWidget::getSelStartRowCol(int *row, int *col) {
  return ((BasicEditor *)editor)->getSelStartRowCol(row, col);
}

/**
 * sets the cursor to the given line number
 */
void EditorWidget::gotoLine(int line) {
  ((BasicEditor *)editor)->gotoLine(line);
}

/**
 * FLTK event handler
 */
int EditorWidget::handle(int e) {
  switch (e) {
  case FL_SHOW:
  case FL_FOCUS:
    Fl::focus(editor);
    handleFileChange();
    return 1;
  case FL_KEYBOARD:
    if (Fl::event_key() == FL_Escape) {
      take_focus();
      return 1;
    }
    break;
  case FL_ENTER:
    if (rename_active) {
      // prevent drawing over the inplace editor child control
      return 0;
    }
  }

  return Fl_Group::handle(e);
}

/**
 * load the given filename into the buffer
 */
void EditorWidget::loadFile(const char *newfile) {
  // save the current filename
  char oldpath[PATH_MAX];
  strcpy(oldpath, filename);

  // convert relative path to full path
  getcwd(filename, sizeof(filename));
  strcat(filename, "/");
  strcat(filename, newfile);

  if (access(filename, R_OK) != 0) {
    // filename unreadable, try newfile
    strcpy(filename, newfile);
  }

  FileWidget::forwardSlash(filename);

  loading = true;
  if (editor->_textbuf->loadfile(filename)) {
    // read failed
    fl_alert("Error reading from file \'%s\':\n%s.", filename, strerror(errno));

    // restore previous file
    strcpy(filename, oldpath);
    editor->_textbuf->loadfile(filename);
  }

  dirty = false;
  loading = false;

  editor->_textbuf->call_modify_callbacks();
  editor->show_insert_position();
  modifiedTime = getModifiedTime();
  readonly(false);

  wnd->updateEditTabName(this);
  wnd->showEditTab(this);

  showPath();
  fileChanged(true);
  setRowCol(1, 1);
}

/**
 * returns the buffer readonly flag
 */
bool EditorWidget::readonly() {
  return ((BasicEditor *)editor)->_readonly;
}

/**
 * sets the buffer readonly flag
 */
void EditorWidget::readonly(bool is_readonly) {
  if (!is_readonly && access(filename, W_OK) != 0) {
    // cannot set writable since file is readonly
    is_readonly = true;
  }
  modStatus->label(is_readonly ? "RO" : "@line");
  modStatus->redraw();
  editor->cursor_style(is_readonly ? Fl_Text_Display::DIM_CURSOR : Fl_Text_Display::NORMAL_CURSOR);
  ((BasicEditor *)editor)->_readonly = is_readonly;
}

/**
 * displays the current run-mode flag
 */
void EditorWidget::runState(RunMessage runMessage) {
  runStatus->callback(MainWindow::run_cb);
  const char *msg = 0;
  switch (runMessage) {
  case rs_err:
    msg = "ERR";
    break;
  case rs_run:
    msg = "BRK";
    runStatus->callback(MainWindow::run_break_cb);
    break;
  default:
    msg = "RUN";
  }
  runStatus->copy_label(msg);
  runStatus->redraw();
}

/**
 * Saves the selected text to the given file path
 */
void EditorWidget::saveSelection(const char *path) {
  FILE *fp = fopen(path, "w");
  if (fp) {
    Fl_Rect rc;
    char *selection = getSelection(&rc);
    if (selection) {
      fwrite(selection, strlen(selection), 1, fp);
      free((void *)selection);
    } else {
      // save as an empty file
      fputc(0, fp);
    }
    fclose(fp);
  }
}

/**
 * Sets the editor and editor toolbar color
 */
void EditorWidget::setEditorColor(Fl_Color c, bool defColor) {
  if (wnd && wnd->_profile) {
    wnd->_profile->_color = c;
  }
  editor->color(c);

  Fl_Color bg = FL_GRAY0;       // same offset as editor line numbers
  Fl_Color fg = fl_contrast(c, bg);
  int i;

  // set the colours on the command text bar
  for (i = commandText->parent()->children(); i > 0; i--) {
    Fl_Widget *child = commandText->parent()->child(i - 1);
    setWidgetColor(child, bg, fg);
  }

  // set the colours on the function list
  setWidgetColor(funcList, bg, fg);

  if (defColor) {
    // contrast the default colours against the background
    for (i = 0; i < st_background; i++) {
      styletable[i].color = fl_contrast(defaultColor[i], c);
    }
  }
}

/**
 * sets the current display font
 */
void EditorWidget::setFont(Fl_Font font) {
  if (font) {
    editor->setFont(font);
    tty->setFont(font);
    wnd->_profile->_font = font;
  }
}

/**
 * sets the current font size
 */
void EditorWidget::setFontSize(int size) {
  editor->setFontSize(size);
  tty->setFontSize(size);
  wnd->_profile->_fontSize = size;
}

/**
 * sets the indent level to the given amount
 */
void EditorWidget::setIndentLevel(int level) {
  ((BasicEditor *)editor)->_indentLevel = level;

  // update environment var for running programs
  char path[PATH_MAX];
  sprintf(path, "INDENT_LEVEL=%d", level);
  putenv(path);
}

/**
 * displays the row/col in the editor toolbar
 */
void EditorWidget::setRowCol(int row, int col) {
  char rowcol[20];
  sprintf(rowcol, "%d", row);
  rowStatus->copy_label(rowcol);
  rowStatus->redraw();
  sprintf(rowcol, "%d", col);
  colStatus->copy_label(rowcol);
  colStatus->redraw();

  // sync the browser widget selection
  int len = funcList->children() - 1;
  for (int i = 0; i < len; i++) {
    int line = (int)funcList->child(i)->argument();
    int nextLine = (int)funcList->child(i + 1)->argument();
    if (row >= line && (i == len - 1 || row < nextLine)) {
      funcList->value(i);
      break;
    }
  }
}

/**
 * display the full pathname
 */
void EditorWidget::showPath() {
  commandChoice->tooltip(filename);
}

/**
 * prints a status message on the tty-widget
 */
void EditorWidget::statusMsg(const char *msg) {
  if (msg) {
    tty->print(msg);
    tty->print("\n");
  }
}

/**
 * sets the font face, size and colour
 */
void EditorWidget::updateConfig(EditorWidget *current) {
  setFont(current->editor->getFont());
  setFontSize(current->editor->getFontSize());
  setEditorColor(current->editor->color(), false);
}

//--Protected methods-----------------------------------------------------------

/**
 * add filename to the hiistory file
 */
void EditorWidget::addHistory(const char *filename) {
  FILE *fp;
  char buffer[PATH_MAX];
  char updatedfile[PATH_MAX];
  char path[PATH_MAX];

  int len = strlen(filename);
  if (strcasecmp(filename + len - 4, ".sbx") == 0 || access(filename, R_OK) != 0) {
    // don't remember bas exe or invalid files
    return;
  }

  len -= strlen(untitledFile);
  if (len > 0 && strcmp(filename + len, untitledFile) == 0) {
    // don't remember the untitled file
    return;
  }
  // save paths with unix path separators
  strcpy(updatedfile, filename);
  FileWidget::forwardSlash(updatedfile);
  filename = updatedfile;

  // save into the history file
  getHomeDir(path, sizeof(path));
  strlcat(path, historyFile, sizeof(path));

  fp = fopen(path, "r");
  if (fp) {
    // don't add the item if it already exists
    while (feof(fp) == 0) {
      if (fgets(buffer, sizeof(buffer), fp) && strncmp(filename, buffer, strlen(filename) - 1) == 0) {
        fclose(fp);
        return;
      }
    }
    fclose(fp);
  }

  fp = fopen(path, "a");
  if (fp) {
    fwrite(filename, strlen(filename), 1, fp);
    fwrite("\n", 1, 1, fp);
    fclose(fp);
  }
}

/**
 * creates the sub/func selection list
 */
void EditorWidget::createFuncList() {
  Fl_Text_Buffer *textbuf = editor->_textbuf;
  const char *text = textbuf->text();
  int len = textbuf->length();
  int curLine = 1;
  const char *keywords[] = {
    "sub ", "func ", "def ", "label ", "const ", "local ", "dim "
  };
  int keywords_length = sizeof(keywords) / sizeof(keywords[0]);
  int keywords_len[keywords_length];
  for (int j = 0; j < keywords_length; j++) {
    keywords_len[j] = strlen(keywords[j]);
  }
  Fl_Group *menuGroup = NULL;

  for (int i = 0; i < len; i++) {
    // skip to the newline start
    while (i < len && i != 0 && text[i] != '\n') {
      i++;
    }

    // skip any successive newlines
    while (i < len && text[i] == '\n') {
      curLine++;
      i++;
    }

    // skip any leading whitespace
    while (i < len && (text[i] == ' ' || text[i] == '\t')) {
      i++;
    }

    for (int j = 0; j < keywords_length; j++) {
      if (!strncasecmp(text + i, keywords[j], keywords_len[j])) {
        i += keywords_len[j];
        int i_begin = i;
        while (i < len && text[i] != '=' && text[i] != '\r' && text[i] != '\n') {
          i++;
        }
        if (i > i_begin) {
          String s(text + i_begin, i - i_begin);
          if (j < 2) {
            // TODO: fixme
            //menuGroup = funcList->add_group(s.c_str(), 0, (void *)(intptr_t)curLine);
          } else {
            // funcList->add_leaf(s.c_str(), menuGroup, (void *)(intptr_t)curLine);
          }
        }
        break;
      }
    }
    if (text[i] == '\n') {
      i--;                      // avoid eating the entire next line
    }
  }
}

/**
 * called when the buffer has change - sets the modified flag
 */
void EditorWidget::doChange(int inserted, int deleted) {
  if (!loading) {
    // do nothing while file load in progress
    if (inserted || deleted) {
      dirty = 1;
    }

    if (!readonly()) {
      setModified(dirty);
    }
  }
}

/**
 * handler for the sub/func list selection event
 */
void EditorWidget::findFunc(const char *find) {
  const char *text = editor->_textbuf->text();
  int findLen = strlen(find);
  int len = editor->_textbuf->length();
  int lineNo = 1;
  for (int i = 0; i < len; i++) {
    if (strncasecmp(text + i, find, findLen) == 0) {
      gotoLine(lineNo);
      break;
    } else if (text[i] == '\n') {
      lineNo++;
    }
  }
}

/**
 * returns the current selection text
 */
char *EditorWidget::getSelection(Fl_Rect *rc) {
  return ((BasicEditor *)editor)->getSelection(rc);
}

/**
 * returns the current file modified time
 */
uint32_t EditorWidget::getModifiedTime() {
  struct stat st_file;
  uint32_t modified = 0;
  if (filename[0] && !stat(filename, &st_file)) {
    modified = st_file.st_mtime;
  }
  return modified;
}

/**
 * handler for the external file change event
 */
void EditorWidget::handleFileChange() {
  // handle outside changes to the file
  if (filename[0] && modifiedTime != 0 && modifiedTime != getModifiedTime()) {
    const char *msg = "File %s\nhas changed on disk.\n\n" "Do you want to reload the file?";
    /*
      TODO: fixme
    if (fl_choice(msg, filename, "Yes", "No")) {
      reloadFile();
    } else {
      modifiedTime = 0;
    }
    */
  }
}

/**
 * prevent the tty and browser from growing when the outer window is resized
 */
void EditorWidget::layout() {
  Fl_Group *tile = editor->parent();
  tile->resizable(editor);
  // TODO: fixme
  //Fl_Group::layout();

  // when set to editor the tile is not resizable using the mouse
  tile->resizable(NULL);
}

/**
 * create a new editor buffer
 */
void EditorWidget::newFile() {
  if (readonly()) {
    return;
  }

  if (!checkSave(true)) {
    return;
  }

  Fl_Text_Buffer *textbuf = editor->_textbuf;
  filename[0] = '\0';
  textbuf->select(0, textbuf->length());
  textbuf->remove_selection();
  dirty = 0;
  textbuf->call_modify_callbacks();
  fileChanged(false);
  modifiedTime = 0;
}

/**
 * reload the editor buffer
 */
void EditorWidget::reloadFile() {
  char buffer[PATH_MAX];
  strcpy(buffer, filename);
  loadFile(buffer);
}

/**
 * replace all occurances of the given text
 */
int EditorWidget::replaceAll(const char *find, const char *replace, bool restorePos, bool matchWord) {
  int times = 0;

  if (strcmp(find, replace) != 0) {
    Fl_Text_Buffer *textbuf = editor->_textbuf;
    int prevPos = editor->insert_position();

    // loop through the whole string
    int pos = 0;
    editor->insert_position(pos);

    while (textbuf->search_forward(pos, find, &pos)) {
      // found a match; update the position and replace text
      if (!matchWord ||
          ((pos == 0 || !isvar(textbuf->char_at(pos - 1))) &&
           !isvar(textbuf->char_at(pos + strlen(find))))) {
        textbuf->select(pos, pos + strlen(find));
        textbuf->remove_selection();
        textbuf->insert(pos, replace);
      }
      // advance beyond replace string
      pos += strlen(replace);
      editor->insert_position(pos);
      times++;
    }

    if (restorePos) {
      editor->insert_position(prevPos);
    }
    editor->show_insert_position();
  }

  return times;
}

/**
 * handler for searching backwards
 */
bool EditorWidget::searchBackward(const char *text, int startPos,
                                  const char *find, int findLen, int *foundPos) {
  int matchIndex = findLen - 1;
  for (int i = startPos; i >= 0; i--) {
    bool equals = toupper(text[i]) == toupper(find[matchIndex]);
    if (equals == false && matchIndex < findLen - 1) {
      // partial match now fails - reset search at current index
      matchIndex = findLen - 1;
      equals = toupper(text[i]) == toupper(find[matchIndex]);
    }
    matchIndex = (equals ? matchIndex - 1 : findLen - 1);
    if (matchIndex == -1 && (i == 0 || isalpha(text[i - 1]) == 0)) {
      // char prior to word is non-alpha
      *foundPos = i;
      return true;
    }
  }
  return false;
}

/**
 * sets the current display colour
 */
void EditorWidget::setColor(const char *label, StyleField field) {
  uint8_t r, g, b;
  // TODO: fixme
  split_color(styletable[field].color, r, g, b);
  if (fl_color_chooser(label, r, g, b)) {
    Fl_Color c = fl_rgb_color(r, g, b);
    //set_color_index(FL_FREE_COLOR + field, c);
    styletable[field].color = c;
    editor->styleChanged();
  }
}

/**
 * sets the current command
 */
void EditorWidget::setCommand(CommandOpt command) {
  if (commandOpt == cmd_input_text) {
    wnd->setModal(false);
  }

  commandOpt = command;
  switch (command) {
  case cmd_find:
    commandChoice->label("@search; Find:");
    break;
  case cmd_find_inc:
    commandChoice->label("Inc Find:");
    break;
  case cmd_replace:
    commandChoice->label("Replace:");
    break;
  case cmd_replace_with:
    commandChoice->label("With:");
    break;
  case cmd_goto:
    commandChoice->label("Goto:");
    break;
  case cmd_input_text:
    commandChoice->label("INPUT:");
    break;
  }
  commandChoice->redraw();
  //TODO: fixme
  //commandText->textcolor(commandChoice->textcolor());
  commandText->redraw();
  commandText->take_focus();
  commandText->when(commandOpt == cmd_find_inc ? FL_WHEN_CHANGED : FL_WHEN_ENTER_KEY_ALWAYS);
}

/**
 * display the toolbar modified flag
 */
void EditorWidget::setModified(bool dirty) {
  this->dirty = dirty;
  modStatus->when(dirty ? FL_WHEN_CHANGED : FL_WHEN_NEVER);
  modStatus->label(dirty ? "MOD" : "@line");
  modStatus->redraw();
}

/**
 * sets the foreground and background colors on the given widget
 */
void EditorWidget::setWidgetColor(Fl_Widget *w, Fl_Color bg, Fl_Color fg) {
  w->color(bg);
  // TODO: fixme
  //w->textcolor(fg);
  w->redraw();
}

/**
 * highlight the given search text
 */
void EditorWidget::showFindText(const char *text) {
  editor->showFindText(text);
}

