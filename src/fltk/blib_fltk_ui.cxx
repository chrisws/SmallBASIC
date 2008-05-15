// $Id$
//
// Copyright(C) 2001-2008 Chris Warren-Smith. [http://tinyurl.com/ja2ss]
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include "sys.h"
#include "var.h"
#include "kw.h"
#include "pproc.h"
#include "device.h"
#include "smbas.h"

#include <fltk/Browser.h>
#include <fltk/Button.h>
#include <fltk/CheckButton.h>
#include <fltk/Choice.h>
#include <fltk/Group.h>
#include <fltk/Input.h>
#include <fltk/Item.h>
#include <fltk/RadioButton.h>
#include <fltk/Rectangle.h>
#include <fltk/StringList.h>
#include <fltk/draw.h>
#include <fltk/events.h>
#include <fltk/run.h>

#include "MainWindow.h"
#include "StringLib.h"

extern "C" {
#include "blib_ui.h"
} 
extern MainWindow* wnd;

struct Form : public Group {
  Form(int x1, int x2, int y1, int y2) : Group(x1, x2, y1, y2) {} 
  void draw(); // avoid drawing over the tab-bar
};

Form* form = 0;

enum ControlType {
  ctrl_button,
  ctrl_radio,
  ctrl_check,
  ctrl_text,
  ctrl_label,
  ctrl_listbox,
  ctrl_dropdown
};

enum Mode { m_unset, m_init, m_modeless, m_modal, m_closed } mode = m_unset;
int modeless_x;
int modeless_y;
int modeless_w;
int modeless_h;

int prev_x;
int prev_y;

// width and height fudge factors for when button w+h specified as -1
#define BN_W  16
#define BN_H   2
#define RAD_W 22
#define RAD_H  0

Color formColor = color(172, 172, 172);

void closeModeless() {
  mode = m_closed;
  ui_reset();
}

struct WidgetInfo {
  ControlType type;
  var_t* var;
  bool is_group_radio;

  // startup value used to check if
  // exec has altered a bound variable
  union {
    long i;
    byte* ptr;
  } orig;

  void update_var_flag() {
    switch (var->type) {
    case V_STR:
      orig.ptr = var->v.p.ptr;
      break;
    case V_ARRAY:
      orig.ptr = var->v.a.ptr;
      break;
    case V_INT:
      orig.i = var->v.i;
      break;
    default:
      orig.i = 0;
    }
  }
};

// implements abstract StringList as a list of strings
struct DropListModel : StringList {
  strlib::List list;
  int focus_index;

  DropListModel(const char* items, var_t* v) : StringList() {
    focus_index = -1;
    
    if (v && v->type == V_ARRAY) {
      fromArray(items, v);
      return;
    }

    // construct from a string like "Easy|Medium|Hard" 
    int item_index = 0;
    int len = items ? strlen(items) : 0;
    for (int i = 0; i < len; i++) {
      char* c = strchr(items + i, '|');
      int end_index = c ? c - items : len;
      String* s = new String(items + i, end_index - i);
      list.add(s);
      i = end_index;
      if (v != 0 && v->type == V_STR && v->v.p.ptr &&
          strcasecmp((const char*)v->v.p.ptr, s->toString()) == 0) {
        focus_index = item_index;
      }
      item_index++;
    }
  }
  
  // construct from an array of values
  void fromArray(const char* caption, var_t* v) {
    for (int i = 0; i < v->v.a.size; i++) {
      var_t* el_p = (var_t*) (v->v.a.ptr + sizeof(var_t)* i);
      if (el_p->type == V_STR) {
        list.add(new String((const char*)el_p->v.p.ptr));
        if (strcasecmp((const char*)el_p->v.p.ptr, caption) == 0) {
          focus_index = i;
        }
      }
      else if (el_p->type == V_INT) {
        char buff[40];
        sprintf(buff, "%ld", el_p->v.i);
        list.add(buff);
      }
      else if (el_p->type == V_ARRAY) {
        fromArray(caption, el_p);
      }
    }
  }

