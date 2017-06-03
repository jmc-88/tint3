#include "tooltip/tooltip.hh"

#include "panel.hh"
#include "util/common.hh"
#include "util/log.hh"
#include "util/window.hh"

TooltipConfig tooltip_config;

TooltipConfig::~TooltipConfig() {
  if (font_desc) {
    pango_font_description_free(font_desc);
  }
}

TooltipConfig TooltipConfig::Default() {
  TooltipConfig cfg;
  cfg.show_timeout_msec = 0;
  cfg.hide_timeout_msec = 0;
  cfg.paddingx = 0;
  cfg.paddingy = 0;
  cfg.font_desc = nullptr;
  cfg.bg = Background{};
  cfg.font_color = Color{{1.0, 1.0, 1.0}, 1.0};
  return cfg;
}

Tooltip::Tooltip(Server* server, Timer* timer)
    : server_(server),
      timer_(timer),
      area_(nullptr),
      font_desc_(nullptr),
      window_(None) {
  font_desc_ = tooltip_config.font_desc;
  if (!font_desc_) {
    font_desc_ = pango_font_description_from_string("sans 10");
  }

  XSetWindowAttributes attr;
  attr.override_redirect = True;
  attr.event_mask = StructureNotifyMask;
  attr.colormap = server_->colormap;
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  unsigned long mask = CWEventMask | CWColormap | CWBorderPixel | CWBackPixel |
                       CWOverrideRedirect;

  window_ = util::x11::CreateWindow(server_->root_window(), 0, 0, 100, 20, 0,
                                    server_->depth, InputOutput,
                                    server_->visual, mask, &attr);
}

Tooltip::~Tooltip() {
  if (window_ != None) {
    XDestroyWindow(server_->dsp, window_);
  }
}

Window Tooltip::window() const { return window_; }

bool Tooltip::IsBound() const { return area_; }

bool Tooltip::IsBoundTo(Area const* area) const { return area == area_; }

void Tooltip::Show(Area const* area, XEvent const* e, std::string text) {
  if (timeout_) {
    timer_->ClearInterval(timeout_);
  }
  timeout_ = timer_->SetTimeout(
      std::chrono::milliseconds(tooltip_config.show_timeout_msec), [=] {
        timeout_.Clear();
        XMapWindow(server_->dsp, window_);
        Update(area, e, text);
        XFlush(server_->dsp);
        return false;
      });
}

void Tooltip::Update(Area const* area, XEvent const* e,
                     std::string const& text) {
  // bind to the given area
  area_ = area;

  // figure out position and size
  int x = area_->panel_x_ + area_->width_ / 2;
  if (e) {
    x += e->xmotion.x_root - e->xmotion.x;
  }
  int y = area_->panel_y_ + area_->height_ / 2;
  if (e) {
    y += e->xmotion.y_root - e->xmotion.y;
  }
  int width, height;
  GetExtents(text, &x, &y, &width, &height);
  XMoveResizeWindow(server_->dsp, window_, x, y, width, height);

  // redraw it
  auto cs = cairo_xlib_surface_create(server_->dsp, window_, server_->visual,
                                      width, height);
  auto c = cairo_create(cs);
  DrawBackground(c, width, height);
  DrawBorder(c, width, height);
  DrawText(c, width, height, text);
  cairo_destroy(c);
  cairo_surface_destroy(cs);
}

