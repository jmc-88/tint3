#ifndef TINT3_LAUNCHER_LAUNCHER_HH
#define TINT3_LAUNCHER_LAUNCHER_HH

#include <xsettings-client.h>
#include <string>
#include <vector>

#include "util/area.hh"
#include "util/common.hh"
#include "util/imlib2.hh"

class LauncherIcon : public Area {
 public:
  util::imlib2::Image icon_original_;
  util::imlib2::Image icon_scaled_;
  util::imlib2::Image icon_hover_;
  util::imlib2::Image icon_pressed_;
  std::string cmd_;
  std::string icon_name_;
  std::string icon_path_;
  std::string icon_tooltip_;
  int icon_size_;
  int is_app_desktop_;
  int x_;
  int y_;

  LauncherIcon();

  void DrawForeground(cairo_t*) override;
  std::string GetTooltipText() override;
  void OnChangeLayout() override;
  bool OnClick(XEvent* event) override;

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

  int GetIconSize() const;

  void CleanupTheme();

  // Populates the list_themes list
  bool LoadThemes();

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
extern bool launcher_tooltip_enabled;
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

// Looks up for the given desktop entry in well known paths.
// The desktop entry can be a relative or absolute path to a file, or it can
// be simply the file name that will be resolved against standard XDG dirs.
bool FindDesktopEntry(std::string const& name, std::string* output_path);

#endif  // TINT3_LAUNCHER_LAUNCHER_HH