  // return the number of elements
  int children(const Menu*) {
    return list.length();
  }
  
  // return the label at the given index
  const char* label(const Menu*, int index) {
    return getElementAt(index)->c_str();
  }
  
  String* getElementAt(int index) {
    return (String*) list.get(index);
  }

  // returns the index corresponding to the given string
  int getPosition(const char* t) {
    int size = list.length();
    for (int i = 0; i < size; i++) {
      if (!strcasecmp(((String*) list.get(i))->c_str(), t)) {
        return i;
      }
    }
    return -1;
  }
};

void Form::draw()
{
  int numchildren = children();
  Rectangle r(w(), h());
  if (box() == NO_BOX) {
    setcolor(color());
    fillrect(r);
  }
  else {
    draw_box();
    box()->inset(r);
  }
  push_clip(r);
  for (int n = 0; n < numchildren; n++) {
    Widget & w = *child(n);
    draw_child(w);
    draw_outside_label(w);
  }
  pop_clip();
}

// convert a basic array into a std::string
void array_to_string(String& s, var_t* v)
{
  for (int i = 0; i < v->v.a.size; i++) {
    var_t*el_p = (var_t*) (v->v.a.ptr + sizeof(var_t)* i);
    if (el_p->type == V_STR) {
      s.append((const char*)el_p->v.p.ptr);
      s.append("\n");
    }
    else if (el_p->type == V_INT) {
      char buff[40];
      sprintf(buff, "%ld\n", el_p->v.i);
      s.append(buff);
    }
    else if (el_p->type == V_ARRAY) {
      array_to_string(s, el_p);
    }
  }
}

// set basic string variable to widget state
bool update_gui(Widget* w, WidgetInfo* inf)
{
  Choice* dropdown;
  Browser* listbox;
  DropListModel* model;

  if (inf->var->type == V_INT && inf->var->v.i != inf->orig.i) {
    // update list control with new int variable
    switch (inf->type) {
    case ctrl_dropdown:
      ((Choice*) w)->value(inf->var->v.i);
      return true;

    case ctrl_listbox:
      ((Browser*) w)->select(inf->var->v.i);
      return true;

    default:
      return false;
    }
  }

  if (inf->var->type == V_ARRAY && inf->var->v.p.ptr != inf->orig.ptr) {
    // update list control with new array variable
    String s;

    switch (inf->type) {
    case ctrl_dropdown:
      delete((Choice*) w)->list();
      ((Choice*) w)->list(new DropListModel(0, inf->var));
      return true;

    case ctrl_listbox:
      delete((Browser*) w)->list();
      ((Browser*) w)->list(new DropListModel(0, inf->var));
      return true;

    case ctrl_label:
      array_to_string(s, inf->var);
      w->copy_label(s.c_str());
      break;

    case ctrl_text:
      array_to_string(s, inf->var);
      ((Input*) w)->copy_label(s.c_str());
      break;

    default:
      return false;
    }
  }

  if (inf->var->type == V_STR && inf->orig.ptr != inf->var->v.p.ptr) {
    // update list control with new string variable
    switch (inf->type) {
    case ctrl_dropdown:
      dropdown = (Choice*) w;
      model = (DropListModel*) dropdown->list();
      if (strchr((const char*)inf->var->v.p.ptr, '|')) {
        // create a new list of items
        delete model;
        model = new DropListModel((const char*)inf->var->v.p.ptr, 0);
        dropdown->list(model);
      }
      else {
        // select one of the existing list items
        int selection = model->getPosition((const char*)inf->var->v.p.ptr);
        if (selection != -1) {
          dropdown->value(selection);
        }
      }
      break;

    case ctrl_listbox:
      listbox = (Browser*) w;
      model = (DropListModel*) listbox->list();
      if (strchr((const char*)inf->var->v.p.ptr, '|')) {
        // create a new list of items
        delete model;
        model = new DropListModel((const char*)inf->var->v.p.ptr, 0);
        listbox->list(model);
      }
      else {
        int selection = model->getPosition((const char*)inf->var->v.p.ptr);
        if (selection != -1) {
          listbox->value(selection);
        }
      }
      break;

    case ctrl_check:
    case ctrl_radio:
      ((CheckButton*) w)->
        value(!strcasecmp((const char*)inf->var->v.p.ptr, w->label()));
      break;

    case ctrl_label:
      w->copy_label((const char*)inf->var->v.p.ptr);
      break;

    case ctrl_text:
      ((Input*)w)->text((const char*)inf->var->v.p.ptr);
      break;

    default:
      break;
    }
    return true;
  }
  return false;
}

