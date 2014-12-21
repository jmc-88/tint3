#ifndef TASKBARBASE_H
#define TASKBARBASE_H

#include <cstdlib>

#include "util/area.h"

enum TaskbarState {
    TASKBAR_NORMAL,
    TASKBAR_ACTIVE,
    TASKBAR_STATE_COUNT
};

class TaskbarBase : public Area {
    Pixmap state_pixmap_[TASKBAR_STATE_COUNT];

  public:
    Pixmap state_pixmap(size_t i) const;
    TaskbarBase& set_state_pixmap(size_t i, Pixmap value);
    TaskbarBase& reset_state_pixmap(size_t i);
};

#endif // TASKBARBASE_H
