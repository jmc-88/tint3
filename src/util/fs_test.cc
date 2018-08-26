#include "catch.hpp"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <functional>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/match.h"

#include "behavior_control.hh"
#include "util/environment.hh"
#include "util/fs.hh"
#include "util/fs_test_utils.hh"

TEST_CASE("Path", "Represents a filesystem path string") {
  SECTION("move constructor") {
    auto new_path = []() -> util::fs::Path { return "/move"; };
    FORCE_STD_MOVE(util::fs::Path moved_path{std::move(new_path())});
    REQUIRE(moved_path == "/move");
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

  SECTION("get parent directory path") {
    util::fs::Path relative{"test"};
    REQUIRE(relative.DirectoryName() == ".");

    util::fs::Path is_root{"/"};
    REQUIRE(is_root.DirectoryName() == "/");

    util::fs::Path has_parent{"/path/to/somewhere"};
    REQUIRE(has_parent.DirectoryName() == "/path/to");
  }

  SECTION("base name") {
    util::fs::Path relative{"test"};
    REQUIRE(relative.BaseName() == "test");

    util::fs::Path is_root{"/"};
    REQUIRE(is_root.BaseName() == "/");

    util::fs::Path has_parent{"/path/to/somewhere"};
    REQUIRE(has_parent.BaseName() == "somewhere");
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
  REQUIRE_FALSE(util::fs::DirectoryExists("/road/to/nowhere"));
}

TEST_CASE("FileExists", "Checks for the existence of a file") {
  // Obviously existing path.
  REQUIRE(util::fs::FileExists("src/util/testdata/fs_test.txt"));
  REQUIRE(util::fs::FileExists({"src", "util", "testdata", "fs_test.txt"}));
  // Obviously non-existing path.
  REQUIRE_FALSE(util::fs::FileExists("/dev/troll"));
}

TEST_CASE("HomeDirectory", "Returns the user's home path") {
  // Check that it honors the value of $HOME by default.
  std::string home_from_environment = environment::Get("HOME");
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);

  // When $HOME is empty or missing, getpwuid() should give us the same info.
  auto unset_home = environment::MakeScopedOverride("HOME", "");
  REQUIRE(util::fs::HomeDirectory() == home_from_environment);
}

TEST_CASE("WriteFile") {
  char* c_temp_path = tempnam("src/util/testdata", "fs_test_write_file");
  if (!c_temp_path)
    FAIL("couln't generate a temporary file name for this test case");
  std::string temp_path = c_temp_path;
  std::free(c_temp_path);

  static const std::string test_content =
      "Hello! I'm just testing WriteFile.\n";
  REQUIRE(util::fs::WriteFile(temp_path, test_content));

  std::string actual_content;
  REQUIRE(util::fs::ReadFile(temp_path, &actual_content));
  REQUIRE(actual_content == test_content);

  unlink(temp_path.c_str());
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
  REQUIRE_FALSE(util::fs::ReadFile("/none", std::ref(read_file_callback_bad)));
  REQUIRE_FALSE(read_file_callback_bad.invoked());

  // Good path case.
  ReadFileCallback read_file_callback_good;
  REQUIRE(util::fs::ReadFile("src/util/testdata/fs_test.txt",
                             std::ref(read_file_callback_good)));
  REQUIRE(read_file_callback_good.invoked());

  // String output case.
  std::string contents;
  REQUIRE(util::fs::ReadFile("src/util/testdata/fs_test.txt", &contents));
  REQUIRE(absl::StartsWith(contents, "Name:\tcat\n"));
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
  REQUIRE_FALSE(
      util::fs::ReadFileByLine("/none", std::ref(read_file_callback_bad)));
  REQUIRE_FALSE(read_file_callback_bad.invoked());

  // Good path case.
  ReadFileByLineCallback read_file_callback_good;
  read_file_callback_good.set_expected_line("Pid:	24440");
  REQUIRE(util::fs::ReadFileByLine("src/util/testdata/fs_test.txt",
                                   std::ref(read_file_callback_good)));
  REQUIRE(read_file_callback_good.invoked());
  REQUIRE(read_file_callback_good.found());
}

TEST_CASE("IsSymbolicLink") {
  FakeFileSystemInterface fake_fs;
  auto original_interface = util::fs::SetSystemInterface(&fake_fs);

  struct stat regular_file;
  regular_file.st_mode = S_IFREG;
  fake_fs.stat_responses["regular_file"].emplace_back(regular_file);
  REQUIRE_FALSE(util::fs::IsSymbolicLink("regular_file"));

  struct stat symlink;
  symlink.st_mode = S_IFLNK;
  fake_fs.stat_responses["symbolic_link"].emplace_back(symlink);
  REQUIRE(util::fs::IsSymbolicLink("symbolic_link"));

  auto fake_interface = util::fs::SetSystemInterface(original_interface);
}

TEST_CASE("SetSystemInterface") {
  // First, the file should be reported as non existing, as expected.
  REQUIRE_FALSE(util::fs::FileExists("./bogus_path"));

  // Then, we override the system interface with a fake one, and test that the
  // same file is now reported as existing.
  struct stat regular_file;
  regular_file.st_mode = S_IFREG;

  FakeFileSystemInterface fake_fs;
  fake_fs.stat_responses = {{"./bogus_path", {regular_file}}};
  auto original_interface = util::fs::SetSystemInterface(&fake_fs);
  REQUIRE(util::fs::FileExists("./bogus_path"));

  // Finally, we set the system interface back to the original one, and verify
  // that the file is reported again as non existing, and that the system
  // interface we just replaced was indeed the fake one.
  auto fake_interface = util::fs::SetSystemInterface(original_interface);
  REQUIRE_FALSE(util::fs::FileExists("./bogus_path"));
  REQUIRE(fake_interface == &fake_fs);
}
