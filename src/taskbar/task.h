/**************************************************************************
* task :
* -
*
**************************************************************************/

#ifndef TASK_H
#define TASK_H

#include <X11/Xlib.h>
#include <pango/pangocairo.h>
#include <Imlib2.h>

#include <list>

#include "common.h"
#include "timer.h"

enum {
    TASK_NORMAL,
    TASK_ACTIVE,
    TASK_ICONIFIED,
    TASK_URGENT,
    TASK_STATE_COUNT
};

// --------------------------------------------------
// global task parameter
class Global_task : public Area {
  public:
    int text;
    int icon;
    int centered;

    int icon_posy;
    int icon_size1;
    int maximum_width;
    int maximum_height;
    int alpha[TASK_STATE_COUNT];
    int saturation[TASK_STATE_COUNT];
    int brightness[TASK_STATE_COUNT];
    int config_asb_mask;
    Background* background[TASK_STATE_COUNT];
    int config_background_mask;
    // starting position for text ~ task_padding + task_border + icon_size
    double text_posx, text_height;

    int font_shadow;
    PangoFontDescription* font_desc;
    Color font[TASK_STATE_COUNT];
    int config_font_mask;
    int tooltip_enabled;
};

// TODO: make this inherit from a common base class that exposes state_pixmap
class Task : public Area {
    bool tooltip_enabled_;
    std::string title_;

    void DrawIcon(int);

  public:
    // TODO: group task with list of windows here
    Window win;
    int desktop;
    int current_state;
    Imlib_Image icon[TASK_STATE_COUNT];
    Pixmap state_pix[TASK_STATE_COUNT];
    unsigned int icon_width;
    unsigned int icon_height;
    int urgent_tick;

    void DrawForeground(cairo_t* c);
    std::string GetTooltipText();
    bool UpdateTitle(); // TODO: find a more descriptive name
    std::string GetTitle() const;
    void SetTitle(std::string const& title);
    void OnChangeLayout();
    Task& SetTooltipEnabled(bool);
};

extern Timeout* urgent_timeout;
extern std::list<Task*> urgent_list;

Task* AddTask(Window win);
void RemoveTask(Task* tsk);

void GetIcon(Task* tsk);
void ActiveTask();
void SetTaskState(Task* tsk, int state);
void set_task_redraw(Task* tsk);

Task* find_active_task(Task* current_task, Task* active_task);
Task* next_task(Task* tsk);
Task* prev_task(Task* tsk);

void add_urgent(Task* tsk);
void del_urgent(Task* tsk);

#endif

