#ifndef TINT3_TASKBAR_TASKBARBASE_HH
#define TINT3_TASKBAR_TASKBARBASE_HH

#include <cstdlib>

#include "util/area.hh"
#include "util/x11.hh"

enum TaskbarState { kTaskbarNormal, kTaskbarActive, kTaskbarCount };

class TaskbarBase : public Area {
 public:
  util::x11::Pixmap state_pixmap(size_t i) const;
  TaskbarBase& set_state_pixmap(size_t i, util::x11::Pixmap const& value);
  TaskbarBase& reset_state_pixmap(size_t i);

 private:
  util::x11::Pixmap state_pixmap_[kTaskbarCount];
};

#endif  // TINT3_TASKBAR_TASKBARBASE_HH
