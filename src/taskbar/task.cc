/**************************************************************************
*
* Tint3 : task
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "panel.hh"
#include "server.hh"
#include "taskbar/task.hh"
#include "taskbar/taskbar.hh"
#include "tooltip/tooltip.hh"
#include "util/common.hh"
#include "util/log.hh"
#include "util/timer.hh"
#include "util/window.hh"

namespace {

const char kUntitled[] = "Untitled";

unsigned int GetMonitor(Window win) {
  unsigned int monitor = 0;

  if (panels.size() > 1) {
    monitor = util::window::GetMonitor(win);
  }

  if (monitor >= panels.size()) {
    monitor = 0;
  }

  return monitor;
}

}  // namespace

Interval::Id urgent_timeout;
std::list<Task*> urgent_list;

Global_task::Global_task() {
  for (int i = 0; i < kTaskStateCount; ++i) {
    background[i] = Background{};
  }
}

Task::Task(Timer& timer) : timer_(timer) { set_has_mouse_effects(true); }

std::string Task::GetTooltipText() {
  return tooltip_enabled_ ? title_ : std::string();
}

Task& Task::SetTooltipEnabled(bool is_enabled) {
  tooltip_enabled_ = is_enabled;
  return (*this);
}

Task* AddTask(Window win, Timer& timer) {
  if (win == 0 || util::window::IsHidden(win)) {
    return nullptr;
  }

  int monitor = GetMonitor(win);

  Task new_tsk{timer};
  new_tsk.win = win;
  new_tsk.desktop = util::window::GetDesktop(win);
  new_tsk.panel_ = &panels[monitor];
  new_tsk.current_state =
      util::window::IsIconified(win) ? kTaskIconified : kTaskNormal;

  // allocate only one title and one icon
  // even with task_on_all_desktop and with task_on_all_panel
  for (int k = 0; k < kTaskStateCount; ++k) {
    new_tsk.icon[k] = 0;
    new_tsk.icon_hover[k] = 0;
    new_tsk.icon_pressed[k] = 0;
    new_tsk.state_pix[k] = None;
  }

  new_tsk.UpdateTitle();
  GetIcon(&new_tsk);

  util::log::Debug() << "task: \"" << new_tsk.GetTitle()
                     << "\", desktop: " << new_tsk.desktop
                     << ", monitor: " << monitor << '\n';
  XSelectInput(server.dsp, new_tsk.win,
               PropertyChangeMask | StructureNotifyMask);

  TaskPtrArray task_group;
  Task* new_tsk2 = nullptr;

  for (unsigned int j = 0; j < panels[monitor].num_desktops_; j++) {
    if (new_tsk.desktop != kAllDesktops && new_tsk.desktop != j) {
      continue;
    }

    Taskbar& tskbar = panels[monitor].taskbar_[j];
    new_tsk2 = new Task{timer};

    // TODO: nuke this from planet Earth ASAP - horrible hack to mimick the
    // original memcpy() call
    new_tsk2->CloneArea(panels[monitor].g_task);

    new_tsk2->parent_ = &tskbar;
    new_tsk2->win = new_tsk.win;
    new_tsk2->desktop = new_tsk.desktop;

    // to update the current state later in set_task_state...
    new_tsk2->current_state = -1;

    if (new_tsk2->desktop == kAllDesktops && server.desktop() != j) {
      // hide ALLDESKTOP task on non-current desktop
      new_tsk2->on_screen_ = false;
    }

    new_tsk2->SetTitle(new_tsk.GetTitle());
    new_tsk2->SetTooltipEnabled(panels[monitor].g_task.tooltip_enabled);

    for (int k = 0; k < kTaskStateCount; ++k) {
      new_tsk2->icon[k] = new_tsk.icon[k];
      new_tsk2->icon_hover[k] = new_tsk.icon_hover[k];
      new_tsk2->icon_pressed[k] = new_tsk.icon_pressed[k];
      new_tsk2->state_pix[k] = None;
    }

    new_tsk2->icon_width = new_tsk.icon_width;
    new_tsk2->icon_height = new_tsk.icon_height;
    tskbar.children_.push_back(new_tsk2);
    tskbar.need_resize_ = true;
    task_group.push_back(new_tsk2);

    util::log::Debug() << "Add task (desktop " << j << ", task "
                       << new_tsk2->GetTitle() << ")\n";
  }

  win_to_task_map.insert(std::make_pair(new_tsk.win, task_group));
  new_tsk2->SetState(new_tsk.current_state);

  if (util::window::IsUrgent(win)) {
    new_tsk2->AddUrgent();
  }

  return new_tsk2;
}

void RemoveTask(Task* tsk) {
  if (!tsk) {
    return;
  }

  // free title and icon just for the first task
  // even with task_on_all_desktop and with task_on_all_panel
  tsk->SetTitle("");

  for (int k = 0; k < kTaskStateCount; ++k) {
    tsk->icon[k] = 0;
    tsk->icon_hover[k] = 0;
    tsk->icon_pressed[k] = 0;
    if (tsk->state_pix[k] != None) {
      XFreePixmap(server.dsp, tsk->state_pix[k]);
    }
  }

  auto it = win_to_task_map.find(tsk->win);

  if (it == win_to_task_map.end()) {
    return;
  }

  for (auto tsk2 : it->second) {
    tsk2->parent_->RemoveChild(tsk2);

    if (tsk2 == task_active) {
      task_active = 0;
    }

    if (tsk2 == task_drag) {
      task_drag = 0;
    }

    auto it = std::find(urgent_list.begin(), urgent_list.end(), tsk2);

    if (it != urgent_list.end()) {
      tsk2->DelUrgent();
    }

    delete tsk2;
  }

  win_to_task_map.erase(it);
}

bool Task::UpdateTitle() {
  if (!panel_->g_task.text && !panel_->g_task.tooltip_enabled) {
    return false;
  }

  auto name = ServerGetProperty<char>(win, server.atom("_NET_WM_VISIBLE_NAME"),
                                      server.atom("UTF8_STRING"), 0);

  if (name == nullptr || *name == '\0') {
    name = ServerGetProperty<char>(win, server.atom("_NET_WM_NAME"),
                                   server.atom("UTF8_STRING"), 0);
  }

  if (name == nullptr || *name == '\0') {
    name = ServerGetProperty<char>(win, server.atom("WM_NAME"), XA_STRING, 0);
  }

  // add space before title
  std::string new_title;

  if (panel_->g_task.icon) {
    new_title.assign(" ");
  }

  if (name != nullptr && *name != '\0') {
    new_title.append(name.get());
  } else {
    new_title.append(kUntitled);
  }

  // check unecessary title change
  if (title_ == new_title) {
    return false;
  }

  title_ = new_title;

  for (auto& tsk2 : TaskGetTasks(win)) {
    tsk2->title_ = title_;
    SetTaskRedraw(tsk2);
  }

  return true;
}

std::string Task::GetTitle() const { return title_; }

void Task::SetTitle(std::string const& title) { title_.assign(title); }

void GetIcon(Task* tsk) {
  Panel* panel = tsk->panel_;

  if (!panel->g_task.icon) {
    return;
  }

  for (int k = 0; k < kTaskStateCount; ++k) {
    if (tsk->icon[k]) {
      imlib_context_set_image(tsk->icon[k]);
      imlib_free_image();
      tsk->icon[k] = 0;
    }
    if (tsk->icon_hover[k]) {
      imlib_context_set_image(tsk->icon_hover[k]);
      imlib_free_image();
      tsk->icon_hover[k] = 0;
    }
    if (tsk->icon_pressed[k]) {
      imlib_context_set_image(tsk->icon_pressed[k]);
      imlib_free_image();
      tsk->icon_pressed[k] = 0;
    }
  }

  Imlib_Image img = nullptr;
  int length = 0;
  auto data = ServerGetProperty<unsigned long>(
      tsk->win, server.atom("_NET_WM_ICON"), XA_CARDINAL, &length);

  if (data != nullptr && length > 0) {
    // get ARGB icon
    int w, h;
    unsigned long* tmp_data =
        GetBestIcon(data.get(), GetIconCount(data.get(), length), length, &w,
                    &h, panel->g_task.icon_size1);
    if (tmp_data) {
      std::vector<DATA32> icon_data{&tmp_data[0], &tmp_data[w * h]};
      img = imlib_create_image_using_copied_data(w, h, icon_data.data());
    }
  } else {
    // get Pixmap icon
    util::x11::ClientData<XWMHints> hints(XGetWMHints(server.dsp, tsk->win));

    if (hints != nullptr) {
      if (hints->flags & IconPixmapHint && hints->icon_pixmap != 0) {
        // get width, height and depth for the pixmap
        Window root;
        int icon_x, icon_y;
        uint border_width, bpp;
        uint w, h;

        XGetGeometry(server.dsp, hints->icon_pixmap, &root, &icon_x, &icon_y,
                     &w, &h, &border_width, &bpp);
        imlib_context_set_drawable(hints->icon_pixmap);
        img = imlib_create_image_from_drawable(hints->icon_mask, 0, 0, w, h, 0);
      }
    }
  }

  if (img == nullptr) {
    if (default_icon != nullptr) {
      imlib_context_set_image(default_icon);
      img = imlib_clone_image();
    } else {
      img = imlib_create_image(48, 48);
      if (img) {
        imlib_context_set_image(img);
        imlib_context_set_color(0, 0, 0, 255);
        imlib_image_fill_rectangle(0, 0, 48, 48);
      }
    }
  }

  // transform icons
  imlib_context_set_image(img);
  imlib_image_set_has_alpha(1);

  int w = imlib_image_get_width();
  int h = imlib_image_get_height();
  Imlib_Image orig_image = imlib_create_cropped_scaled_image(
      0, 0, w, h, panel->g_task.icon_size1, panel->g_task.icon_size1);
  imlib_free_image();

  imlib_context_set_image(orig_image);
  tsk->icon_width = imlib_image_get_width();
  tsk->icon_height = imlib_image_get_height();

  for (int k = 0; k < kTaskStateCount; ++k) {
    auto adjusted_icon = util::imlib2::Image::CloneExisting(orig_image);
    imlib_context_set_image(adjusted_icon);
    if (panel->g_task.alpha[k] != 100 || panel->g_task.saturation[k] != 0 ||
        panel->g_task.brightness[k] != 0) {
      DATA32* data32 = imlib_image_get_data();
      AdjustASB(data32, tsk->icon_width, tsk->icon_height,
                panel->g_task.alpha[k],
                (float)panel->g_task.saturation[k] / 100,
                (float)panel->g_task.brightness[k] / 100);
      imlib_image_put_back_data(data32);
    }
    tsk->icon[k] = adjusted_icon;

    auto adjusted_hover_icon = util::imlib2::Image::CloneExisting(orig_image);
    if (panel_config.mouse_effects) {
      imlib_context_set_image(adjusted_hover_icon);
      DATA32* hover_data = imlib_image_get_data();
      AdjustASB(hover_data, tsk->icon_width, tsk->icon_height,
                panel_config.mouse_hover_alpha,
                panel_config.mouse_hover_saturation / 100.0f,
                panel_config.mouse_hover_brightness / 100.0f);
      imlib_image_put_back_data(hover_data);
    }
    tsk->icon_hover[k] = adjusted_hover_icon;

    auto adjusted_pressed_icon = util::imlib2::Image::CloneExisting(orig_image);
    if (panel_config.mouse_effects) {
      imlib_context_set_image(adjusted_pressed_icon);
      DATA32* pressed_data = imlib_image_get_data();
      AdjustASB(pressed_data, tsk->icon_width, tsk->icon_height,
                panel_config.mouse_pressed_alpha,
                panel_config.mouse_pressed_saturation / 100.0f,
                panel_config.mouse_pressed_brightness / 100.0f);
      imlib_image_put_back_data(pressed_data);
    }
    tsk->icon_pressed[k] = adjusted_hover_icon;
  }

  imlib_context_set_image(orig_image);
  imlib_free_image();

  for (auto& tsk2 : TaskGetTasks(tsk->win)) {
    tsk2->icon_width = tsk->icon_width;
    tsk2->icon_height = tsk->icon_height;

    for (int k = 0; k < kTaskStateCount; ++k) {
      tsk2->icon[k] = tsk->icon[k];
    }

    SetTaskRedraw(tsk2);
  }
}

void Task::DrawIcon(int text_width) {
  if (icon[current_state] == 0) {
    return;
  }

  // Find pos
  int pos_x;

  if (panel_->g_task.centered) {
    if (panel_->g_task.text) {
      pos_x = (width_ - text_width - panel_->g_task.icon_size1) / 2;
    } else {
      pos_x = (width_ - panel_->g_task.icon_size1) / 2;
    }
  } else {
    pos_x = panel_->g_task.padding_x_lr_ + bg_.border().width();
  }

  // Render
  if (mouse_state() == MouseState::kMouseOver) {
    imlib_context_set_image(icon_hover[current_state]);
  } else if (mouse_state() == MouseState::kMousePressed) {
    imlib_context_set_image(icon_pressed[current_state]);
  } else {
    imlib_context_set_image(icon[current_state]);
  }

  if (server.real_transparency) {
    RenderImage(pix_, pos_x, panel_->g_task.icon_posy, imlib_image_get_width(),
                imlib_image_get_height());
  } else {
    imlib_context_set_drawable(pix_);
    imlib_render_image_on_drawable(pos_x, panel_->g_task.icon_posy);
  }
}

void Task::DrawForeground(cairo_t* c) {
  state_pix[current_state] = pix_;

  int width = 0;
  int height = 0;

  if (panel_->g_task.text) {
    /* Layout */
    util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
    pango_layout_set_font_description(layout.get(), panel_->g_task.font_desc);
    pango_layout_set_text(layout.get(), title_.c_str(), -1);

    /* Drawing width and Cut text */
    // pango use U+22EF or U+2026
    pango_layout_set_width(layout.get(),
                           ((Taskbar*)parent_)->text_width_ * PANGO_SCALE);
    pango_layout_set_height(layout.get(),
                            panel_->g_task.text_height * PANGO_SCALE);
    pango_layout_set_wrap(layout.get(), PANGO_WRAP_CHAR);
    pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_END);

    /* Center text */
    if (panel_->g_task.centered) {
      pango_layout_set_alignment(layout.get(), PANGO_ALIGN_CENTER);
    } else {
      pango_layout_set_alignment(layout.get(), PANGO_ALIGN_LEFT);
    }

    pango_layout_get_pixel_size(layout.get(), &width, &height);

    Color const& config_text = panel_->g_task.font[current_state];
    cairo_set_source_rgba(c, config_text[0], config_text[1], config_text[2],
                          config_text.alpha());

    pango_cairo_update_layout(c, layout.get());
    double text_posy = (panel_->g_task.height_ - height) / 2.0;
    cairo_move_to(c, panel_->g_task.text_posx, text_posy);
    pango_cairo_show_layout(c, layout.get());

    if (panel_->g_task.font_shadow) {
      cairo_set_source_rgba(c, 0.0, 0.0, 0.0, 0.5);
      pango_cairo_update_layout(c, layout.get());
      cairo_move_to(c, panel_->g_task.text_posx + 1, text_posy + 1);
      pango_cairo_show_layout(c, layout.get());
    }
  }

  if (panel_->g_task.icon) {
    DrawIcon(width);
  }
}

