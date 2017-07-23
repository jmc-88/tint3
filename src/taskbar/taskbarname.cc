/**************************************************************************
*
* Tint3 : taskbarname
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

#include "panel.hh"
#include "server.hh"
#include "taskbar.hh"
#include "taskbar/taskbarname.hh"
#include "util/common.hh"
#include "util/window.hh"

bool taskbarname_enabled;
PangoFontDescription* taskbarname_font_desc;
Color taskbarname_font;
Color taskbarname_active_font;

void Taskbarname::Default() {
  taskbarname_enabled = false;
  taskbarname_font_desc = nullptr;
}

void Taskbarname::InitPanel(Panel* panel) {
  if (!taskbarname_enabled) {
    return;
  }

  auto desktop_names = server.GetDesktopNames();
  auto it = desktop_names.begin();

  for (unsigned int j = 0; j < panel->num_desktops_; ++j) {
    Taskbar& tskbar = panel->taskbar_[j];
    tskbar.bar_name = panel->g_taskbar.bar_name_;
    tskbar.bar_name.parent_ = &tskbar;
    tskbar.bar_name.set_name(*it++);

    if (j == server.desktop()) {
      tskbar.bar_name.bg_ = panel->g_taskbar.background_name[kTaskbarActive];
    } else {
      tskbar.bar_name.bg_ = panel->g_taskbar.background_name[kTaskbarNormal];
    }

    // append the name at the beginning of taskbar
    tskbar.children_.push_back(&(tskbar.bar_name));
  }
}

void Taskbarname::Cleanup() {
  for (Panel& panel : panels) {
    for (unsigned int j = 0; j < panel.num_desktops_; j++) {
      Taskbar& tskbar = panel.taskbar_[j];

      tskbar.bar_name.FreeArea();

      for (int k = 0; k < kTaskbarCount; ++k) {
        tskbar.bar_name.reset_state_pixmap(k);
      }

      auto it = std::find(tskbar.children_.begin(), tskbar.children_.end(),
                          &(tskbar.bar_name));

      if (it != tskbar.children_.end()) {
        tskbar.children_.erase(it);
      }
    }
  }
}

std::string const& Taskbarname::name() const { return name_; }

Taskbarname& Taskbarname::set_name(std::string const& name) {
  name_.assign(name);
  return (*this);
}

void Taskbarname::DrawForeground(cairo_t* c) {
  Taskbar* taskbar = reinterpret_cast<Taskbar*>(parent_);

  // TODO: the parent should return this value, without the children knowing
  // about its internals
  Color const& config_text = (taskbar->desktop == server.desktop())
                                 ? taskbarname_active_font
                                 : taskbarname_font;

  // TODO: the parent should return this state, without the children knowing
  // about its internals
  int state =
      (taskbar->desktop == server.desktop()) ? kTaskbarActive : kTaskbarNormal;
  set_state_pixmap(state, pix_);

  // draw content
  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), taskbarname_font_desc);
  pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);
  pango_layout_set_alignment(layout.get(), PANGO_ALIGN_CENTER);
  pango_layout_set_text(layout.get(), name_.c_str(), name_.length());

  cairo_set_source_rgba(c, config_text[0], config_text[1], config_text[2],
                        config_text.alpha());

  pango_cairo_update_layout(c, layout.get());
  cairo_move_to(c, 0, panel_y_);
  pango_cairo_show_layout(c, layout.get());
}

bool Taskbarname::Resize() {
  need_redraw_ = true;

  int name_width, name_height;
  GetTextSize(taskbarname_font_desc, name(), &name_width, &name_height);

  if (panel_horizontal) {
    int new_size = name_width + (2 * (padding_x_lr_ + bg_.border().width()));

    if (new_size != static_cast<int>(width_)) {
      width_ = new_size;
      panel_y_ = (height_ - name_height) / 2;
      return true;
    }
  } else {
    int new_size = name_height + (2 * (padding_x_lr_ + bg_.border().width()));

    if (new_size != static_cast<int>(height_)) {
      height_ = new_size;
      panel_y_ = (height_ - name_height) / 2;
      return true;
    }
  }

  return false;
}
