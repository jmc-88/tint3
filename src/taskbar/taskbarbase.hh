#ifndef TINT3_TASKBAR_TASKBARBASE_HH
#define TINT3_TASKBAR_TASKBARBASE_HH

#include <cstdlib>

#include "util/area.hh"

enum TaskbarState { kTaskbarNormal, kTaskbarActive, kTaskbarCount };

class TaskbarBase : public Area {
 public:
  TaskbarBase();

  Pixmap state_pixmap(size_t i) const;
  TaskbarBase& set_state_pixmap(size_t i, Pixmap value);
  TaskbarBase& reset_state_pixmap(size_t i);

 private:
  Pixmap state_pixmap_[kTaskbarCount];
};

#endif  // TINT3_TASKBAR_TASKBARBASE_HH
