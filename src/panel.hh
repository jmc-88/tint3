#ifndef TINT3_PANEL_HH
#define TINT3_PANEL_HH

#include <X11/Xlib.h>
#include <sys/time.h>

#include <functional>
#include <string>
#include <vector>

#include "clock/clock.hh"
#include "execp/execp.hh"
#include "launcher/launcher.hh"
#include "server.hh"
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
#endif  // ENABLE_BATTERY

enum class PanelLayer { kBottom, kNormal, kTop };
enum class PanelVerticalPosition { kTop, kBottom, kCenter };
enum class PanelHorizontalPosition { kLeft, kRight, kCenter };
enum class TaskbarMode { kSingleDesktop, kMultiDesktop };
enum class PanelStrutPolicy { kMinimum, kFollowSize, kNone };

struct MouseActionConfig {
  MouseAction middle = MouseAction::kNone;
  MouseAction right = MouseAction::kNone;
  MouseAction scroll_up = MouseAction::kNone;
  MouseAction scroll_down = MouseAction::kNone;
};

extern std::vector<Background> backgrounds;
extern std::vector<Executor> executors;
extern std::vector<util::Gradient> gradients;

extern bool panel_refresh;
extern bool task_dragged;
extern util::imlib2::Image default_icon;

using PanelSizer = std::function<unsigned int(unsigned int)>;
PanelSizer AbsoluteSize(unsigned int size);
PanelSizer PercentageSize(unsigned int size);

struct PanelConfig {
  PanelConfig();

  std::string items_order;

  Background background;
#ifdef ENABLE_BATTERY
  Battery battery;
#endif  // ENABLE_BATTERY

  PanelLayer layer = PanelLayer::kBottom;
  TaskbarMode taskbar_mode = TaskbarMode::kSingleDesktop;

  unsigned int monitor = 0;

  int margin_x = 0;
  int margin_y = 0;

  MouseActionConfig mouse_actions;
  bool mouse_effects = false;
  int mouse_hover_alpha = 100;
  int mouse_hover_saturation = 0;
  int mouse_hover_brightness = 10;
  int mouse_pressed_alpha = 100;
  int mouse_pressed_saturation = 0;
  int mouse_pressed_brightness = -10;

  int padding_x_lr = 0;
  int padding_x = 0;
  int padding_y = 0;

  PanelSizer width = PercentageSize(100);
  PanelSizer height = AbsoluteSize(40);

  bool autohide = false;
  int autohide_show_timeout = 0;
  int autohide_hide_timeout = 0;
  int autohide_size_px = 5;

  PanelStrutPolicy strut_policy = PanelStrutPolicy::kFollowSize;
  PanelHorizontalPosition horizontal_position =
      PanelHorizontalPosition::kCenter;
  PanelVerticalPosition vertical_position = PanelVerticalPosition::kBottom;

  bool dock = false;
  bool horizontal = true;
  bool wm_menu = false;
  int max_urgent_blinks = 14;
};

extern PanelConfig new_panel_config;

// tint3 use one panel per monitor and one taskbar per desktop.
class Panel : public Area {
 public:
  // For _NET_WM_DESKTOP: indicates the window should appear on all desktops.
  // See: https://specifications.freedesktop.org/wm-spec/wm-spec-latest.html
  static constexpr auto kAllMonitors = 0xFFFFFFFF;

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

#ifdef ENABLE_BATTERY
  Battery* battery();
#endif  // ENABLE_BATTERY

  Clock* clock();

  Taskbar* ClickTaskbar(int x, int y);
  Task* ClickTask(int x, int y);
  bool ClickLauncher(int x, int y);
  LauncherIcon* ClickLauncherIcon(int x, int y);
  bool ClickPadding(int x, int y);

  MouseAction FindMouseActionForEvent(XEvent* event);
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
  int max_urgent_blinks() const;
  void UseConfig(PanelConfig const& cfg, unsigned int num_desktop);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG

 private:
  PanelConfig config_;

  bool hidden_;
  Clock clock_;

#ifdef ENABLE_BATTERY
  Battery battery_;
#endif  // ENABLE_BATTERY

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
