#include <algorithm>

#include "server.hh"
#include "taskbarbase.hh"

util::x11::Pixmap TaskbarBase::state_pixmap(size_t i) const {
  return state_pixmap_[i];
}

TaskbarBase& TaskbarBase::set_state_pixmap(size_t i,
                                           util::x11::Pixmap const& value) {
  state_pixmap_[i] = value;
  return (*this);
}

TaskbarBase& TaskbarBase::reset_state_pixmap(size_t i) {
  state_pixmap_[i] = {};
  return (*this);
}
