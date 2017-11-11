#include "catch.hpp"

#include "execp/execp.hh"

TEST_CASE("GetTooltipText") {
  SECTION("no tooltip provided") {
    Executor e;
    // TODO: this should check that GetTooltipText() == last_contents_of_stderr,
    // but that part is not yet implemented, so we can't test it here.
    REQUIRE(e.GetTooltipText().empty());
  }
  SECTION("empty tooltip provided") {
    Executor e;
    e.set_tooltip("");
    REQUIRE(e.GetTooltipText().empty());
  }
  SECTION("non-empty tooltip provided") {
    Executor e;
    e.set_tooltip("something");
    REQUIRE(e.GetTooltipText() == "something");
  }
}
