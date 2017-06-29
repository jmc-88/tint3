#include "execp/execp.hh"

#include <algorithm>

PangoFontDescriptionPtr::PangoFontDescriptionPtr()
    : font_description_(pango_font_description_from_string("sans 10")) {}

PangoFontDescriptionPtr::PangoFontDescriptionPtr(
    PangoFontDescriptionPtr const& other)
    : font_description_(pango_font_description_copy(other.font_description_)) {}

PangoFontDescriptionPtr::PangoFontDescriptionPtr(
    PangoFontDescriptionPtr&& other)
    : font_description_(std::move(other.font_description_)) {
  other.font_description_ = nullptr;
}

PangoFontDescriptionPtr::~PangoFontDescriptionPtr() {
  if (font_description_) {
    pango_font_description_free(font_description_);
  }
}

PangoFontDescriptionPtr& PangoFontDescriptionPtr::operator=(
    PangoFontDescriptionPtr other) {
  std::swap(font_description_, other.font_description_);
  return (*this);
}

PangoFontDescriptionPtr& PangoFontDescriptionPtr::operator=(
    PangoFontDescription* ptr) {
  if (font_description_) {
    pango_font_description_free(font_description_);
  }
  font_description_ = ptr;
  return (*this);
}

PangoFontDescription* PangoFontDescriptionPtr::operator()() const {
  return font_description_;
}

void Executor::set_background(Background const& background) {
  background_ = background;
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
