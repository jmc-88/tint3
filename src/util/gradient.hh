#ifndef TINT3_UTIL_GRADIENT_HH
#define TINT3_UTIL_GRADIENT_HH

#include <cairo.h>

#include <map>

#include "util/color.hh"
#include "util/geometry.hh"

namespace test {

class GradientHelper;

}  // namespace test

namespace util {

enum class GradientKind { kVertical, kHorizontal, kRadial };

class Gradient {
 public:
  friend class test::GradientHelper;

  Gradient();
  Gradient(GradientKind kind);
  Gradient(Gradient&&) = default;

  Gradient(Gradient const&) = delete;
  Gradient& operator=(Gradient const&) = delete;

  void set_start_color(Color color);
  void set_end_color(Color color);
  bool AddColorStop(unsigned short stop_percent, Color color);

  void Draw(cairo_t* c, util::Rect const& r);

  bool operator==(Gradient const& other);

 private:
 public:
  GradientKind kind_;
  Color start_color_;
  Color end_color_;
  std::map<unsigned short, Color> color_stops_;
};

}  // namespace util

#endif  // TINT3_UTIL_GRADIENT_HH
