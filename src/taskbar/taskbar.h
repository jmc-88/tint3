/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
*
* taskbar
*
**************************************************************************/

#ifndef TASKBAR_H
#define TASKBAR_H

#include "task.h"
#include "taskbarname.h"

enum { TASKBAR_NORMAL, TASKBAR_ACTIVE, TASKBAR_STATE_COUNT };
extern GHashTable* win_to_task_table;
extern Task* task_active;
extern Task* task_drag;
extern int taskbar_enabled;

class TaskbarBase : public Area {
    Pixmap state_pixmap_[TASKBAR_STATE_COUNT];

  public:
    Pixmap state_pixmap(size_t i) const;
    TaskbarBase& set_state_pixmap(size_t i, Pixmap value);
    TaskbarBase& reset_state_pixmap(size_t i);
};

class Taskbarname : public TaskbarBase {
    std::string name_;

  public:
    std::string const& name() const;
    Taskbarname& set_name(std::string const& name);

    void DrawForeground(cairo_t*) override;
    bool Resize() override;
};

// tint3 uses one taskbar per desktop.
class Taskbar : public TaskbarBase {
  public:
    int desktop;
    int text_width;
    Taskbarname bar_name;

    Taskbar& set_state(size_t state);
    void DrawForeground(cairo_t*) override;
    void OnChangeLayout() override;
    bool Resize() override;
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
void InitTaskbarPanel(Panel* panel);

void TaskbarRemoveTask(gpointer key, gpointer value, gpointer user_data);
Task* TaskGetTask(Window win);
GPtrArray* TaskGetTasks(Window win);
void TaskRefreshTasklist();


#endif