// synchronise basic variable and widget state
void transfer_data(Widget* w, WidgetInfo* inf)
{
  Choice* dropdown;
  Browser* listbox;
  DropListModel* model;

  if (update_gui(w, inf)) {
    inf->update_var_flag();
    return;
  }

  // set widget state to basic variable
  switch (inf->type) {
  case ctrl_check:
    if (((CheckButton*) w)->value()) {
      v_setstr(inf->var, w->label());
    }
    else {
      v_zerostr(inf->var);
    }
    break;

  case ctrl_radio:
    if (((RadioButton*) w)->value() && w->label() != 0) {
      v_setstr(inf->var, w->label());
    }
    else if (!inf->is_group_radio) {
      // reset radio variable for radio that is not part of a group
      v_zerostr(inf->var);
    }
    break;

  case ctrl_text:
    if (((Input*)w)->text()) {
      v_setstr(inf->var, ((Input*)w)->text());
    }
    else {
      v_zerostr(inf->var);
    }
    break;

  case ctrl_dropdown:
    dropdown = (Choice*) w;
    model = (DropListModel*) dropdown->list();
    if (dropdown->value() != -1) {
      String* s = model->getElementAt(dropdown->value());
      v_setstr(inf->var, s->c_str());
    }
    break;

  case ctrl_listbox:
    listbox = (Browser*) w;
    model = (DropListModel*) listbox->list();
    if (listbox->value() != -1) {
      String* s = model->getElementAt(listbox->value());
      v_setstr(inf->var, s->c_str());
    }
    break;

  default:
    break;
  }

  // only update the gui when the variable is changed in basic code
  inf->update_var_flag();
}

// find the radio group of the given variable from within the parent
Group* findRadioGroup(Group* parent, var_t* v) {
  Group* radioGroup = 0;
  int n = parent->children();
  for (int i = 0; i < n && !radioGroup; i++) {
    Widget* w = parent->child(i);
    if (!w->user_data()) {
      radioGroup = findRadioGroup((Group*) w, v);
    }
    else {
      WidgetInfo* inf = (WidgetInfo*) w->user_data();
      if (inf->type == ctrl_radio &&
          inf->var->type == V_STR &&
          (inf->var == v || inf->var->v.p.ptr == v->v.p.ptr)) {
        // another ctrl_radio is linked to the same variable
        inf->is_group_radio = true;
        radioGroup = parent;
      }
    }
  }
  return radioGroup;
}

// radio control's belong to the same group when they share
// a common basic variable
void update_radio_group(WidgetInfo* radioInf, RadioButton* radio)
{
  var_t* v = radioInf->var;

  if (v == 0 || v->type != V_STR) {
    return;
  }

  Group* radioGroup = findRadioGroup(form, v);

  if (!radioGroup) {
    radioGroup = new Group(0, 0, form->w(), form->h());
    form->add(radioGroup);
  }
  else {
    radioInf->is_group_radio = true;
  }

  radioGroup->add(radio);
}

void widget_cb(Widget* w, void* v)
{
  WidgetInfo* inf = (WidgetInfo*) v;
  transfer_data(w, (WidgetInfo*) v);

  if (inf->type == ctrl_button || mode == m_modeless) {
    // any button type = end modal loop -or- exit modeless
    // loop in cmd_doform() and continue basic statements
    // cmd_doform() can later be re-entered to continue form
    mode = m_closed;
  }

  if (inf->type == ctrl_button) {
    // update the basic variable with the button pressed
    v_setstr(inf->var, w->label());
  }
}

