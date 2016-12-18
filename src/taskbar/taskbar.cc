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
// Task* on each panel a pointer (i.e. GPtrArray.len == server.nb_desktop)

WindowToTaskMap win_to_task_map;

Task* task_active;
Task* task_drag;
int taskbar_enabled;

Taskbar& Taskbar::SetState(size_t state) {
  bg_ = panel1[0].g_taskbar.background[state];
  pix_ = state_pixmap(state);

  if (taskbarname_enabled) {
    bar_name.bg_ = panel1[0].g_taskbar.background_name[state];
    bar_name.pix_ = bar_name.state_pixmap(state);
  }

  if (panel_mode != PanelMode::kMultiDesktop) {
    on_screen_ = (state != kTaskbarNormal);
  }

  if (on_screen_ == true) {
    if (state_pixmap(state) == 0) {
      need_redraw_ = true;
    }

    if (taskbarname_enabled && bar_name.state_pixmap(state) == 0) {
      bar_name.need_redraw_ = true;
    }

    auto normal_bg = panel1[0].g_taskbar.background[kTaskbarNormal];
    auto active_bg = panel1[0].g_taskbar.background[kTaskbarActive];

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
  taskbar_enabled = 0;
  Taskbarname::Default();
}

void CleanupTaskbar(Timer& timer) {
  Taskbarname::Cleanup();

  for (auto const& pair : win_to_task_map) {
    TaskbarRemoveTask(pair.first, timer);
  }

  win_to_task_map.clear();

  for (int i = 0; i < nb_panel; ++i) {
    Panel& panel = panel1[i];

    for (int j = 0; j < panel.nb_desktop_; ++j) {
      Taskbar* tskbar = &panel.taskbar_[j];

      for (int k = 0; k < kTaskbarCount; ++k) {
        tskbar->reset_state_pixmap(k);
      }

      tskbar->FreeArea();
      // remove taskbar from the panel
      auto it =
          std::find(panel.children_.begin(), panel.children_.end(), tskbar);

      if (it != panel.children_.end()) {
        panel.children_.erase(it);
      }
    }

    if (panel.taskbar_ != nullptr) {
      delete[] panel.taskbar_;
      panel.taskbar_ = nullptr;
    }
  }
}

void InitTaskbar() {
  task_active = nullptr;
  task_drag = nullptr;
}

void Taskbar::InitPanel(Panel* panel) {
  if (panel->g_taskbar.background[kTaskbarNormal] == nullptr) {
    panel->g_taskbar.background[kTaskbarNormal] = backgrounds.front();
    panel->g_taskbar.background[kTaskbarActive] = backgrounds.front();
  }

  if (panel->g_taskbar.background_name[kTaskbarNormal] == nullptr) {
    panel->g_taskbar.background_name[kTaskbarNormal] = backgrounds.front();
    panel->g_taskbar.background_name[kTaskbarActive] = backgrounds.front();
  }

  if (panel->g_task.bg_ == nullptr) {
    panel->g_task.bg_ = backgrounds.front();
  }

  // taskbar name
  panel->g_taskbar.bar_name_.panel_ = panel;
  panel->g_taskbar.bar_name_.size_mode_ = SizeMode::kByContent;
  panel->g_taskbar.bar_name_.need_resize_ = true;
  panel->g_taskbar.bar_name_.on_screen_ = true;

  // taskbar
  panel->g_taskbar.parent_ = panel;
  panel->g_taskbar.panel_ = panel;
  panel->g_taskbar.size_mode_ = SizeMode::kByLayout;
  panel->g_taskbar.need_resize_ = true;
  panel->g_taskbar.on_screen_ = true;

  if (panel_horizontal) {
    panel->g_taskbar.panel_y_ = (panel->bg_->border.width + panel->padding_y_);
    panel->g_taskbar.height_ = panel->height_ - (2 * panel->g_taskbar.panel_y_);
    panel->g_taskbar.bar_name_.panel_y_ = panel->g_taskbar.panel_y_;
    panel->g_taskbar.bar_name_.height_ = panel->g_taskbar.height_;
  } else {
    panel->g_taskbar.panel_x_ = panel->bg_->border.width + panel->padding_y_;
    panel->g_taskbar.width_ = panel->width_ - (2 * panel->g_taskbar.panel_x_);
    panel->g_taskbar.bar_name_.panel_x_ = panel->g_taskbar.panel_x_;
    panel->g_taskbar.bar_name_.width_ = panel->g_taskbar.width_;
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
    panel->g_task.font[kTaskNormal] = (Color){{0, 0, 0}, 0};
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
        panel->g_taskbar.background[kTaskbarNormal]->border.width +
        panel->g_taskbar.padding_y_;
    panel->g_task.height_ = panel->height_ - (2 * panel->g_task.panel_y_);
  } else {
    panel->g_task.panel_x_ =
        panel->g_taskbar.panel_x_ +
        panel->g_taskbar.background[kTaskbarNormal]->border.width +
        panel->g_taskbar.padding_y_;
    panel->g_task.width_ = panel->width_ - (2 * panel->g_task.panel_x_);
    panel->g_task.height_ = panel->g_task.maximum_height;
  }

  for (int j = 0; j < kTaskStateCount; ++j) {
    if (panel->g_task.background[j] == nullptr) {
      panel->g_task.background[j] = backgrounds.front();
    }

    auto half_height = (panel->g_task.height_ / 2);

    if (panel->g_task.background[j]->border.rounded > half_height) {
      std::string separator =
          (j == 0 ? "_" : (j == 1 ? "_active_"
                                  : (j == 2 ? "_iconified_" : "_urgent_")));

      util::log::Error() << "task" << separator << "background_id "
                         << "has a too large rounded value. "
                         << "Please fix your tint3rc.\n";
      /* backgrounds.push_back(*panel->g_task.background[j]); */
      /* panel->g_task.background[j] = backgrounds.back(); */
      panel->g_task.background[j]->border.rounded = half_height;
    }
  }

  // compute vertical position : text and icon
  int height_ink, height;
  GetTextSize(panel->g_task.font_desc, &height_ink, &height, panel->height_,
              "TAjpg");

  if (!panel->g_task.maximum_width && panel_horizontal) {
    panel->g_task.maximum_width = server.monitor[panel->monitor_].width;
  }

  panel->g_task.text_posx =
      panel->g_task.background[0]->border.width + panel->g_task.padding_x_lr_;
  panel->g_task.text_height =
      panel->g_task.height_ - (2 * panel->g_task.padding_y_);

  if (panel->g_task.icon) {
    panel->g_task.icon_size1 =
        panel->g_task.height_ - (2 * panel->g_task.padding_y_);
    panel->g_task.text_posx += panel->g_task.icon_size1;
    panel->g_task.icon_posy =
        (panel->g_task.height_ - panel->g_task.icon_size1) / 2;
  }

  panel->nb_desktop_ = server.nb_desktop;
  panel->taskbar_ = new Taskbar[server.nb_desktop];

  for (int j = 0; j < panel->nb_desktop_; j++) {
    Taskbar* tskbar = &panel->taskbar_[j];

    // TODO: nuke this from planet Earth ASAP - horrible hack to mimick the
    // original memcpy() call
    tskbar->CloneArea(panel->g_taskbar);

    tskbar->desktop = j;

    if (j == server.desktop) {
      tskbar->bg_ = panel->g_taskbar.background[kTaskbarActive];
    } else {
      tskbar->bg_ = panel->g_taskbar.background[kTaskbarNormal];
    }
  }

  Taskbarname::InitPanel(panel);
}

void TaskbarRemoveTask(Window win, Timer& timer) {
  RemoveTask(TaskGetTask(win), timer);
}

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
  auto windows = ServerGetProperty<Window>(server.root_win,
                                           server.atoms_["_NET_CLIENT_LIST"],
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
    TaskbarRemoveTask(w, timer);
  }

  // Add any new
  for (int i = 0; i < num_results; i++) {
    if (!TaskGetTask(windows.get()[i])) {
      AddTask(windows.get()[i], timer);
    }
  }
}

void Taskbar::DrawForeground(cairo_t* /* c */) {
  size_t state = (desktop == server.desktop ? kTaskbarActive : kTaskbarNormal);
  set_state_pixmap(state, pix_);
}

bool Taskbar::Resize() {
  int horizontal_size =
      (panel_->g_task.text_posx + panel_->g_task.bg_->border.width +
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

  pix_ = 0;
  need_redraw_ = true;
}

#ifdef _TINT3_DEBUG

std::string Taskbar::GetFriendlyName() const { return "Taskbar"; }

#endif  // _TINT3_DEBUG
