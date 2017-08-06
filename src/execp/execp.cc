#include <pango/pangocairo.h>

#include "execp/execp.hh"
#include "panel.hh"
#include "util/common.hh"
#include "util/log.hh"
#include "util/window.hh"

Executor::Executor()
    : Area(),
      cache_icon_(),
      centered_(),
      continuous_(),
      has_icon_(),
      icon_height_(),
      icon_width_(),
      interval_(),
      markup_() {
  size_mode_ = SizeMode::kByContent;
}

void Executor::InitPanel(Panel* panel) {
  parent_ = panel;
  panel_ = panel;
  need_resize_ = true;
  on_screen_ = true;
}

bool Executor::Resize() {
  int text_width, text_height;
  GetTextSize(font_description_, command_, &text_width, &text_height);

  width_ = text_width;
  height_ = text_height;
  need_redraw_ = true;
  return false;
}

void Executor::DrawForeground(cairo_t* c) {
  cairo_set_source_rgba(c, font_color_[0], font_color_[1], font_color_[2],
                        font_color_.alpha());

  util::GObjectPtr<PangoLayout> layout(pango_cairo_create_layout(c));
  pango_layout_set_font_description(layout.get(), font_description_());
  if (markup_) {
    pango_layout_set_markup(layout.get(), command_.c_str(), -1);
  } else {
    pango_layout_set_text(layout.get(), command_.c_str(), -1);
  }

  PangoRectangle r1;
  pango_layout_get_pixel_extents(layout.get(), &r1, nullptr);
  pango_layout_set_width(layout.get(), width_ * PANGO_SCALE);
  pango_layout_set_height(layout.get(), height_ * PANGO_SCALE);
  pango_layout_set_ellipsize(layout.get(), PANGO_ELLIPSIZE_END);

  Border b = bg_.border();
  const int w = b.width();
  cairo_move_to(c, -r1.x / 2 + w, -r1.y / 2 + w);
  pango_cairo_show_layout(c, layout.get());
}

std::string Executor::GetTooltipText() { return command_; }

void Executor::set_cache_icon(bool cache_icon) { cache_icon_ = cache_icon; }

void Executor::set_centered(bool centered) { centered_ = centered; }

std::string const& Executor::command() const { return command_; }

void Executor::set_command(std::string const& command) { command_ = command; }

void Executor::set_command_left_click(std::string const& command) {
  command_left_click_ = command;
}

void Executor::set_command_middle_click(std::string const& command) {
  command_middle_click_ = command;
}

void Executor::set_command_right_click(std::string const& command) {
  command_right_click_ = command;
}

void Executor::set_command_up_wheel(std::string const& command) {
  command_up_wheel_ = command;
}

void Executor::set_command_down_wheel(std::string const& command) {
  command_down_wheel_ = command;
}

void Executor::set_continuous(bool continuous) { continuous_ = continuous; }

void Executor::set_font(std::string const& font) {
  font_description_ = pango_font_description_from_string(font.c_str());
}
void Executor::set_font_color(Color const& color) { font_color_ = color; }

void Executor::set_has_icon(bool has_icon) { has_icon_ = has_icon; }

unsigned int Executor::icon_height() const { return icon_height_; }

void Executor::set_icon_height(unsigned int height) { icon_height_ = height; }

unsigned int Executor::icon_width() const { return icon_width_; }

void Executor::set_icon_width(unsigned int width) { icon_width_ = width; }

unsigned int Executor::interval() const { return interval_; }

void Executor::set_interval(unsigned int interval) { interval_ = interval; }

void Executor::set_markup(bool markup) { markup_ = markup; }

void Executor::set_tooltip(std::string const& tooltip) { tooltip_ = tooltip; }
