#include "catch.hpp"

#include <X11/Xlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_SN
#include <libsn/sn.h>
#endif  // HAVE_SN

#include "absl/base/attributes.h"

#include "startup_notification.hh"
#include "util/common.hh"
#include "util/environment.hh"

namespace {

SnDisplay* GetDisplay() {
#ifdef HAVE_SN
  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    FAIL("Couldn't connect to the X server on DISPLAY="
         << environment::Get("DISPLAY"));
  }
  return sn_display_new(dpy, nullptr, nullptr);
#else
  return nullptr;
#endif  // HAVE_SN
}

void CloseDisplay(SnDisplay* SN_MAYBE_UNUSED(sn_dpy)) {
#ifdef HAVE_SN
  Display* dpy = sn_display_get_x_display(sn_dpy);
  sn_display_unref(sn_dpy);
  XCloseDisplay(dpy);
#endif  // HAVE_SN
}

}  // namespace

TEST_CASE("StartupNotification") {
  SnDisplay* sn_dpy = GetDisplay();
  ABSL_ATTRIBUTE_UNUSED auto sn_dpy_deleter =
      util::MakeScopedCallback([=] { CloseDisplay(sn_dpy); });

  StartupNotification sn{sn_dpy, 0};
  sn.set_name("test");
  sn.set_description("Simple startup notification for testing");

#ifdef HAVE_SN
  // not yet initiated
  REQUIRE_FALSE(sn_launcher_context_get_initiated(sn.context()));
#endif  // HAVE_SN
  sn.Initiate("startup_notification_test", CurrentTime);
#ifdef HAVE_SN
  // initiated!
  REQUIRE(sn_launcher_context_get_initiated(sn.context()));
#endif  // HAVE_SN

  pid_t pid = fork();
  if (pid < 0) {
    FAIL("fork() failed");
  }

  if (pid == 0) {  // child
    sn.IncrementRef();
    sn.SetupChildProcess();
    _exit(0);
  }

  // parent
  int status;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    FAIL("abnormal termination of child process");
  }
  sn.Complete();
}