void Task::OnChangeLayout() {
  long value[] = {panel_->panel_x_ + panel_x_, panel_->panel_y_ + panel_y_,
                  width_, height_};

  XChangeProperty(server.dsp, win, server.atom("_NET_WM_ICON_GEOMETRY"),
                  XA_CARDINAL, 32, PropModeReplace, (unsigned char*)value, 4);

  // reset Pixmap when position/size changed
  SetTaskRedraw(this);
}

// Given a pointer to the active task (active_task) and a pointer
// to the task that is currently under the mouse (current_task),
// return a pointer to the active task that is on the same desktop
// as current_task. Normally this is simply active_task, except when
// it is set to appear on all desktops. In that case we search for
// another Task on current_task's taskbar, with the same window as
// active_task.
Task* FindActiveTask(Task* current_task, Task* active_task) {
  if (active_task == 0) {
    return current_task;
  }

  if (active_task->desktop != kAllDesktops) {
    return active_task;
  }

  if (current_task == 0) {
    return active_task;
  }

  Taskbar* tskbar = reinterpret_cast<Taskbar*>(current_task->parent_);
  auto it = tskbar->children_.begin();

  if (taskbarname_enabled) {
    ++it;
  }

  for (; it != tskbar->children_.end(); ++it) {
    auto tsk = static_cast<Task*>(*it);

    if (tsk->win == active_task->win) {
      return tsk;
    }
  }

  return active_task;
}

