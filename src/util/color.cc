#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "util/color.hh"

namespace {

unsigned int HexCharToInt(char c) {
  c = std::tolower(c);
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return 0;
}

bool HexToRgb(std::string const& hex, unsigned int* r, unsigned int* g,
              unsigned int* b) {
  if (hex.empty() || hex[0] != '#') {
    return false;
  }

  if (hex.length() == 3 + 1) {
    (*r) = HexCharToInt(hex[1]) * 16 + HexCharToInt(hex[1]);
    (*g) = HexCharToInt(hex[2]) * 16 + HexCharToInt(hex[2]);
    (*b) = HexCharToInt(hex[3]) * 16 + HexCharToInt(hex[3]);
  } else if (hex.length() == 6 + 1) {
    (*r) = HexCharToInt(hex[1]) * 16 + HexCharToInt(hex[2]);
    (*g) = HexCharToInt(hex[3]) * 16 + HexCharToInt(hex[4]);
    (*b) = HexCharToInt(hex[5]) * 16 + HexCharToInt(hex[6]);
  } else if (hex.length() == 12 + 1) {
    (*r) = HexCharToInt(hex[1]) * 16 + HexCharToInt(hex[2]);
    (*g) = HexCharToInt(hex[5]) * 16 + HexCharToInt(hex[6]);
    (*b) = HexCharToInt(hex[9]) * 16 + HexCharToInt(hex[10]);
  } else {
    return false;
  }

  return true;
}

}  // namespace

Color::Color() : color_(), alpha_() {}

Color::Color(Color::Array const& color, double alpha)
    : color_(color), alpha_(alpha) {}

Color::Color(Color const& other) : color_(other.color_), alpha_(other.alpha_) {}

Color::Color(Color&& other)
    : color_(std::move(other.color_)), alpha_(std::move(other.alpha_)) {}

Color& Color::operator=(Color other) {
  std::swap(color_, other.color_);
  std::swap(alpha_, other.alpha_);
  return (*this);
}

bool Color::SetColorFromHexString(std::string const& hex) {
  unsigned int r, g, b;
  if (!HexToRgb(hex, &r, &g, &b)) {
    return false;
  }

  color_[0] = (r / 255.0);
  color_[1] = (g / 255.0);
  color_[2] = (b / 255.0);
  return true;
}

double Color::operator[](unsigned int i) const { return color_.at(i); }

double Color::alpha() const { return alpha_; }

void Color::set_alpha(double alpha) { alpha_ = alpha; }

bool Color::operator==(Color const& other) const {
  return (color_[0] == other.color_[0] && color_[1] == other.color_[1] &&
          color_[2] == other.color_[2] && alpha_ == other.alpha_);
}

bool Color::operator!=(Color const& other) const { return !(*this == other); }

Border::Border() : width_(0), rounded_(0) {}

Border::Border(Border const& other)
    : Color(other), width_(other.width_), rounded_(other.rounded_) {}

Border::Border(Border&& other)
    : Color(other),
      width_(std::move(other.width_)),
      rounded_(std::move(other.rounded_)) {}

Border& Border::operator=(Border other) {
  Color::operator=(other);
  std::swap(width_, other.width_);
  std::swap(rounded_, other.rounded_);
  return (*this);
}

void Border::set_color(Color const& other) { Color::operator=(other); }

int Border::width() const { return width_; }

void Border::set_width(int width) { width_ = width; }

int Border::rounded() const { return rounded_; }

void Border::set_rounded(int rounded) { rounded_ = rounded; }

bool Border::operator==(Border const& other) const {
  return Color::operator==(other) && (width_ == other.width_) &&
         (rounded_ == other.rounded_);
}

bool Border::operator!=(Border const& other) const { return !(*this == other); }

Background::Background()
    : gradient_id_(-1), gradient_id_hover_(-1), gradient_id_pressed_(-1) {}

Color Background::fill_color() const { return fill_color_; }

void Background::set_fill_color(Color const& color) { fill_color_ = color; }

Color Background::fill_color_hover() const {
  if (!fill_color_hover_) {
    return fill_color_;
  }
  return fill_color_hover_.Unwrap();
}

void Background::set_fill_color_hover(Color const& color) {
  fill_color_hover_ = color;
}

Color Background::fill_color_pressed() const {
  if (!fill_color_pressed_) {
    return fill_color_;
  }
  return fill_color_pressed_.Unwrap();
}

void Background::set_fill_color_pressed(Color const& color) {
  fill_color_pressed_ = color;
}

Border const& Background::border() const { return border_; }

Border& Background::border() { return border_; }

void Background::set_border(Border const& border) { border_ = border; }

Color Background::border_color_hover() const {
  if (!border_color_hover_) {
    return border_;
  }
  return border_color_hover_.Unwrap();
}

void Background::set_border_color_hover(Color const& color) {
  border_color_hover_ = color;
}

Color Background::border_color_pressed() const {
  if (!border_color_pressed_) {
    return border_;
  }
  return border_color_pressed_.Unwrap();
}

void Background::set_border_color_pressed(Color const& color) {
  border_color_pressed_ = color;
}

int Background::gradient_id() const { return gradient_id_; }

void Background::set_gradient_id(int id) { gradient_id_ = id; }

int Background::gradient_id_hover() const { return gradient_id_hover_; }

void Background::set_gradient_id_hover(int id) { gradient_id_hover_ = id; }

int Background::gradient_id_pressed() const { return gradient_id_pressed_; }

void Background::set_gradient_id_pressed(int id) { gradient_id_pressed_ = id; }

bool Background::operator==(Background const& other) const {
  return (fill_color_ == other.fill_color_) &&
         (fill_color_hover_ == other.fill_color_hover_) &&
         (fill_color_pressed_ == other.fill_color_pressed_) &&
         (border_ == other.border_) &&
         (border_color_hover_ == other.border_color_hover_) &&
         (border_color_pressed_ == other.border_color_pressed_) &&
         (gradient_id_ == other.gradient_id_) &&
         (gradient_id_hover_ == other.gradient_id_hover_) &&
         (gradient_id_pressed_ == other.gradient_id_pressed_);
}

bool Background::operator!=(Background const& other) const {
  return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, Color const& color) {
  unsigned char r = (color[0] * 255);
  unsigned char g = (color[1] * 255);
  unsigned char b = (color[2] * 255);
  return os << "rgba(" << static_cast<unsigned int>(r) << ", "
            << static_cast<unsigned int>(g) << ", "
            << static_cast<unsigned int>(b) << ", " << color.alpha() << ")";
}
