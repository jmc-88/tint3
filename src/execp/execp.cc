#include <pango/pangocairo.h>

#include "execp/execp.hh"
#include "panel.hh"
#include "util/common.hh"
#include "util/log.hh"
#include "util/window.hh"

Executor::Executor() { size_mode_ = SizeMode::kByContent; }

void Executor::InitPanel(Panel* panel) {
  parent_ = panel;
  panel_ = panel;
  need_resize_ = true;
  on_screen_ = true;
}

namespace {

inline MarkupTag markup_tag(bool has_markup) {
  if (has_markup) {
    return MarkupTag::kHasMarkup;
  } else {
    return MarkupTag::kNoMarkup;
  }
}

}  // namespace

bool Executor::Resize() {
  int text_width, text_height;
  GetTextSize(font_description_, command_, markup_tag(markup_), &text_width,
              &text_height);

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

std::string Executor::GetTooltipText() {
  if (has_tooltip_) {
    return tooltip_;
  }
  // TODO: return last contents of stderr here
  return {};
}

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

bool Executor::continuous() const { return continuous_ != 0; }

void Executor::set_continuous(unsigned int continuous) {
  continuous_ = continuous;
}

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

void Executor::set_tooltip(std::string const& tooltip) {
  tooltip_ = tooltip;
  has_tooltip_ = true;
}

bool Executor::HandlesClick(XEvent* event) {
  if (!Area::HandlesClick(event)) {
    return false;
  }

  int button = event->xbutton.button;
  return ((button == 1 && !command_left_click_.empty()) ||
          (button == 2 && !command_middle_click_.empty()) ||
          (button == 3 && !command_right_click_.empty()) ||
          (button == 4 && !command_up_wheel_.empty()) ||
          (button == 5 && !command_down_wheel_.empty()));
}

bool Executor::OnClick(XEvent* event) {
  if (!Area::HandlesClick(event)) {
    return false;
  }

  int button = event->xbutton.button;
  std::string const* commands[] = {
      &command_left_click_, &command_middle_click_, &command_right_click_,
      &command_up_wheel_,   &command_down_wheel_,
  };

  pid_t child_pid = util::ShellExec(*commands[button - 1]);
  return (child_pid > 0);
}
