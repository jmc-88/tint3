#ifndef TINT3_TASKBAR_TASKBAR_H
#define TINT3_TASKBAR_TASKBAR_H

#include <map>
#include <vector>

#include "task.h"
#include "taskbarbase.h"
#include "taskbarname.h"

using TaskPtrArray = std::vector<Task*>;
using WindowToTaskMap = std::map<Window, TaskPtrArray>;
extern WindowToTaskMap win_to_task_map;

extern Task* task_active;
extern Task* task_drag;
extern int taskbar_enabled;

// tint3 uses one taskbar per desktop.
class Taskbar : public TaskbarBase {
 public:
  int desktop;
  int text_width_;
  Taskbarname bar_name;

  Taskbar& SetState(size_t state);
  void DrawForeground(cairo_t*) override;
  void OnChangeLayout() override;
  bool Resize() override;

  bool RemoveChild(Area* child) override;

  static void InitPanel(Panel* panel);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG
};

class Global_taskbar : public TaskbarBase {
 public:
  Taskbarname bar_name_;
  Background* background[kTaskbarCount];
  Background* background_name[kTaskbarCount];
};

// default global data
void DefaultTaskbar();

// freed memory
void CleanupTaskbar(ChronoTimer& timer);

void InitTaskbar();

void TaskbarRemoveTask(Window win, ChronoTimer& timer);
Task* TaskGetTask(Window win);
TaskPtrArray TaskGetTasks(Window win);
void TaskRefreshTasklist(ChronoTimer& timer);

#endif  // TINT3_TASKBAR_TASKBAR_H
