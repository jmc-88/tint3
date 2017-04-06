#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <sstream>
#include <string>
#include <utility>

#include "util/environment.hh"
#include "util/fs.hh"

TEST_CASE("Path", "Represents a filesystem path string") {
  SECTION("move constructor") {
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif  // __clang__
    auto new_path = []() -> util::fs::Path { return "/move"; };
    util::fs::Path moved_path{std::move(new_path())};
    REQUIRE(moved_path == "/move");
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__
  }

  SECTION("assignment") {
    util::fs::Path test_path{"/"};
    // Assignment from another Path object.
    util::fs::Path other_path{"/path/to/somewhere"};
    test_path = other_path;
    REQUIRE(test_path == "/path/to/somewhere");
    // Assignment from a string.
    test_path = "/path/from/string";
    REQUIRE(test_path == "/path/from/string");
  }

  SECTION("copy and append") {
    util::fs::Path root{"/"};
    util::fs::Path usr{root / "usr"};
    REQUIRE(root == "/");    // root was not modified
    REQUIRE(usr == "/usr");  // usr holds "/usr"
  }

  SECTION("append in place") {
    util::fs::Path root{"/"};
    util::fs::Path usr{root /= "usr"};
    REQUIRE(root == "/usr");  // root was modified
    REQUIRE(usr == root);     // usr now holds the same value as "root"
  }

  SECTION("strip trailing and leading slashes") {
    REQUIRE(util::fs::Path("/usr/share/") == "/usr/share");
    REQUIRE((util::fs::Path("/usr") / "share/") == "/usr/share");
    REQUIRE((util::fs::Path("/usr") / "/share/") == "/usr/share");
  }

  SECTION("output to a stream") {
    util::fs::Path test_path{"/test/path"};
    std::ostringstream oss;
    oss << test_path;
    REQUIRE(test_path == oss.str());
  }
}

TEST_CASE("BuildPath", "Joins the given pieces into a single path string") {
  REQUIRE(util::fs::BuildPath({"/"}) == "/");
  REQUIRE(util::fs::BuildPath({"/", "home", "testusr"}) == "/home/testusr");
  REQUIRE(util::fs::BuildPath({"/", "/home", "/testusr"}) == "/home/testusr");
}

TEST_CASE("DirectoryExists", "Checks for the existence of a directory") {
  // Obviously existing path.
  REQUIRE(util::fs::DirectoryExists("/"));
  // Obviously non-existing path.
  REQUIRE(!util::fs::DirectoryExists("/road/to/nowhere"));
}

TEST_CASE("FileExists", "Checks for the existence of a file") {
  // Obviously existing path.
  REQUIRE(util::fs::FileExists("src/util/testdata/fs_test.txt"));
  REQUIRE(util::fs::FileExists({"src", "util", "testdata", "fs_test.txt"}));
  // Obviously non-existing path.
  REQUIRE(!util::fs::FileExists("/dev/troll"));
}

TEST_CASE("HomeDirectory", "Returns the user's home path") {
  // Check that it honors the value of $HOME by default.
  std::string home_from_environment = environment::Get("HOME");
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);

  // When $HOME is empty or missing, getpwuid() should give us the same info.
  environment::ScopedOverride unset_home{"HOME", ""};
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);
}

class ReadFileCallback {
 public:
  ReadFileCallback() : invoked_(false) {}
  ReadFileCallback(ReadFileCallback const&) = delete;

  bool invoked() { return invoked_; }

  bool operator()(std::string const& /*contents*/) {
    invoked_ = true;
    return true;
  }

 private:
  bool invoked_;
};

TEST_CASE("ReadFile", "Reads the entire contents of a file into memory") {
  // Bad path case.
  ReadFileCallback read_file_callback_bad;
  REQUIRE(!util::fs::ReadFile("/none", std::ref(read_file_callback_bad)));
  REQUIRE(!read_file_callback_bad.invoked());

  // Good path case.
  ReadFileCallback read_file_callback_good;
  REQUIRE(util::fs::ReadFile("src/util/testdata/fs_test.txt",
                             std::ref(read_file_callback_good)));
  REQUIRE(read_file_callback_good.invoked());
}

class ReadFileByLineCallback : public ReadFileCallback {
 public:
  void set_expected_line(std::string const& expected_line) {
    expected_line_ = expected_line;
  }

  bool found() { return found_; }

  bool operator()(std::string const& line) {
    if (line == expected_line_) {
      found_ = true;
    }
    return ReadFileCallback::operator()(line);
  }

 private:
  bool found_;
  std::string expected_line_;
};

TEST_CASE("ReadFileByLine", "Reads the contents of a file line by line") {
  // Bad path case.
  ReadFileCallback read_file_callback_bad;
  REQUIRE(!util::fs::ReadFileByLine("/none", std::ref(read_file_callback_bad)));
  REQUIRE(!read_file_callback_bad.invoked());

  // Good path case.
  ReadFileByLineCallback read_file_callback_good;
  read_file_callback_good.set_expected_line("Pid:	24440");
  REQUIRE(util::fs::ReadFileByLine("src/util/testdata/fs_test.txt",
                                   std::ref(read_file_callback_good)));
  REQUIRE(read_file_callback_good.invoked());
  REQUIRE(read_file_callback_good.found());
}
