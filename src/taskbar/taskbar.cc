/**************************************************************************
*
* Tint3 : taskbar
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
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

#include <Imlib2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>

#include "panel.hh"
#include "server.hh"
#include "taskbar/task.hh"
#include "taskbar/taskbar.hh"
#include "util/log.hh"
#include "util/window.hh"

namespace {

bool FindWindow(Window const needle, Window const* const haystack,
                int num_results) {
  for (int i = 0; i < num_results; i++) {
    if (needle == haystack[i]) {
      return true;
    }
  }

  return false;
}

}  // namespace

// win_to_task_map holds for every Window an array of tasks.
// Usually the array contains only one element. However for omnipresent windows
// (windows which are visible in every taskbar) the array contains to every
// Task* on each panel a pointer (i.e. GPtrArray.len == server.num_desktops)

WindowToTaskMap win_to_task_map;

Task* task_active;
Task* task_drag;
bool taskbar_enabled;

Taskbar& Taskbar::SetState(size_t state) {
  bg_ = panels[0].g_taskbar.background[state];
  pix_ = state_pixmap(state);

  if (taskbarname_enabled) {
    bar_name.bg_ = panels[0].g_taskbar.background_name[state];
    bar_name.pix_ = bar_name.state_pixmap(state);
  }

  if (panel_mode != PanelMode::kMultiDesktop) {
    on_screen_ = (state != kTaskbarNormal);
  }

  if (on_screen_ == true) {
    if (state_pixmap(state) == None) {
      need_redraw_ = true;
    }

    if (taskbarname_enabled && bar_name.state_pixmap(state) == None) {
      bar_name.need_redraw_ = true;
    }

    auto& normal_bg = panels[0].g_taskbar.background[kTaskbarNormal];
    auto& active_bg = panels[0].g_taskbar.background[kTaskbarActive];

    if (panel_mode == PanelMode::kMultiDesktop && normal_bg != active_bg) {
      auto it = children_.begin();

      if (taskbarname_enabled) {
        ++it;
      }

      for (; it != children_.end(); ++it) {
        SetTaskRedraw(static_cast<Task*>(*it));
      }
    }
  }

  panel_refresh = true;
  return (*this);
}

void DefaultTaskbar() {
  win_to_task_map.clear();
  urgent_timeout.Clear();
  urgent_list.clear();
  taskbar_enabled = false;
  Taskbarname::Default();
}

void CleanupTaskbar() {
  Taskbarname::Cleanup();

  while (!win_to_task_map.empty()) {
    TaskbarRemoveTask(win_to_task_map.begin()->first);
  }

  for (Panel& panel : panels) {
    for (unsigned int j = 0; j < panel.num_desktops_; ++j) {
      Taskbar* tskbar = &panel.taskbars[j];
      panel.children_.erase(
          std::remove(panel.children_.begin(), panel.children_.end(), tskbar),
          panel.children_.end());
      tskbar->FreeArea();
    }
    panel.taskbars.clear();
  }
}

void InitTaskbar() {
  task_active = nullptr;
  task_drag = nullptr;
}

void Taskbar::InitPanel(Panel* panel) {
  // taskbar name
  panel->g_taskbar.bar_name.panel_ = panel;
  panel->g_taskbar.bar_name.size_mode_ = SizeMode::kByContent;
  panel->g_taskbar.bar_name.need_resize_ = true;
  panel->g_taskbar.bar_name.on_screen_ = true;

  // taskbar
  panel->g_taskbar.parent_ = panel;
  panel->g_taskbar.panel_ = panel;
  panel->g_taskbar.size_mode_ = SizeMode::kByLayout;
  panel->g_taskbar.need_resize_ = true;
  panel->g_taskbar.on_screen_ = true;

  Border const& b = panel->bg_.border();
  if (panel_horizontal) {
    panel->g_taskbar.panel_y_ =
        b.width_for_side(BORDER_TOP) + panel->padding_y_;
    panel->g_taskbar.height_ = panel->height_ - (2 * panel->g_taskbar.panel_y_);
    panel->g_taskbar.bar_name.panel_y_ = panel->g_taskbar.panel_y_;
    panel->g_taskbar.bar_name.height_ = panel->g_taskbar.height_;
  } else {
    panel->g_taskbar.panel_x_ =
        b.width_for_side(BORDER_LEFT) + panel->padding_y_;
    panel->g_taskbar.width_ = panel->width_ - (2 * panel->g_taskbar.panel_x_);
    panel->g_taskbar.bar_name.panel_x_ = panel->g_taskbar.panel_x_;
    panel->g_taskbar.bar_name.width_ = panel->g_taskbar.width_;
  }

  // task
  panel->g_task.panel_ = panel;
  panel->g_task.size_mode_ = SizeMode::kByLayout;
  panel->g_task.need_resize_ = true;
  panel->g_task.on_screen_ = true;

  if ((panel->g_task.config_asb_mask & (1 << kTaskNormal)) == 0) {
    panel->g_task.alpha[kTaskNormal] = 100;
    panel->g_task.saturation[kTaskNormal] = 0;
    panel->g_task.brightness[kTaskNormal] = 0;
  }

  if ((panel->g_task.config_asb_mask & (1 << kTaskActive)) == 0) {
    panel->g_task.alpha[kTaskActive] = panel->g_task.alpha[kTaskNormal];
    panel->g_task.saturation[kTaskActive] =
        panel->g_task.saturation[kTaskNormal];
    panel->g_task.brightness[kTaskActive] =
        panel->g_task.brightness[kTaskNormal];
  }

  if ((panel->g_task.config_asb_mask & (1 << kTaskIconified)) == 0) {
    panel->g_task.alpha[kTaskIconified] = panel->g_task.alpha[kTaskNormal];
    panel->g_task.saturation[kTaskIconified] =
        panel->g_task.saturation[kTaskNormal];
    panel->g_task.brightness[kTaskIconified] =
        panel->g_task.brightness[kTaskNormal];
  }

  if ((panel->g_task.config_asb_mask & (1 << kTaskUrgent)) == 0) {
    panel->g_task.alpha[kTaskUrgent] = panel->g_task.alpha[kTaskActive];
    panel->g_task.saturation[kTaskUrgent] =
        panel->g_task.saturation[kTaskActive];
    panel->g_task.brightness[kTaskUrgent] =
        panel->g_task.brightness[kTaskActive];
  }

  if ((panel->g_task.config_font_mask & (1 << kTaskNormal)) == 0) {
    panel->g_task.font[kTaskNormal] = Color{};
  }

  if ((panel->g_task.config_font_mask & (1 << kTaskActive)) == 0) {
    panel->g_task.font[kTaskActive] = panel->g_task.font[kTaskNormal];
  }

  if ((panel->g_task.config_font_mask & (1 << kTaskIconified)) == 0) {
    panel->g_task.font[kTaskIconified] = panel->g_task.font[kTaskNormal];
  }

  if ((panel->g_task.config_font_mask & (1 << kTaskUrgent)) == 0) {
    panel->g_task.font[kTaskUrgent] = panel->g_task.font[kTaskActive];
  }

  if ((panel->g_task.config_background_mask & (1 << kTaskNormal)) == 0) {
    panel->g_task.background[kTaskNormal] = backgrounds.front();
  }

  if ((panel->g_task.config_background_mask & (1 << kTaskActive)) == 0) {
    panel->g_task.background[kTaskActive] =
        panel->g_task.background[kTaskNormal];
  }

  if ((panel->g_task.config_background_mask & (1 << kTaskIconified)) == 0) {
    panel->g_task.background[kTaskIconified] =
        panel->g_task.background[kTaskNormal];
  }

  if ((panel->g_task.config_background_mask & (1 << kTaskUrgent)) == 0) {
    panel->g_task.background[kTaskUrgent] =
        panel->g_task.background[kTaskActive];
  }

  if (panel_horizontal) {
    panel->g_task.panel_y_ =
        panel->g_taskbar.panel_y_ +
        panel->g_taskbar.background[kTaskbarNormal].border().width() +
        panel->g_taskbar.padding_y_;
    panel->g_task.height_ = panel->height_ - (2 * panel->g_task.panel_y_);
  } else {
    panel->g_task.panel_x_ =
        panel->g_taskbar.panel_x_ +
        panel->g_taskbar.background[kTaskbarNormal].border().width() +
        panel->g_taskbar.padding_y_;
    panel->g_task.width_ = panel->width_ - (2 * panel->g_task.panel_x_);
    panel->g_task.height_ = panel->g_task.maximum_height;
  }

  for (int j = 0; j < kTaskStateCount; ++j) {
    int half_height = static_cast<int>(panel->g_task.height_ / 2);

    if (panel->g_task.background[j].border().rounded() > half_height) {
      std::string separator =
          (j == 0 ? "_" : (j == 1 ? "_active_"
                                  : (j == 2 ? "_iconified_" : "_urgent_")));

      util::log::Error() << "task" << separator << "background_id "
                         << "has a too large rounded value. "
                         << "Please fix your tint3rc.\n";
      panel->g_task.background[j].border().set_rounded(half_height);
    }
  }

  // compute vertical position : text and icon
  int height;
  GetTextSize(panel->g_task.font_desc, "TAjpg", MarkupTag::kNoMarkup,
              nullptr, &height);

  if (!panel->g_task.maximum_width && panel_horizontal) {
    panel->g_task.maximum_width = panel->monitor().width;
  }

  panel->g_task.text_posx = panel->g_task.background[0].border().width() +
                            panel->g_task.padding_x_lr_;
  panel->g_task.text_height =
      panel->g_task.height_ - (2 * panel->g_task.padding_y_);

  if (panel->g_task.icon) {
    panel->g_task.icon_size1 =
        panel->g_task.height_ - (2 * panel->g_task.padding_y_);
    panel->g_task.text_posx += panel->g_task.icon_size1;
    panel->g_task.icon_posy =
        (panel->g_task.height_ - panel->g_task.icon_size1) / 2;
  }

  panel->num_desktops_ = server.num_desktops();
  auto& taskbars = panel->taskbars;
  taskbars.resize(panel->num_desktops_);
  taskbars.shrink_to_fit();
  std::fill(taskbars.begin(), taskbars.end(), panel->g_taskbar);

  for (unsigned int j = 0; j < panel->num_desktops_; j++) {
    Taskbar* tskbar = &taskbars[j];
    tskbar->desktop = j;
    tskbar->bg_ = panel->g_taskbar.background[kTaskbarNormal];
    if (j == server.desktop()) {
      tskbar->bg_ = panel->g_taskbar.background[kTaskbarActive];
    }
  }

  Taskbarname::InitPanel(panel);
}

void TaskbarRemoveTask(Window win) { RemoveTask(TaskGetTask(win)); }

Task* TaskGetTask(Window win) {
  auto const& task_group = TaskGetTasks(win);

  if (!task_group.empty()) {
    return task_group[0];
  }

  return nullptr;
}

TaskPtrArray TaskGetTasks(Window win) {
  if (taskbar_enabled && !win_to_task_map.empty()) {
    auto it = win_to_task_map.find(win);

    if (it != win_to_task_map.end()) {
      return it->second;
    }
  }

  return TaskPtrArray();
}

void TaskRefreshTasklist(Timer& timer) {
  if (!taskbar_enabled) {
    return;
  }

  int num_results = 0;
  auto windows = ServerGetProperty<Window>(server.root_window(),
                                           server.atom("_NET_CLIENT_LIST"),
                                           XA_WINDOW, &num_results);

  if (windows == nullptr) {
    return;
  }

  std::list<Window> windows_to_remove;

  for (auto const& pair : win_to_task_map) {
    if (!FindWindow(pair.first, windows.get(), num_results)) {
      windows_to_remove.push_back(pair.first);
    }
  }

  for (auto const& w : windows_to_remove) {
    TaskbarRemoveTask(w);
  }

  // Add any new
  for (int i = 0; i < num_results; i++) {
    if (!TaskGetTask(windows.get()[i])) {
      AddTask(windows.get()[i], timer);
    }
  }
}

void Taskbar::DrawForeground(cairo_t* /* c */) {
  size_t state =
      (desktop == server.desktop() ? kTaskbarActive : kTaskbarNormal);
  set_state_pixmap(state, pix_);
}

