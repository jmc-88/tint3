#ifndef TINT3_UTIL_COLOR_HH
#define TINT3_UTIL_COLOR_HH

#include <array>
#include <ostream>

#include "util/geometry.hh"
#include "absl/types/optional.h"

// forward declaration from area.hh
enum class MouseState;

class Color {
 public:
  friend std::ostream& operator<<(std::ostream& os, Color const& color);

  using Array = std::array<double, 3>;

  Color();
  Color(std::array<double, 3> const& color, double alpha);
  Color(Color const& other);
  Color(Color&& other);

  Color& operator=(Color other);
  bool SetColorFromHexString(std::string const& hex);

  double operator[](unsigned int i) const;
  double alpha() const;
  void set_alpha(double alpha);

  bool operator==(Color const& other) const;
  bool operator!=(Color const& other) const;

 private:
  Array color_;
  double alpha_;
};

enum BorderMask {
  BORDER_TOP = (1 << 0),
  BORDER_RIGHT = (1 << 1),
  BORDER_BOTTOM = (1 << 2),
  BORDER_LEFT = (1 << 3),
  BORDER_ALL = (1 << 4) - 1,
};

struct Border : public Color {
 public:
  Border();
  Border(Border const& other);
  Border(Border&& other);

  Border& operator=(Border other);

  // Returns the util::Rect corresponding to the inner area of a given Area
  // size. This is the rect (0; 0)-(w; h) for the given width and height, but
  // with the sides reduced by the border size according to which are enabled
  // or disabled by the configuration.
  util::Rect GetInnerAreaRect(unsigned int area_width,
                              unsigned int area_height) const;

  void set_color(Color const& other);

  int width() const;
  int width_for_side(unsigned int border_side) const;
  void set_width(int width);

  int rounded() const;
  void set_rounded(int rounded);

  unsigned int mask() const;
  void set_mask(unsigned int mask);

  bool operator==(Border const& other) const;
  bool operator!=(Border const& other) const;

 private:
  int width_;
  int rounded_;
  unsigned int mask_;
};

class Background {
 public:
  Background();
  Background(Background const&) = default;
  Background(Background&&) = default;
  Background& operator=(Background const&) = default;

  Color fill_color_for(MouseState mouse_state) const;
  Color fill_color() const;
  void set_fill_color(Color const& color);
  Color fill_color_hover() const;
  void set_fill_color_hover(Color const& color);
  Color fill_color_pressed() const;
  void set_fill_color_pressed(Color const& color);

  Color border_color_for(MouseState mouse_state) const;
  Border const& border() const;
  Border& border();
  void set_border(Border const& border);
  Color border_color_hover() const;
  void set_border_color_hover(Color const& border);
  Color border_color_pressed() const;
  void set_border_color_pressed(Color const& border);

  int gradient_id_for(MouseState mouse_state) const;
  int gradient_id() const;
  void set_gradient_id(int id);
  int gradient_id_hover() const;
  void set_gradient_id_hover(int id);
  int gradient_id_pressed() const;
  void set_gradient_id_pressed(int id);

  bool operator==(Background const& other) const;
  bool operator!=(Background const& other) const;

 private:
  Color fill_color_;
  absl::optional<Color> fill_color_hover_;
  absl::optional<Color> fill_color_pressed_;
  Border border_;
  absl::optional<Color> border_color_hover_;
  absl::optional<Color> border_color_pressed_;
  int gradient_id_;
  int gradient_id_hover_;
  int gradient_id_pressed_;
};

std::ostream& operator<<(std::ostream& os, Color const& color);

#endif  // TINT3_UTIL_COLOR_HH
