/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
*
* taskbar
*
**************************************************************************/

#ifndef TASKBAR_H
#define TASKBAR_H

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

    Taskbar& set_state(size_t state);
    void DrawForeground(cairo_t*) override;
    void OnChangeLayout() override;
    bool Resize() override;

    bool RemoveChild(Area* child) override;

    static void InitPanel(Panel* panel);
};

class Global_taskbar : public TaskbarBase {
  public:
    Taskbarname bar_name_;
    Background* background[TASKBAR_STATE_COUNT];
    Background* background_name[TASKBAR_STATE_COUNT];
};


// default global data
void DefaultTaskbar();

// freed memory
void CleanupTaskbar();

void InitTaskbar();

void TaskbarRemoveTask(Window win);
Task* TaskGetTask(Window win);
TaskPtrArray TaskGetTasks(Window win);
void TaskRefreshTasklist();


#endif