void Tooltip::GetExtents(std::string const& text, int* x, int* y, int* width,
                         int* height) {
  int text_width, text_height;
  GetTextSize(font_desc_, text, &text_width, &text_height);

  // Find the base dimensions and positions
  Border b = tooltip_config.bg.border();
  const int w = b.width();
  (*width) = 2 * w + 2 * tooltip_config.paddingx + text_width;
  (*height) = 2 * w + 2 * tooltip_config.paddingy + text_height;

  Panel const* panel = area_->panel_;
  if (panel_horizontal) {
    (*x) -= (*width) / 2;
    if (panel_vertical_position == PanelVerticalPosition::kBottom) {
      (*y) = panel->root_y_ - (*height);
    } else if (panel_vertical_position == PanelVerticalPosition::kTop) {
      (*y) = panel->root_y_ + panel->height_;
    }
  } else {
    (*y) -= (*height) / 2;
    if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
      (*x) = panel->root_x_ + panel->width_;
    } else {
      (*x) = panel->root_x_ - (*width);
    }
  }

  // Limit the size of the tooltip
  int max_width = server_->monitor[panel->monitor_].width;
  int max_height = server_->monitor[panel->monitor_].height;
  if (panel_horizontal) {
    max_height -= panel->height_;
  } else {
    max_width -= panel->width_;
  }
  (*width) = std::min(*width, max_width);
  (*height) = std::min(*height, max_height);

  // Don't escape the screen from the left or the top side
  int min_x = 0;
  int min_y = 0;
  if (panel_horizontal) {
    if (panel_vertical_position == PanelVerticalPosition::kBottom) {
      min_y = panel->height_;
    }
  } else {
    if (panel_horizontal_position == PanelHorizontalPosition::kLeft) {
      min_x = panel->width_;
    }
  }
  (*x) = std::max(*x, min_x);
  (*y) = std::max(*y, min_y);

  // Don't escape the screen from the right or the bottom side
  int max_x = server_->monitor[panel->monitor_].x +
              server_->monitor[panel->monitor_].width - (*width);
  int max_y = server_->monitor[panel->monitor_].y +
              server_->monitor[panel->monitor_].height - (*height);
  (*x) = std::min(*x, max_x);
  (*y) = std::min(*y, max_y);
}

void Tooltip::DrawBackground(cairo_t* c, int width, int height) {
  Color bc = tooltip_config.bg.fill_color();
  Border b = tooltip_config.bg.border();
  const int w = b.width();

  if (server_->real_transparency) {
    ClearPixmap(window_, 0, 0, width, height);
    DrawRect(c, w, w, width - 2 * w, height - w, b.rounded() - w / 1.571);
    cairo_set_source_rgba(c, bc[0], bc[1], bc[2], bc.alpha());
  } else {
    cairo_rectangle(c, 0, 0, width, height);
    cairo_set_source_rgb(c, bc[0], bc[1], bc[2]);
  }
  cairo_fill(c);
}

void Tooltip::DrawBorder(cairo_t* c, int width, int height) {
  Border b = tooltip_config.bg.border();
  const int w = b.width();
  cairo_set_line_width(c, w);

  if (server_->real_transparency) {
    DrawRect(c, w / 2.0, w / 2.0, width - w, height - w, b.rounded());
  } else {
    cairo_rectangle(c, w / 2.0, w / 2.0, width - w, height - w);
  }

  cairo_set_source_rgba(c, b[0], b[1], b[2], b.alpha());
  cairo_stroke(c);
}

void Tooltip::DrawText(cairo_t* c, int width, int height,
                       std::string const& text) {
  cairo_set_source_rgba(
      c, tooltip_config.font_color[0], tooltip_config.font_color[1],
      tooltip_config.font_color[2], tooltip_config.font_color.alpha());

  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), font_desc_);
  pango_layout_set_text(layout.get(), text.c_str(), -1);

  PangoRectangle r1, r2;
  pango_layout_get_pixel_extents(layout.get(), &r1, &r2);
  pango_layout_set_width(layout.get(), width * PANGO_SCALE);
  pango_layout_set_height(layout.get(), height * PANGO_SCALE);
  pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_END);

  Border b = tooltip_config.bg.border();
  const int w = b.width();
  cairo_move_to(c, -r1.x / 2 + w + tooltip_config.paddingx,
                -r1.y / 2 + w + tooltip_config.paddingy);
  pango_cairo_show_layout(c, layout.get());
}

void Tooltip::Hide() {
  if (timeout_) {
    timer_->ClearInterval(timeout_);
  }
  timeout_ = timer_->SetTimeout(
      std::chrono::milliseconds(tooltip_config.hide_timeout_msec), [=] {
        area_ = nullptr;
        timeout_.Clear();
        XUnmapWindow(server_->dsp, window_);
        XFlush(server_->dsp);
        return false;
      });
}
