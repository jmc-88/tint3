#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <initializer_list>
#include <string>

#include "config.h"
#include "server.h"

using namespace config;

TEST_CASE("ExtractValues",
          "Parsing a line from the configuration file should work") {
  std::string v1, v2, v3;

  SECTION("One value specified") {
    ExtractValues("first_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2.empty());
    REQUIRE(v3.empty());
  }

  SECTION("Two values specified") {
    ExtractValues("first_value second_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3.empty());
  }

  SECTION("Three values specified") {
    ExtractValues("first_value second_value third_value", v1, v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value");
  }

  SECTION("More than three values specified") {
    ExtractValues("first_value second_value third_value and all the rest", v1,
                  v2, v3);
    REQUIRE(v1 == "first_value");
    REQUIRE(v2 == "second_value");
    REQUIRE(v3 == "third_value and all the rest");
  }
}

namespace test {

class MockServer : public Server {
 public:
  void SetMonitors(std::initializer_list<std::string> const& monitor_list) {
    nb_monitor = monitor_list.size();

    monitor.clear();
    for (auto const& name : monitor_list) {
      Monitor m;
      m.names.push_back(name);
      monitor.push_back(m);
    }
  }
};

class ConfigReader : public config::Reader {
 public:
  ConfigReader() : config::Reader(&server_) {}

  MockServer& server() { return server_; }

  bool ParseLine(std::string const& line, std::string& key,
                 std::string& value) const {
    return config::Reader::ParseLine(line, key, value);
  }

  int GetMonitor(std::string const& monitor_name) const {
    return config::Reader::GetMonitor(monitor_name);
  }

 private:
  MockServer server_;
};

}  // namespace test

TEST_CASE("ParseLine", "Config lines are parsed as expected") {
  test::ConfigReader reader;
  std::string key, value;

  REQUIRE(!reader.ParseLine("# Comment, nothing to parse", key, value));
  REQUIRE(!reader.ParseLine("", key, value));  // empty string, nothing to parse
  REQUIRE(!reader.ParseLine("malformed, parsing fails", key, value));

  REQUIRE(reader.ParseLine("empty_value=", key, value));
  REQUIRE(key == "empty_value");
  REQUIRE(value.empty());

  REQUIRE(reader.ParseLine("key=value", key, value));
  REQUIRE(key == "key");
  REQUIRE(value == "value");

  REQUIRE(reader.ParseLine("spaces   =    allowed", key, value));
  REQUIRE(key == "spaces");
  REQUIRE(value == "allowed");
}

TEST_CASE("GetMonitor", "Accessing named monitors works as expected") {
  test::ConfigReader reader;
  reader.server().SetMonitors({"0", "1", "2"});
  REQUIRE(reader.GetMonitor("0") == 0);
  REQUIRE(reader.GetMonitor("1") == 1);
  REQUIRE(reader.GetMonitor("2") == 2);
  REQUIRE(reader.GetMonitor("all") == -1);
  REQUIRE(reader.GetMonitor("3") == -1);
}