void update_widget(Widget* widget, WidgetInfo* inf, Rectangle& rect)
{
  if (rect.w() != -1) {
    widget->w(rect.w());
  }

  if (rect.h() != -1) {
    widget->h(rect.h());
  }

  if (rect.x() < 0) {
    rect.x(prev_x - rect.x());
  }

  if (rect.y() < 0) {
    rect.y(prev_y - rect.y());
  }

  prev_x = rect.x() + rect.w();
  prev_y = rect.y() + rect.h();

  widget->x(rect.x());
  widget->y(rect.y());
  widget->callback(widget_cb);
  widget->user_data(inf);

  // allow form init to update widget from basic variable
  inf->orig.ptr = 0;
  inf->orig.i = 0;
}

void update_button(Widget* widget, WidgetInfo* inf,
                   const char* caption, Rectangle& rect, int def_w, int def_h)
{
  if (rect.w() == -1 && caption != 0) {
    rect.w(getwidth(caption) + def_w);
  }

  if (rect.h() == -1) {
    rect.h(getascent() + getdescent() + def_h);
  }

  update_widget(widget, inf, rect);
  widget->copy_label(caption);
}

// copy all widget fields into variables
void update_form(Group* group)
{
  if (group) {
    int n = group->children();
    for (int i = 0; i < n; i++) {
      Widget* w = group->child(i);
      if (!w->user_data()) {
        update_form((Group*) w);
      }
      else {
        transfer_data(w, (WidgetInfo*) w->user_data());
      }
    }
  }
}

void form_begin()
{
  if (form == 0) {
    wnd->outputGroup->begin();
    form = new Form(wnd->out->x() + 2,
                    wnd->out->y() + 2, 
                    wnd->out->w() - 2, 
                    wnd->out->h() - 2);
    form->resizable(0);
    form->color(formColor);
    form->box(BORDER_BOX);
    wnd->outputGroup->end();
  }
  form->begin();
}

C_LINKAGE_BEGIN void ui_reset()
{
  if (form != 0) {
    form->hide();
    int n = form->children();
    for (int i = 0; i < n; i++) {
      WidgetInfo* inf = (WidgetInfo*) form->child(i)->user_data();
      delete inf;
    }
    form->clear();
    wnd->outputGroup->remove(form);
    form->parent(0);
    delete form;
    form = 0;

    wnd->out->show();
    wnd->out->take_focus();
    wnd->out->redraw();
  }
  mode = m_unset;
}

// BUTTON x, y, w, h, variable, caption [,type]
//
// type can optionally be 'radio' | 'checkbox' | 'link' | 'choice'
// variable is set to 1 if a button or link was pressed (which
// will have closed the form, or if a radio or checkbox was
// selected when the form was closed
//
void cmd_button()
{
  int x, y, w, h;
  var_t* v = 0;
  char* caption = 0;
  char* type = 0;

  form_begin();
  if (-1 != par_massget("IIIIPSs", &x, &y, &w, &h, &v, &caption, &type)) {
    WidgetInfo* inf = new WidgetInfo();
    inf->var = v;
    Rectangle rect(x, y, w, h);

    if (prog_error) {
      return;
    }
    if (type) {
      if (strcasecmp("radio", type) == 0) {
        inf->type = ctrl_radio;
        inf->is_group_radio = false;
        form->end(); // add widget to RadioGroup
        RadioButton* widget = new RadioButton(x, y, w, h);
        update_radio_group(inf, widget);
        update_button(widget, inf, caption, rect, RAD_W, RAD_H);
      }
      else if (strcasecmp("checkbox", type) == 0) {
        inf->type = ctrl_check;
        CheckButton* widget = new CheckButton(x, y, w, h);
        update_button(widget, inf, caption, rect, RAD_W, RAD_H);
      }
      else if (strcasecmp("button", type) == 0) {
        inf->type = ctrl_button;
        Button* widget = new Button(x, y, w, h);
        update_button(widget, inf, caption, rect, BN_W, BN_H);
      }
      else if (strcasecmp("label", type) == 0) {
        inf->type = ctrl_label;
        Widget* widget = new Widget(x, y, w, h);
        widget->box(FLAT_BOX);
        widget->color(formColor);
        widget->align(ALIGN_LEFT | ALIGN_INSIDE);
        update_button(widget, inf, caption, rect, BN_W, BN_H);
      }
      else if (strcasecmp("listbox", type) == 0) {
        inf->type = ctrl_listbox;
        Browser* widget = new Browser(x, y, w, h);
        DropListModel* model = new DropListModel(caption, v);
        widget->list(model);
        if (model->focus_index != -1) {
          widget->value(model->focus_index);
        }
        update_widget(widget, inf, rect);
      }
      else if (strcasecmp("dropdown", type) == 0 || strcasecmp("choice", type) == 0) {
        inf->type = ctrl_dropdown;
        Choice* widget = new Choice(x, y, w, h);
        DropListModel* model = new DropListModel(caption, v);
        widget->list(model);
        if (model->focus_index != -1) {
          widget->value(model->focus_index);
        }
        update_widget(widget, inf, rect);
      }
      else {
        ui_reset();
        rt_raise("UI: UNKNOWN TYPE: %s", type);
      }
    }
    else {
      inf->type = ctrl_button;
      Button* widget = new Button(x, y, w, h);
      update_button(widget, inf, caption, rect, BN_W, BN_H);
    }
  }

  form->end();
  pfree2(caption, type);
}

