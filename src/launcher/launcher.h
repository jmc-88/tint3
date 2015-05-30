#ifndef TINT3_LAUNCHER_LAUNCHER_H
#define TINT3_LAUNCHER_LAUNCHER_H

#include <string>
#include <vector>

#include "launcher/xsettings-client.h"
#include "util/area.h"
#include "util/common.h"
#include "util/imlib2.h"

class LauncherIcon : public Area {
 public:
  util::imlib2::Image icon_scaled_;
  util::imlib2::Image icon_original_;
  std::string cmd_;
  std::string icon_name_;
  std::string icon_path_;
  std::string icon_tooltip_;
  int icon_size_;
  int is_app_desktop_;
  int x_;
  int y_;

  void DrawForeground(cairo_t*) override;
  std::string GetTooltipText() override;
  void OnChangeLayout() override;

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG
};

struct DesktopEntry {
  std::string name;
  std::string exec;
  std::string icon;
};

#define ICON_DIR_TYPE_SCALABLE 0
#define ICON_DIR_TYPE_FIXED 1
#define ICON_DIR_TYPE_THRESHOLD 2

struct IconThemeDir {
  std::string name;
  std::string context;
  int size;
  int type;
  int max_size;
  int min_size;
  int threshold;
};

class IconTheme {
 public:
  ~IconTheme();

  std::string name;
  std::vector<std::string> list_inherits;
  std::vector<IconThemeDir*> list_directories;
};

class Launcher : public Area {
  std::string GetIconPath(std::string const& icon_name, int size);

 public:
  std::vector<std::string> list_apps_;  // paths to .desktop files
  std::vector<LauncherIcon*> list_icons_;
  std::vector<IconTheme*> list_themes_;

  void CleanupTheme();

  // Populates the list_themes list
  void LoadThemes();

  // Populates the list_icons list
  void LoadIcons();

  bool Resize() override;

  static void InitPanel(Panel* panel);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG
};

extern bool launcher_enabled;
extern int launcher_max_icon_size;
extern int launcher_tooltip_enabled;
extern int launcher_alpha;
extern int launcher_saturation;
extern int launcher_brightness;
extern std::string icon_theme_name;  // theme name
extern XSettingsClient* xsettings_client;

// default global data
void DefaultLauncher();

// initialize launcher : y position, precision, ...
void InitLauncher();
void CleanupLauncher();

void LauncherAction(LauncherIcon* launcher_icon, XEvent* evt);

#endif  // TINT3_LAUNCHER_LAUNCHER_H
