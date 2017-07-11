#include "catch.hpp"

#include "util/pango.hh"

TEST_CASE("FontDescriptionPtr") {
  SECTION("copy constructor") {
    util::pango::FontDescriptionPtr ptr1;
    util::pango::FontDescriptionPtr ptr2{ptr1};
    REQUIRE(pango_font_description_equal(ptr1(), ptr2()));
    REQUIRE(ptr1() != ptr2());
  }

  SECTION("move constructor") {
    util::pango::FontDescriptionPtr ptr1;
    util::pango::FontDescriptionPtr ptr2{std::move(ptr1)};
    REQUIRE(ptr1() == nullptr);
    REQUIRE(ptr2() != nullptr);
  }

  SECTION("copy assignment") {
    // setup
    auto ptr1 = util::pango::FontDescriptionPtr::FromPointer(
        pango_font_description_from_string("sans 10"));
    auto ptr2 = util::pango::FontDescriptionPtr::FromPointer(
        pango_font_description_from_string("sans 8"));
    REQUIRE_FALSE(pango_font_description_equal(ptr1(), ptr2()));

    ptr1 = ptr2;
    REQUIRE(pango_font_description_equal(ptr1(), ptr2()));
    REQUIRE(ptr1() != ptr2());

    PangoFontDescription* ptr1_previous = ptr1();
    ptr1 = ptr2;
    REQUIRE(pango_font_description_equal(ptr1(), ptr2()));
    REQUIRE(ptr1() != ptr2());
    REQUIRE(ptr1() != ptr1_previous);
  }

  SECTION("assignment to PangoFontDescription*") {
    // setup
    auto ptr = util::pango::FontDescriptionPtr::FromPointer(
        pango_font_description_from_string("sans 10"));
    PangoFontDescription* font = pango_font_description_from_string("sans 8");
    REQUIRE_FALSE(pango_font_description_equal(ptr(), font));

    // assign once
    ptr = font;
    REQUIRE(pango_font_description_equal(ptr(), font));
    REQUIRE(ptr() == font);

    // assignment is idempotent
    ptr = font;
    REQUIRE(pango_font_description_equal(ptr(), font));
    REQUIRE(ptr() == font);
  }
}
