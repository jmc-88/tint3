#include "startup_notification.hh"

#include <algorithm>
#include <iostream>

StartupNotification::StartupNotification(SnDisplay* SN_MAYBE_UNUSED(dpy),
                                         unsigned int SN_MAYBE_UNUSED(screen)) {
#ifdef HAVE_SN
  context_ = sn_launcher_context_new(dpy, screen);
#endif  // HAVE_SN
}

StartupNotification::StartupNotification(
    StartupNotification const& SN_MAYBE_UNUSED(other)) {
#ifdef HAVE_SN
  context_ = other.context_;
  if (context_ != nullptr) {
    sn_launcher_context_ref(context_);
  }
#endif  // HAVE_SN
}

StartupNotification::~StartupNotification() {
#ifdef HAVE_SN
  if (context_ != nullptr) {
    sn_launcher_context_unref(context_);
  }
#endif  // HAVE_SN
}

StartupNotification& StartupNotification::operator=(StartupNotification other) {
#ifdef HAVE_SN
  std::swap(context_, other.context_);
#endif  // HAVE_SN
  return (*this);
}

SnLauncherContext* StartupNotification::context() const {
#ifdef HAVE_SN
  return context_;
#else
  return nullptr;
#endif  // HAVE_SN
}

void StartupNotification::set_name(
    std::string const& SN_MAYBE_UNUSED(name)) const {
#ifdef HAVE_SN
  if (context_ != nullptr) {
    sn_launcher_context_set_name(context_, name.c_str());
  }
#endif  // HAVE_SN
}

void StartupNotification::set_description(
    std::string const& SN_MAYBE_UNUSED(description)) const {
#ifdef HAVE_SN
  if (context_ != nullptr) {
    sn_launcher_context_set_description(context_, description.c_str());
  }
#endif  // HAVE_SN
}

void StartupNotification::IncrementRef() const {
#ifdef HAVE_SN
  if (context_ != nullptr) {
    sn_launcher_context_ref(context_);
  }
#endif  // HAVE_SN
}

void StartupNotification::Initiate(
    std::string const& SN_MAYBE_UNUSED(binary_name),
    Time SN_MAYBE_UNUSED(time)) const {
#ifdef HAVE_SN
  if (context_) {
    sn_launcher_context_set_binary_name(context_, binary_name.c_str());
    sn_launcher_context_initiate(context_, "tint3", binary_name.c_str(), time);
  }
#endif  // HAVE_SN
}

void StartupNotification::SetupChildProcess() const {
#ifdef HAVE_SN
  if (context_) {
    sn_launcher_context_setup_child_process(context_);
  }
#endif  // HAVE_SN
}

void StartupNotification::Complete() const {
#ifdef HAVE_SN
  if (context_) {
    sn_launcher_context_complete(context_);
  }
#endif  // HAVE_SN
}
