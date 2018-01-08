#include <algorithm>
#include <cmath>

#include "util/gradient.hh"
#include "util/log.hh"

namespace util {

Gradient::Gradient() : kind_(GradientKind::kVertical) {}

Gradient::Gradient(GradientKind kind) : kind_(kind) {}

void Gradient::set_start_color(Color color) { start_color_ = color; }

void Gradient::set_end_color(Color color) { end_color_ = color; }

bool Gradient::AddColorStop(unsigned short stop_percent, Color color) {
  if (stop_percent == 0 || stop_percent >= 100) {
    return false;
  }
  return color_stops_.emplace(stop_percent, color).second;
}

void Gradient::Draw(cairo_t* c, Rect const& r) {
  // Values for radial gradients. Center at (cx, cy), diameter equals d.
  auto cx = (r.bottom_right().first + r.top_left().first) / 2;
  auto cy = (r.bottom_right().second + r.top_left().second) / 2;
  auto d = std::min(r.bottom_right().first - r.top_left().first,
                    r.bottom_right().second - r.top_left().second);

  cairo_pattern_t* pat = nullptr;

  switch (kind_) {
    case GradientKind::kVertical:
      pat = cairo_pattern_create_linear(r.top_left().first, r.top_left().second,
                                        r.top_left().first,
                                        r.bottom_right().second);
      cairo_rectangle(c, r.top_left().first, r.top_left().second,
                      r.bottom_right().first, r.bottom_right().second);
      break;

    case GradientKind::kHorizontal:
      pat = cairo_pattern_create_linear(r.top_left().first, r.top_left().second,
                                        r.bottom_right().first,
                                        r.top_left().second);
      cairo_rectangle(c, r.top_left().first, r.top_left().second,
                      r.bottom_right().first, r.bottom_right().second);
      break;

    case GradientKind::kRadial:
      pat = cairo_pattern_create_radial(cx, cy, 0, cx, cy, d / 2);
      cairo_arc(c, cx, cy, d / 2, 0, 2 * M_PI);
      break;
  }

  if (cairo_pattern_status(pat) != CAIRO_STATUS_SUCCESS) {
    util::log::Error() << "cairo_pattern_create_*() failed\n";
    return;
  }

  cairo_pattern_add_color_stop_rgba(pat, 0.0, start_color_[0], start_color_[1],
                                    start_color_[2], start_color_.alpha());
  for (auto const& it : color_stops_) {
    cairo_pattern_add_color_stop_rgba(pat, it.first / 100.0, it.second[0],
                                      it.second[1], it.second[2],
                                      it.second.alpha());
  }
  cairo_pattern_add_color_stop_rgba(pat, 1.0, end_color_[0], end_color_[1],
                                    end_color_[2], end_color_.alpha());

  cairo_set_source(c, pat);
  cairo_fill(c);
  cairo_pattern_destroy(pat);
}

bool Gradient::operator==(Gradient const& other) const {
  return kind_ == other.kind_ && start_color_ == other.start_color_ &&
         end_color_ == other.end_color_ && color_stops_ == other.color_stops_;
}

}  // namespace util
