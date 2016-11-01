#include <cstring>

#include "server.hh"
#include "taskbarbase.hh"

TaskbarBase::TaskbarBase() {
  std::memset(&state_pixmap_, 0, sizeof(state_pixmap_));
}

Pixmap TaskbarBase::state_pixmap(size_t i) const { return state_pixmap_[i]; }

TaskbarBase& TaskbarBase::set_state_pixmap(size_t i, Pixmap value) {
  state_pixmap_[i] = value;
  return (*this);
}

TaskbarBase& TaskbarBase::reset_state_pixmap(size_t i) {
  if (state_pixmap_[i] != 0) {
    XFreePixmap(server.dsp, state_pixmap_[i]);
  }

  state_pixmap_[i] = 0;
  return (*this);
}
