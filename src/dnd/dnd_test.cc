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
  constexpr char data[] = "one\ntwo\nthree";
  FakeStringProperty prop{data, sizeof(data) / sizeof(char)};
  REQUIRE(dnd::BuildCommand("test_command", prop) ==
          "test_command \"one\" \"two\" \"three\"");
}
