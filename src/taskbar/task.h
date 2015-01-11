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

#include "util/common.h"
#include "util/timer.h"
#include "util/area.h"

enum TaskState {
  kTaskNormal,
  kTaskActive,
  kTaskIconified,
  kTaskUrgent,
  kTaskStateCount
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
  int alpha[kTaskStateCount];
  int saturation[kTaskStateCount];
  int brightness[kTaskStateCount];
  int config_asb_mask;
  Background* background[kTaskStateCount];
  int config_background_mask;
  // starting position for text ~ task_padding + task_border + icon_size
  double text_posx, text_height;

  int font_shadow;
  PangoFontDescription* font_desc;
  Color font[kTaskStateCount];
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
  Imlib_Image icon[kTaskStateCount];
  Pixmap state_pix[kTaskStateCount];
  unsigned int icon_width;
  unsigned int icon_height;
  int urgent_tick;

  void DrawForeground(cairo_t* c) override;
  std::string GetTooltipText() override;
  bool UpdateTitle();  // TODO: find a more descriptive name
  std::string GetTitle() const;
  void SetTitle(std::string const& title);
  void OnChangeLayout() override;
  Task& SetTooltipEnabled(bool);

  void AddUrgent();
  void DelUrgent();
};

extern Timeout* urgent_timeout;
extern std::list<Task*> urgent_list;

Task* AddTask(Window win);
void RemoveTask(Task* tsk);

void GetIcon(Task* tsk);
void ActiveTask();
void SetTaskState(Task* tsk, int state);
void set_task_redraw(Task* tsk);

Task* FindActiveTask(Task* current_task, Task* active_task);
Task* NextTask(Task* tsk);
Task* PreviousTask(Task* tsk);

#endif
