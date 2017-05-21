#include "catch.hpp"

#include <iostream>
#include <string>

#include "dnd/dnd.hh"

class FakeStringProperty : public dnd::Property {
 public:
  FakeStringProperty() = delete;
  explicit FakeStringProperty(const char* data, unsigned int length)
      : dnd::Property{data, 8, length - 1, XA_STRING} {}
};

TEST_CASE("BuildCommand", "Command construction is (somewhat) sane") {
  constexpr char plain[] = "one\ntwo\nthree";
  FakeStringProperty plain_prop{plain, sizeof(plain) / sizeof(char)};
  REQUIRE(dnd::BuildCommand("test_command", plain_prop) ==
          "test_command \"one\" \"two\" \"three\"");

  constexpr char escape[] = "one`\ntwo$\nthree\\";
  FakeStringProperty escape_prop{escape, sizeof(escape) / sizeof(char)};
  REQUIRE(dnd::BuildCommand("test_command", escape_prop) ==
          "test_command \"one\\`\" \"two\\$\" \"three\\\\\"");
}
