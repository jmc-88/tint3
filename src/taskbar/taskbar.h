/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
*
* taskbar
*
**************************************************************************/

#ifndef TASKBAR_H
#define TASKBAR_H

#include "task.h"
#include "taskbarbase.h"
#include "taskbarname.h"

extern GHashTable* win_to_task_table;
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

void TaskbarRemoveTask(gpointer key, gpointer value, gpointer user_data);
Task* TaskGetTask(Window win);
GPtrArray* TaskGetTasks(Window win);
void TaskRefreshTasklist();


#endif

