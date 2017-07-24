#include "catch.hpp"

#include <map>
#include <utility>
#include <vector>

#include "util/color.hh"
#include "util/gradient.hh"

namespace test {

class GradientHelper {
 public:
  static util::GradientKind GetKind(util::Gradient const& g) { return g.kind_; }

  static Color const& GetStartColor(util::Gradient const& g) {
    return g.start_color_;
  }

  static Color const& GetEndColor(util::Gradient const& g) {
    return g.end_color_;
  }

  static std::map<unsigned short, Color> GetColorStops(
      util::Gradient const& g) {
    return g.color_stops_;
  }
};

Color const kTransparentBlack{{{0, 0, 0}}, 0};
Color const kSemitrasparentWhite{{{1, 1, 1}}, 0.5};

}  // namespace test

TEST_CASE("Constructor", "Construction works as expected") {
  util::Gradient default_gradient;
  REQUIRE(test::GradientHelper::GetKind(default_gradient) ==
          util::GradientKind::kVertical);
  REQUIRE(test::GradientHelper::GetStartColor(default_gradient) == Color{});
  REQUIRE(test::GradientHelper::GetEndColor(default_gradient) == Color{});

  util::Gradient horizontal_gradient{util::GradientKind::kHorizontal};
  REQUIRE(test::GradientHelper::GetKind(horizontal_gradient) ==
          util::GradientKind::kHorizontal);
  REQUIRE(test::GradientHelper::GetStartColor(horizontal_gradient) == Color{});
  REQUIRE(test::GradientHelper::GetEndColor(horizontal_gradient) == Color{});
}

TEST_CASE("set_start_color", "Setter works as expected") {
  util::Gradient g;
  g.set_start_color(test::kTransparentBlack);
  REQUIRE(test::GradientHelper::GetStartColor(g) == test::kTransparentBlack);
  g.set_start_color(test::kSemitrasparentWhite);
  REQUIRE(test::GradientHelper::GetStartColor(g) == test::kSemitrasparentWhite);
}

TEST_CASE("set_end_color", "Setter works as expected") {
  util::Gradient g;
  g.set_end_color(test::kTransparentBlack);
  REQUIRE(test::GradientHelper::GetEndColor(g) == test::kTransparentBlack);
  g.set_end_color(test::kSemitrasparentWhite);
  REQUIRE(test::GradientHelper::GetEndColor(g) == test::kSemitrasparentWhite);
}

TEST_CASE("AddColorStop", "Adding color stops works as expected") {
  util::Gradient g;

  // Reject stops outside (0; 100).
  // For the special values 0 and 100, we already have set_start_color and
  // set_end_color.
  REQUIRE_FALSE(g.AddColorStop(-1, test::kTransparentBlack));
  REQUIRE_FALSE(g.AddColorStop(0, test::kTransparentBlack));
  REQUIRE_FALSE(g.AddColorStop(100, test::kTransparentBlack));
  REQUIRE_FALSE(g.AddColorStop(101, test::kTransparentBlack));

  // Accept stops inside (0; 100) and keep them ordered.
  REQUIRE(g.AddColorStop(50, test::kTransparentBlack));
  REQUIRE(g.AddColorStop(25, test::kSemitrasparentWhite));
  REQUIRE(g.AddColorStop(75, test::kSemitrasparentWhite));

  // Set up expectations on the order of stops that we want the iterator to
  // traverse when used.
  std::vector<std::pair<unsigned short, Color>> const expected_stops{
      std::make_pair(25, test::kSemitrasparentWhite),
      std::make_pair(50, test::kTransparentBlack),
      std::make_pair(75, test::kSemitrasparentWhite),
  };
  auto it = expected_stops.cbegin();

  for (auto const& p : test::GradientHelper::GetColorStops(g)) {
    if (it == expected_stops.cend()) {
      FAIL("exhausted the expected stops, but not the iterator");
    }
    REQUIRE(p.first == it->first);
    REQUIRE(p.second == it->second);
    ++it;
  }

  if (it != expected_stops.cend()) {
    FAIL("exhausted the iterator, but not the expected stops");
  }
}
