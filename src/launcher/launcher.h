/**************************************************************************
 * Copyright (C) 2010       (mrovi@interfete-web-club.com)
 *
 *
 **************************************************************************/

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <string>
#include <vector>

#include "common.h"
#include "area.h"
#include "xsettings-client.h"

class LauncherIcon : public Area {
  public:
    Imlib_Image icon_scaled;
    Imlib_Image icon_original;
    char* cmd;
    char* icon_name;
    char* icon_path;
    char* icon_tooltip;
    int icon_size;
    int is_app_desktop;
    int x, y;

    void DrawForeground(cairo_t*);
    std::string GetTooltipText();
    void OnChangeLayout();
};

struct DesktopEntry {
    char* name;
    char* exec;
    char* icon;
};

#define ICON_DIR_TYPE_SCALABLE 0
#define ICON_DIR_TYPE_FIXED 1
#define ICON_DIR_TYPE_THRESHOLD 2

struct IconThemeDir {
    char* name;
    int size;
    int type;
    int max_size;
    int min_size;
    int threshold;
    char* context;
};

struct IconTheme {
    std::string name;
    GSList* list_inherits; // each item is a char* (theme name)
    GSList* list_directories; // each item is an IconThemeDir*
};

class Launcher : public Area {
    std::string GetIconPath(std::string const& icon_name, int size);

  public:
    std::vector<char*> list_apps;          // paths to .desktop files
    std::vector<LauncherIcon*> list_icons;
    std::vector<IconTheme*> list_themes;

    void CleanupTheme();

    // Populates the list_themes list
    void LoadThemes();

    // Populates the list_icons list
    void LoadIcons();

    bool Resize();
};

extern bool launcher_enabled;
extern int launcher_max_icon_size;
extern int launcher_tooltip_enabled;
extern int launcher_alpha;
extern int launcher_saturation;
extern int launcher_brightness;
extern std::string icon_theme_name;   // theme name
extern XSettingsClient* xsettings_client;

// default global data
void DefaultLauncher();

// initialize launcher : y position, precision, ...
void InitLauncher();
void InitLauncherPanel(void* panel);
void CleanupLauncher();

void LauncherAction(LauncherIcon* icon, XEvent* e);

#endif