bool Taskbar::Resize() {
  int horizontal_size =
      (panel_->g_task.text_posx + panel_->g_task.bg_.border().width() +
       panel_->g_task.padding_x_);

  if (panel_horizontal) {
    ResizeByLayout(panel_->g_task.maximum_width);

    int new_width = panel_->g_task.maximum_width;
    auto it = children_.begin();

    if (taskbarname_enabled) {
      ++it;
    }

    if (it != children_.end()) {
      new_width = (*it)->width_;
    }

    text_width_ = (new_width - horizontal_size);
  } else {
    ResizeByLayout(panel_->g_task.maximum_height);
    text_width_ =
        (width_ - (2 * panel_->g_taskbar.padding_y_) - horizontal_size);
  }

  return false;
}

bool Taskbar::RemoveChild(Area* child) {
  if (Area::RemoveChild(child)) {
    need_resize_ = true;
    return true;
  }

  return false;
}

void Taskbar::OnChangeLayout() {
  // reset Pixmap when position/size changed
  for (int k = 0; k < kTaskbarCount; ++k) {
    reset_state_pixmap(k);
  }

  pix_ = None;
  need_redraw_ = true;
}

#ifdef _TINT3_DEBUG

std::string Taskbar::GetFriendlyName() const { return "Taskbar"; }

#endif  // _TINT3_DEBUG

Global_taskbar::Global_taskbar()
    : background(kTaskbarCount), background_name(kTaskbarCount) {}
