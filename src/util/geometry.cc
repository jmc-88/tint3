#include "util/geometry.hh"

namespace util {

Rect::Rect(int x, int y, unsigned int w, unsigned int h)
    : tl_(std::make_pair(x, y)), br_(std::make_pair(x + w, y + h)) {}

void Rect::ExpandBy(unsigned int p) {
  tl_ = std::make_pair(tl_.first - p, tl_.second - p);
  br_ = std::make_pair(br_.first + p, br_.second + p);
}

bool Rect::ShrinkBy(unsigned int p) {
  if (br_.first - tl_.first < 2 * p || br_.second - tl_.second < 2 * p) {
    return false;
  }

  tl_ = std::make_pair(tl_.first + p, tl_.second + p);
  br_ = std::make_pair(br_.first - p, br_.second - p);
  return true;
}

Point Rect::top_left() const { return tl_; }

Point Rect::bottom_right() const { return br_; }

}  // namespace util
