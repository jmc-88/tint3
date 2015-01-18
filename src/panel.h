/**************************************************************************
* Copyright (C) 2008 Pål Staurland (staura@gmail.com)
* Modified (C) 2008/2009 thierry lorthiois (lorthiois@bbsoft.fr)
*
* panel :
* - draw panel and all objects according to panel_layout
*
*
**************************************************************************/

#ifndef PANEL_H
#define PANEL_H

#include <pango/pangocairo.h>
#include <sys/time.h>
#include <X11/Xlib.h>

#include <string>
#include <vector>

#include "clock/clock.h"
#include "launcher/launcher.h"
#include "util/area.h"
#include "util/common.h"
#include "systray/systraybar.h"
#include "taskbar/task.h"
#include "taskbar/taskbar.h"

#ifdef ENABLE_BATTERY
#include "battery/battery.h"
#endif

extern int signal_pending;
// --------------------------------------------------
// mouse events
extern MouseAction mouse_middle;
extern MouseAction mouse_right;
extern MouseAction mouse_scroll_up;
extern MouseAction mouse_scroll_down;
extern MouseAction mouse_tilt_left;
extern MouseAction mouse_tilt_right;

extern int wm_menu;
extern int panel_dock;

// panel mode
enum class PanelMode { kSingleDesktop, kMultiDesktop };

extern PanelMode panel_mode;

// panel layer
enum class PanelLayer { kBottom, kNormal, kTop };

extern PanelLayer panel_layer;

// panel position
enum PanelPosition {
  kLeft = 0x01,
  kRight = 0x02,
  kCenter = 0x04,
  kTop = 0x08,
  kBottom = 0x10
};

extern int panel_position;
extern bool panel_horizontal;
extern bool panel_refresh;
extern bool task_dragged;

// panel autohide
extern bool panel_autohide;
extern int panel_autohide_show_timeout;
extern int panel_autohide_hide_timeout;
extern int
    panel_autohide_height;  // for vertical panels this is of course the width
extern std::string panel_items_order;

// panel strut policy
enum class PanelStrutPolicy { kMinimum, kFollowSize, kNone };

extern PanelStrutPolicy panel_strut_policy;

extern int max_tick_urgent;

extern std::vector<Background*> backgrounds;

extern Imlib_Image default_icon;

// tint3 use one panel per monitor and one taskbar per desktop.
class Panel : public Area {
 public:
  // --------------------------------------------------
  // panel
  Window main_win_;
  Pixmap temp_pmap;

  // position relative to root window
  int root_x_, root_y_;
  int margin_x_, margin_y_;
  int percent_x, percent_y;
  // location of the panel (monitor number)
  int monitor_;

  // --------------------------------------------------
  // task and taskbar parameter per panel
  Global_taskbar g_taskbar;
  Global_task g_task;

  // --------------------------------------------------
  // taskbar point to the first taskbar in panel.area.list.
  // number of tasbar == nb_desktop. taskbar[i] is for desktop(i).
  // taskbar[i] is used to loop over taskbar,
  // while panel->area.list is used to loop over all panel's objects
  Taskbar* taskbar_;
  int nb_desktop_;

  // --------------------------------------------------
  // clock
  Clock clock_;

// --------------------------------------------------
// battery
#ifdef ENABLE_BATTERY
  Battery battery_;
#endif

  Launcher launcher_;

  // autohide
  int is_hidden_;
  int hidden_width_, hidden_height_;
  Pixmap hidden_pixmap_;
  Timeout* autohide_timeout_;

  Taskbar* ClickTaskbar(int x, int y);
  Task* ClickTask(int x, int y);
  bool ClickLauncher(int x, int y);
  LauncherIcon* ClickLauncherIcon(int x, int y);
  bool ClickPadding(int x, int y);
  bool ClickClock(int x, int y);
  Area* ClickArea(int x, int y);
  bool HandlesClick(XButtonEvent* e);

  void Render();
  bool Resize() override;

  // TODO: this should be private
  void InitSizeAndPosition();

  // draw background panel
  void SetBackground();

  void SetItemsOrder();
  void SetProperties();

  // show/hide taskbar according to current desktop
  void UpdateTaskbarVisibility();
};

extern Panel panel_config;
extern Panel* panel1;
extern int nb_panel;

// default global data
void DefaultPanel();

// freed memory
void CleanupPanel();

// realloc panels according to number of monitor
// use panel_config as default value
void InitPanel();

// detect wich panel
Panel* GetPanel(Window win);

void AutohideShow(void* p);
void AutohideHide(void* p);
void AutohideTriggerShow(Panel* p);
void AutohideTriggerHide(Panel* p);

#endif