// TEXT x, y, w, h, variable
// When DOFORM returns the variable contains the user entered value
void cmd_text()
{
  int x, y, w, h;
  var_t* v = 0;

  if (-1 != par_massget("IIIIP", &x, &y, &w, &h, &v)) {
    form_begin();
    Input* widget = new Input(x, y, w, h);
    widget->box(BORDER_BOX);
    widget->color(color(220, 220, 220));
    Rectangle rect(x, y, w, h);
    WidgetInfo* inf = new WidgetInfo();
    inf->var = v;
    inf->type = ctrl_text;
    update_widget(widget, inf, rect);
    form->end();
  }
}

// DOFORM [x, y, w, h [,border-style, bg-color]]
// Executes the form
void cmd_doform()
{
  int x, y, w, h;
  int num_args;

  x = y = w = h = 0;
  num_args = par_massget("iiii", &x, &y, &w, &h);

  if (form == 0) {
    // begin modeless state - m_unset, m_init, m_modeless
    mode = m_init;
    modeless_x = x;
    modeless_y = y;
    modeless_w = w;
    modeless_h = h;
    return;
  }

  if (mode != m_unset) {
    // continue modeless state
    if (form == 0) {
      rt_raise("UI: FORM HAS CLOSED");
      return;
    }

    // set form position in initial iteration
    if (mode == m_init) {
      mode = m_modeless;
      if (num_args == 0) {
        // apply coordinates from inital doform call
        x = modeless_x;
        y = modeless_y;
        w = modeless_w;
        h = modeless_h;
      }
    }
    else {
      // pump system messages until button is clicked
      update_form(form);
      while (mode == m_modeless) {
        fltk::wait();
      }
      mode = m_modeless;
      update_form(form);
      return;
    }
  }

  switch (num_args) {
  case 0:
    x = y = 2;
  case 2:
  case 4:
    break;
  default:
    ui_reset();
    rt_raise("UI: INVALID FORM ARGUMENTS: %d", num_args);
    return;
  }

  if (w < 1 || x + w > form->w()) {
    w = form->w() + 3;
  }
  if (h < 1 || y + h > form->h()) {
    h = form->h() + 3;
  }

  wnd->tabGroup->selected_child(wnd->outputGroup);
  form->resize(x, y, w, h);
  form->take_focus();
  form->show();

  if (mode == m_unset) {
    mode = m_modal;
    wnd->setModal(true);
    update_form(form);
    while (mode == m_modal && wnd->isModal()) {
      fltk::wait();
    }
    update_form(form);
    ui_reset();
  }
}

C_LINKAGE_END

// End of "$Id$".