Task* NextTask(Task* tsk) {
  if (tsk == nullptr) {
    return nullptr;
  }

  auto tskbar = reinterpret_cast<Taskbar*>(tsk->parent_);
  auto first = tskbar->children_.begin();

  if (taskbarname_enabled) {
    ++first;
  }

  auto it = std::find(first, tskbar->children_.end(), tsk);

  if (it == tskbar->children_.end()) {
    return nullptr;
  }

  if (++it == tskbar->children_.end()) {
    return static_cast<Task*>(*it);
  }

  return static_cast<Task*>(*first);
}

Task* PreviousTask(Task* tsk) {
  if (tsk == nullptr) {
    return nullptr;
  }

  auto tskbar = reinterpret_cast<Taskbar*>(tsk->parent_);
  auto first = tskbar->children_.begin();

  if (taskbarname_enabled) {
    ++first;
  }

  auto it = std::find(tskbar->children_.begin(), tskbar->children_.end(), tsk);

  if (it == tskbar->children_.end()) {
    return nullptr;
  }

  if (it-- == first) {
    return static_cast<Task*>(tskbar->children_.back());
  }

  return static_cast<Task*>(*it);
}

void ActiveTask() {
  if (task_active != nullptr) {
    task_active->SetState(util::window::IsIconified(task_active->win)
                              ? kTaskIconified
                              : kTaskNormal);
    task_active = nullptr;
  }

  Window w1 = util::window::GetActive();

  if (w1) {
    if (TaskGetTasks(w1).empty()) {
      Window w2;

      while (XGetTransientForHint(server.dsp, w1, &w2)) {
        w1 = w2;
      }
    }

    task_active = TaskGetTask(w1);

    if (task_active != nullptr) {
      task_active->SetState(kTaskActive);
    }
  }
}

