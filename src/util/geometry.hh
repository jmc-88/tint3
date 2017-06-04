#ifndef TINT3_UTIL_GEOMETRY_HH
#define TINT3_UTIL_GEOMETRY_HH

#include <utility>

namespace util {

using Point = std::pair<int, int>;

class Rect {
 public:
  Rect() = delete;

  // Tests equality of two Rect objects.
  // To be considered equal, they must have the same size *and* position.
  bool operator==(Rect const& other) const;

  // Creates a new rectangle from the given coordinates.
  // The created rectangle strecthes from (x; y) to (x + w; y + h).
  Rect(int x, int y, unsigned int w, unsigned int h);

  // Tells if the other rectangle is contained in this one.
  bool Contains(Rect const& other);

  // Expands the rectangle in all directions by the given amount of pixels.
  void ExpandBy(unsigned int p);

  // Shrinks the rectangle in all directions by the given amount of pixels.
  // Returns a boolean indicating whether shrinking succeded or failed (which
  // can happen when the rectangle is smaller than 2*p in either direction).
  bool ShrinkBy(unsigned int p);

  // Returns the top-left vertex.
  Point top_left() const;

  // Returns the bottom-right vertex.
  Point bottom_right() const;

 private:
  Point tl_;
  Point br_;
};

}  // namespace util

#endif  // TINT3_UTIL_GEOMETRY_HH
