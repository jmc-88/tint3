#ifndef TINT3_STARTUP_NOTIFICATION_H
#define TINT3_STARTUP_NOTIFICATION_H

#ifdef HAVE_SN
#include <X11/Xlib.h>
#include <libsn/sn.h>
#define SN_MAYBE_UNUSED(x) x
#else
using Time = unsigned long;
using SnDisplay = void;
using SnLauncherContext = void;
#define SN_MAYBE_UNUSED(x)
#endif  // HAVE_SN

#include <string>

class StartupNotification {
 public:
  StartupNotification() = default;
  StartupNotification(StartupNotification const& SN_MAYBE_UNUSED(other));
  StartupNotification(StartupNotification&&) = default;
  StartupNotification(SnDisplay* SN_MAYBE_UNUSED(dpy),
                      unsigned int SN_MAYBE_UNUSED(screen));
  ~StartupNotification();

  StartupNotification& operator=(StartupNotification other);

  SnLauncherContext* context() const;

  void set_name(std::string const& SN_MAYBE_UNUSED(name)) const;
  void set_description(std::string const& SN_MAYBE_UNUSED(description)) const;

  void IncrementRef() const;
  void Initiate(std::string const& SN_MAYBE_UNUSED(binary_name),
                Time SN_MAYBE_UNUSED(time)) const;
  void SetupChildProcess() const;
  void Complete() const;

 private:
#ifdef HAVE_SN
  SnLauncherContext* context_ = nullptr;
#endif  // HAVE_SN
};

#endif  // TINT3_STARTUP_NOTIFICATION_H