void Task::SetState(int state) {
  if (state < 0 || state >= kTaskStateCount) {
    return;
  }

  if (current_state != state) {
    for (auto& tsk1 : TaskGetTasks(win)) {
      tsk1->current_state = state;
      tsk1->bg_ = panels[0].g_task.background[state];
      tsk1->pix_ = tsk1->state_pix[state];
      tsk1->set_mouse_state(MouseState::kMouseNormal);

      if (tsk1->state_pix[state] == None) {
        tsk1->need_redraw_ = true;
      }

      auto it = std::find(urgent_list.begin(), urgent_list.end(), tsk1);

      if (state == kTaskActive && it != urgent_list.end()) {
        tsk1->DelUrgent();
      }
    }

    panel_refresh = true;
  }
}

void SetTaskRedraw(Task* tsk) {
  for (int k = 0; k < kTaskStateCount; ++k) {
    if (tsk->state_pix[k] != None) {
      XFreePixmap(server.dsp, tsk->state_pix[k]);
    }
    tsk->state_pix[k] = None;
  }

  tsk->pix_ = None;
  tsk->need_redraw_ = true;
}

bool BlinkUrgent() {
  for (auto& t : urgent_list) {
    if (t->urgent_tick < max_tick_urgent) {
      if (t->urgent_tick++ % 2) {
        t->SetState(kTaskUrgent);
      } else {
        t->SetState(util::window::IsIconified(t->win) ? kTaskIconified
                                                      : kTaskNormal);
      }
    }
  }

  panel_refresh = true;
  return true;
}

void Task::AddUrgent() {
  // some programs set urgency hint although they are active
  if (task_active && task_active->win == win) {
    return;
  }

  // always add the first tsk for a task group (omnipresent windows)
  Task* tsk = TaskGetTask(win);
  tsk->urgent_tick = 0;

  auto it = std::find(urgent_list.begin(), urgent_list.end(), tsk);

  if (it == urgent_list.end()) {
    // not yet in the list, so we have to add it
    urgent_list.push_front(tsk);

    if (!urgent_timeout) {
      urgent_timeout = timer_.SetInterval(std::chrono::seconds(1), BlinkUrgent);
      BlinkUrgent();
    }
  }
}

void Task::DelUrgent() {
  urgent_list.erase(std::remove(urgent_list.begin(), urgent_list.end(), this),
                    urgent_list.end());

  if (urgent_list.empty()) {
    timer_.ClearInterval(urgent_timeout);
    urgent_timeout.Clear();
  }
}

#ifdef _TINT3_DEBUG

std::string Task::GetFriendlyName() const { return "Task"; }

#endif  // _TINT3_DEBUG
