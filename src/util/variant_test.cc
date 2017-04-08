#include "catch.hpp"

#include <string>
#include <type_traits>
#include <vector>

#include "util/variant.hh"

TEST_CASE("Store", "Can set and store variables") {
  util::variant::Value<int, std::string> v;

  float f = 1.23;
  v.Set<float>(f);
  REQUIRE(v.Get<float>() == f);

  int i = 123;
  v.Set<int>(i);
  REQUIRE(v.Get<int>() == i);

  REQUIRE_THROWS_AS(v.Get<bool>(), std::bad_cast);
}

TEST_CASE("Assign", "The assignment operator should work") {
  util::variant::Value<int, bool> v1, v2;

  v1.Set<int>(10);
  v2.Set<bool>(false);

  v1 = v2;
  REQUIRE(v1.Get<bool>() == v2.Get<bool>());
}

TEST_CASE("STL", "Works with STL types") {
  using Value = util::variant::Value<int, std::string>;
  std::vector<Value> var_vec;

  Value v1;
  v1.Set<int>(10);
  var_vec.push_back(v1);

  Value v2;
  v2.Set<std::string>("hello");
  var_vec.push_back(v2);

  REQUIRE(var_vec[0].Get<int>() == 10);
  REQUIRE(var_vec[1].Get<std::string>() == "hello");
}

TEST_CASE("Constructor", "The templated constructor should work") {
  // This needs to be explicitly constructed as an std::string before being
  // passed to the util::variant::Value constructor, as otherwise it'd be
  // interpreted as a const char* which is not the template parameters.
  util::variant::Value<int, bool, std::string> v{std::string{"hello"}};
  REQUIRE_THROWS_AS(v.Get<int>(), std::bad_cast);
  REQUIRE_THROWS_AS(v.Get<bool>(), std::bad_cast);
  REQUIRE(v.Get<std::string>() == "hello");
}
