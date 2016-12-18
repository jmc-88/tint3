/**************************************************************************
*
* Copyright (C) 2009 Andreas.Fink (Andreas.Fink85@gmail.com)
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

#include <cairo-xlib.h>
#include <cairo.h>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>

#include "panel.hh"
#include "server.hh"
#include "tooltip/tooltip.hh"
#include "util/common.hh"
#include "util/timer.hh"
#include "util/x11.hh"

static int x, y, width, height;

Tooltip g_tooltip;

namespace {

void StopTooltipTimeout(Timer& timer) {
  if (g_tooltip.timeout) {
    timer.ClearInterval(g_tooltip.timeout);
    g_tooltip.timeout.Clear();
  }
}

void StartShowTimeout(Timer& timer) {
  g_tooltip.timeout =
      timer.SetTimeout(std::chrono::milliseconds(g_tooltip.show_timeout_msec),
                       [&timer]() -> bool { return TooltipShow(timer); });
}

void StartHideTimeout(Timer& timer) {
  g_tooltip.timeout =
      timer.SetTimeout(std::chrono::milliseconds(g_tooltip.hide_timeout_msec),
                       [&timer]() -> bool { return TooltipHide(timer); });
}

}  // namespace

void DefaultTooltip() {
  // give the tooltip some reasonable default values
  g_tooltip.BindTo(nullptr);
  g_tooltip.panel = nullptr;
  g_tooltip.window = 0;
  g_tooltip.show_timeout_msec = 0;
  g_tooltip.hide_timeout_msec = 0;
  g_tooltip.mapped_ = False;
  g_tooltip.paddingx = 0;
  g_tooltip.paddingy = 0;
  g_tooltip.font_desc = nullptr;
  g_tooltip.bg = nullptr;
  g_tooltip.timeout.Clear();
  g_tooltip.font_color.color[0] = 1;
  g_tooltip.font_color.color[1] = 1;
  g_tooltip.font_color.color[2] = 1;
  g_tooltip.font_color.alpha = 1;
}

void CleanupTooltip(Timer& timer) {
  StopTooltipTimeout(timer);
  TooltipHide(timer);
  g_tooltip.BindTo(nullptr);

  if (g_tooltip.window) {
    XDestroyWindow(server.dsp, g_tooltip.window);
  }

  if (g_tooltip.font_desc != nullptr) {
    pango_font_description_free(g_tooltip.font_desc);
  }
}

void InitTooltip() {
  if (!g_tooltip.font_desc) {
    g_tooltip.font_desc = pango_font_description_from_string("sans 10");
  }

  if (g_tooltip.bg == nullptr) {
    g_tooltip.bg = backgrounds.front();
  }

  XSetWindowAttributes attr;
  attr.override_redirect = True;
  attr.event_mask = StructureNotifyMask;
  attr.colormap = server.colormap;
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  unsigned long mask = CWEventMask | CWColormap | CWBorderPixel | CWBackPixel |
                       CWOverrideRedirect;

  if (g_tooltip.window) {
    XDestroyWindow(server.dsp, g_tooltip.window);
  }

  g_tooltip.window =
      util::x11::CreateWindow(server.root_win, 0, 0, 100, 20, 0, server.depth,
                              InputOutput, server.visual, mask, &attr);
}

void TooltipTriggerShow(Area* area, Panel* p, XEvent* e, Timer& timer) {
  // Position the tooltip in the center of the area
  x = area->panel_x_ + area->width_ / 2 + e->xmotion.x_root - e->xmotion.x;
  y = area->panel_y_ + area->height_ / 2 + e->xmotion.y_root - e->xmotion.y;

  if (!panel_horizontal) {
    y -= height / 2;
  }

  g_tooltip.panel = p;

  if (g_tooltip.IsBoundTo(area)) {
    g_tooltip.BindTo(area);
    g_tooltip.Update(timer);
    StopTooltipTimeout(timer);
  } else if (!g_tooltip.mapped_) {
    StartShowTimeout(timer);
  }
}

bool TooltipShow(Timer& timer) {
  int mx, my;
  Window w;
  XTranslateCoordinates(server.dsp, server.root_win, g_tooltip.panel->main_win_,
                        x, y, &mx, &my, &w);

  if (!panel_horizontal) {
    // we adjusted y in tooltip_trigger_show, revert or we won't find the
    // correct area anymore
    my += height / 2;
  }

  Area* area = g_tooltip.panel->ClickArea(mx, my);

  if (!g_tooltip.mapped_) {
    g_tooltip.BindTo(area);
    g_tooltip.mapped_ = true;
    XMapWindow(server.dsp, g_tooltip.window);
    g_tooltip.Update(timer);
    XFlush(server.dsp);
  }

  return false;
}

void TooltipUpdateGeometry() {
  cairo_surface_t* cs = cairo_xlib_surface_create(server.dsp, g_tooltip.window,
                                                  server.visual, width, height);
  cairo_t* c = cairo_create(cs);
  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));

  pango_layout_set_font_description(layout.get(), g_tooltip.font_desc);
  pango_layout_set_text(layout.get(), g_tooltip.tooltip_text.c_str(), -1);

  PangoRectangle r1, r2;
  pango_layout_get_pixel_extents(layout.get(), &r1, &r2);
  width = 2 * g_tooltip.bg->border.width + 2 * g_tooltip.paddingx + r2.width;
  height = 2 * g_tooltip.bg->border.width + 2 * g_tooltip.paddingy + r2.height;

  Panel* panel = g_tooltip.panel;

  if (panel_horizontal &&
      panel_vertical_position == PanelVerticalPosition::kBottom) {
    y = panel->root_y_ - height;
  } else if (panel_horizontal &&
             panel_vertical_position == PanelVerticalPosition::kTop) {
    y = panel->root_y_ + panel->height_;
  } else if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
    x = panel->root_x_ + panel->width_;
  } else {
    x = panel->root_x_ - width;
  }

  cairo_destroy(c);
  cairo_surface_destroy(cs);
}

void TooltipAdjustGeometry() {
  // adjust coordinates and size to not go offscreen
  // it seems quite impossible that the height needs to be adjusted, but we do
  // it anyway.

  Panel* panel = g_tooltip.panel;
  int screen_width =
      server.monitor[panel->monitor_].x + server.monitor[panel->monitor_].width;
  int screen_height = server.monitor[panel->monitor_].y +
                      server.monitor[panel->monitor_].height;

  if (x + width <= screen_width && y + height <= screen_height &&
      x >= server.monitor[panel->monitor_].x &&
      y >= server.monitor[panel->monitor_].y) {
    return;  // no adjustment needed
  }

  int min_x, min_y, max_width, max_height;

  if (panel_horizontal) {
    min_x = 0;
    max_width = server.monitor[panel->monitor_].width;
    max_height = server.monitor[panel->monitor_].height - panel->height_;
    min_y = (panel_vertical_position == PanelVerticalPosition::kBottom)
                ? 0
                : panel->height_;
  } else {
    max_width = server.monitor[panel->monitor_].width - panel->width_;
    min_y = 0;
    max_height = server.monitor[panel->monitor_].height;
    min_x = (panel_horizontal_position == PanelHorizontalPosition::kLeft)
                ? panel->width_
                : 0;
  }

  if (x + width > server.monitor[panel->monitor_].x +
                      server.monitor[panel->monitor_].width) {
    x = server.monitor[panel->monitor_].x +
        server.monitor[panel->monitor_].width - width;
  }

  if (y + height > server.monitor[panel->monitor_].y +
                       server.monitor[panel->monitor_].height) {
    y = server.monitor[panel->monitor_].y +
        server.monitor[panel->monitor_].height - height;
  }

  x = std::max(x, min_x);
  y = std::max(y, min_y);
  width = std::min(width, max_width);
  height = std::min(height, max_height);
}

void Tooltip::Update(Timer& timer) {
  if (tooltip_text.empty()) {
    TooltipHide(timer);
    return;
  }

  TooltipUpdateGeometry();
  TooltipAdjustGeometry();
  XMoveResizeWindow(server.dsp, window, x, y, width, height);

  // Stuff for drawing the tooltip
  auto cs = cairo_xlib_surface_create(server.dsp, window, server.visual, width,
                                      height);
  auto c = cairo_create(cs);
  Color& bc = bg->back;
  Border& b = bg->border;

  if (server.real_transparency) {
    ClearPixmap(window, 0, 0, width, height);
    DrawRect(c, b.width, b.width, width - 2 * b.width, height - 2 * b.width,
             b.rounded - b.width / 1.571);
    cairo_set_source_rgba(c, bc.color[0], bc.color[1], bc.color[2], bc.alpha);
  } else {
    cairo_rectangle(c, 0., 0, width, height);
    cairo_set_source_rgb(c, bc.color[0], bc.color[1], bc.color[2]);
  }

  cairo_fill(c);
  cairo_set_line_width(c, b.width);

  if (server.real_transparency) {
    DrawRect(c, b.width / 2.0, b.width / 2.0, width - b.width, height - b.width,
             b.rounded);
  } else {
    cairo_rectangle(c, b.width / 2.0, b.width / 2.0, width - b.width,
                    height - b.width);
  }

  cairo_set_source_rgba(c, b.color[0], b.color[1], b.color[2], b.alpha);
  cairo_stroke(c);

  Color& fc = font_color;
  cairo_set_source_rgba(c, fc.color[0], fc.color[1], fc.color[2], fc.alpha);

  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), font_desc);
  pango_layout_set_text(layout.get(), tooltip_text.c_str(), -1);

  PangoRectangle r1, r2;
  pango_layout_get_pixel_extents(layout.get(), &r1, &r2);
  pango_layout_set_width(layout.get(), width * PANGO_SCALE);
  pango_layout_set_height(layout.get(), height * PANGO_SCALE);
  pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_END);
  // I do not know why this is the right way, but with the below cairo_move_to
  // it seems to be centered (horiz. and vert.)
  cairo_move_to(c, -r1.x / 2 + bg->border.width + paddingx,
                -r1.y / 2 + bg->border.width + paddingy);
  pango_cairo_show_layout(c, layout.get());

  cairo_destroy(c);
  cairo_surface_destroy(cs);
}

void TooltipTriggerHide(Timer& timer) {
  if (g_tooltip.mapped_) {
    g_tooltip.BindTo(nullptr);
    StartHideTimeout(timer);
  } else {
    // tooltip not visible yet, but maybe a timeout is still pending
    StopTooltipTimeout(timer);
  }
}

bool TooltipHide(Timer& timer) {
  if (g_tooltip.mapped_) {
    g_tooltip.mapped_ = false;
    XUnmapWindow(server.dsp, g_tooltip.window);
    XFlush(server.dsp);
  }

  return false;
}

void Tooltip::BindTo(Area* area) {
  tooltip_text.clear();

  if (area) {
    std::string tooltip = area->GetTooltipText();

    if (!tooltip.empty()) {
      tooltip_text.assign(tooltip);
    }
  }

  area_ = area;
}

bool Tooltip::IsBoundTo(Area* area) const { return (mapped_ && area_ == area); }
