#ifndef TINT3_PANEL_HH
#define TINT3_PANEL_HH

#include <X11/Xlib.h>
#include <sys/time.h>

#include <string>
#include <vector>

#include "clock/clock.hh"
#include "execp/execp.hh"
#include "launcher/launcher.hh"
#include "systray/systraybar.hh"
#include "taskbar/task.hh"
#include "taskbar/taskbar.hh"
#include "util/area.hh"
#include "util/color.hh"
#include "util/common.hh"
#include "util/gradient.hh"
#include "util/imlib2.hh"
#include "util/timer.hh"

#ifdef ENABLE_BATTERY
#include "battery/battery.hh"
#endif

// --------------------------------------------------
// mouse events
extern MouseAction mouse_middle;
extern MouseAction mouse_right;
extern MouseAction mouse_scroll_up;
extern MouseAction mouse_scroll_down;
extern MouseAction mouse_tilt_left;
extern MouseAction mouse_tilt_right;

// panel layer
enum class PanelLayer { kBottom, kNormal, kTop };

// panel position
enum class PanelVerticalPosition { kTop, kBottom, kCenter };
enum class PanelHorizontalPosition { kLeft, kRight, kCenter };

// taskbar mode
enum class TaskbarMode { kSingleDesktop, kMultiDesktop };

extern bool panel_refresh;
extern bool task_dragged;

// panel strut policy
enum class PanelStrutPolicy { kMinimum, kFollowSize, kNone };

extern int max_tick_urgent;

extern std::vector<Background> backgrounds;
extern std::vector<Executor> executors;
extern std::vector<util::Gradient> gradients;

extern util::imlib2::Image default_icon;

class PanelConfig {
 public:
  PanelConfig() = default;

  std::string items_order;

  Background background;
#ifdef ENABLE_BATTERY
  Battery battery;
#endif  // ENABLE_BATTERY

  PanelLayer layer;
  TaskbarMode taskbar_mode;

  unsigned int monitor;

  int margin_x;
  int margin_y;

  bool mouse_effects;
  int mouse_hover_alpha;
  int mouse_hover_saturation;
  int mouse_hover_brightness;
  int mouse_pressed_alpha;
  int mouse_pressed_saturation;
  int mouse_pressed_brightness;

  int padding_x_lr;
  int padding_x;
  int padding_y;

  unsigned int width;
  bool percent_x;
  unsigned int height;
  bool percent_y;

  bool autohide;
  int autohide_show_timeout;
  int autohide_hide_timeout;
  int autohide_size_px;

  PanelStrutPolicy strut_policy;
  PanelHorizontalPosition horizontal_position;
  PanelVerticalPosition vertical_position;

  bool dock;
  bool horizontal;
  bool wm_menu;

  static PanelConfig Default();
};

extern PanelConfig new_panel_config;

// tint3 use one panel per monitor and one taskbar per desktop.
class Panel : public Area {
 public:
  static constexpr unsigned int kAllMonitors = static_cast<unsigned int>(-1);

  // --------------------------------------------------
  // panel
  Window main_win_;
  Pixmap temp_pmap;

  // position relative to root window
  int root_x_, root_y_;

  // --------------------------------------------------
  // task and taskbar parameter per panel
  Global_taskbar g_taskbar;
  Global_task g_task;

  std::vector<Taskbar> taskbars;
  unsigned int num_desktops_;

  Launcher launcher_;

  // autohide
  int hidden_width_, hidden_height_;
  Pixmap hidden_pixmap_;
  Interval::Id autohide_timeout_;

  Battery* battery();
  Clock* clock();

  Taskbar* ClickTaskbar(int x, int y);
  Task* ClickTask(int x, int y);
  bool ClickLauncher(int x, int y);
  LauncherIcon* ClickLauncherIcon(int x, int y);
  bool ClickPadding(int x, int y);
  bool HandlesClick(XEvent* event) override;

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

  bool hidden() const;
  bool autohide() const;
  bool AutohideShow();
  void AutohideTriggerShow(Timer& timer);
  void AutohideTriggerHide(Timer& timer);
  bool AutohideHide();

  PanelLayer layer() const;
  TaskbarMode taskbar_mode() const;
  PanelHorizontalPosition horizontal_position() const;
  PanelVerticalPosition vertical_position() const;
  Monitor const& monitor() const;
  bool horizontal() const;
  bool window_manager_menu() const;
  void UseConfig(PanelConfig const& cfg, unsigned int num_desktop);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG

 private:
  PanelConfig config_;

  bool hidden_;
#ifdef ENABLE_BATTERY
  Battery battery_;
#endif  // ENABLE_BATTERY
  Clock clock_;

  void UpdateNetWMStrut();
};

extern Panel panel_config;
extern std::vector<Panel> panels;

// default global data
void DefaultPanel();

// free memory
void CleanupPanel();

// realloc panels according to number of monitor
// use panel_config as default value
void InitPanel(Timer& timer);

// detect wich panel
Panel* GetPanel(Window win);

#endif  // TINT3_PANEL_HH
