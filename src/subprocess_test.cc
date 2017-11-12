#include "catch.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "subprocess.hh"

TEST_CASE("make_subprocess") {
  auto exit_status = [](pid_t child_pid) {
    int status = 0;
    if (waitpid(child_pid, &status, 0) == -1) {
      FAIL("waitpid(): " << std::strerror(errno));
    }
    if (!WIFEXITED(status)) {
      FAIL("not WIFEXITED()");
    }
    return WEXITSTATUS(status);
  };

  SECTION("empty command") {
    auto sp = make_subprocess("");
    pid_t child_pid = sp.start();
    REQUIRE(child_pid < 0);
  }

  SECTION("execution failure") {
    auto sp = make_subprocess("bogus command");
    pid_t child_pid = sp.start();
    REQUIRE(child_pid != -1);
    REQUIRE(exit_status(child_pid) != 0);
  }

  SECTION("default") {
    auto sp = make_subprocess("/bin/true");
    pid_t child_pid = sp.start();
    REQUIRE(child_pid != -1);
    REQUIRE(exit_status(child_pid) == 0);
  }

  SECTION("callback") {
    auto sp = make_subprocess("/bin/true", child_callback{[] { _exit(123); }});
    pid_t child_pid = sp.start();
    REQUIRE(child_pid != -1);
    REQUIRE(exit_status(child_pid) == 123);
  }

  SECTION("shell") {
    auto sp = make_subprocess("[ true ] && echo \"Hooray! Todd episode!\"",
                              shell{true});
    pid_t child_pid = sp.start();
    REQUIRE(child_pid != -1);
    REQUIRE(exit_status(child_pid) == 0);
  }

  SECTION("multiple options") {
    auto sp = make_subprocess("/bin/true", shell{false}, session_leader{true});
    pid_t child_pid = sp.start();
    REQUIRE(child_pid != -1);
    REQUIRE(exit_status(child_pid) == 0);
  }
}

TEST_CASE("ShellExec") {
  SECTION("empty command") {
    // doesn't make sense, should be refused
    REQUIRE(ShellExec("") < 0);
  }

  SECTION("bogus command") {
    // won't really do anything, but is a valid input, should fork() and exec*()
    REQUIRE(ShellExec("there is no such command") > 0);
  }
}
