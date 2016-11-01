/**************************************************************************
* Tint3 : launcher
*
* Copyright (C) 2010       (mrovi@interfete-web-club.com)
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <cairo-xlib.h>
#include <cairo.h>
#include <glib/gi18n.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_SN
#include <libsn/sn.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#include <memory>
#include <set>
#include <string>

#include "launcher.h"
#include "panel.h"
#include "server.h"
#include "taskbar.h"
#include "util/fs.h"
#include "util/log.h"

bool launcher_enabled = false;
int launcher_max_icon_size;
int launcher_tooltip_enabled;
int launcher_alpha;
int launcher_saturation;
int launcher_brightness;
std::string icon_theme_name;
XSettingsClient* xsettings_client;

namespace {

const char kIconFallback[] = "application-x-executable";

}  // namespace

bool LauncherReadDesktopFile(std::string const& path, DesktopEntry* entry);
util::imlib2::Image ScaleIcon(Imlib_Image original, int icon_size);
void FreeIconTheme(IconTheme* theme);

void DefaultLauncher() {
  launcher_enabled = false;
  launcher_max_icon_size = 0;
  launcher_tooltip_enabled = 0;
  launcher_alpha = 100;
  launcher_saturation = 0;
  launcher_brightness = 0;
  icon_theme_name.clear();
  xsettings_client = nullptr;
}

void InitLauncher() {
  if (launcher_enabled) {
    // if XSETTINGS manager running, tint3 read the icon_theme_name.
    xsettings_client = XSettingsClientNew(
        server.dsp, server.screen, XSettingsNotifyCallback, nullptr, nullptr);
  }
}

void Launcher::InitPanel(Panel* panel) {
  Launcher& launcher = panel->launcher_;

  launcher.parent_ = panel;
  launcher.panel_ = panel;
  launcher.size_mode_ = SizeMode::kByContent;
  launcher.need_resize_ = true;
  launcher.need_redraw_ = true;

  if (launcher.bg_ == nullptr) {
    launcher.bg_ = backgrounds.front();
  }

  // check consistency
  if (launcher.list_apps_.empty()) {
    return;
  }

  launcher.on_screen_ = true;
  panel_refresh = true;

  launcher.LoadThemes();
  launcher.LoadIcons();
}

void CleanupLauncher() {
  if (xsettings_client) {
    XSettingsClientDestroy(xsettings_client);
  }

  for (int i = 0; i < nb_panel; i++) {
    panel1[i].launcher_.CleanupTheme();
  }

  panel_config.launcher_.list_apps_.clear();
  icon_theme_name.clear();
  launcher_enabled = false;
}

void Launcher::CleanupTheme() {
  FreeArea();

  for (auto const& launcher_icon : list_icons_) {
    if (launcher_icon != nullptr) {
      delete launcher_icon;
    }
  }

  for (auto const& theme : list_themes_) {
    if (theme != nullptr) {
      delete theme;
    }
  }

  list_icons_.clear();
  list_themes_.clear();
}

bool Launcher::Resize() {
  int icons_per_column = 1, icons_per_row = 1, margin = 0;
  int icon_size;

  if (panel_horizontal) {
    icon_size = height_;
  } else {
    icon_size = width_;
  }

  icon_size -= (2 * bg_->border.width) - (2 * padding_y_);

  if (launcher_max_icon_size > 0 && icon_size > launcher_max_icon_size) {
    icon_size = launcher_max_icon_size;
  }

  // Resize icons if necessary
  for (auto& launcher_icon : list_icons_) {
    if (launcher_icon->icon_size_ != icon_size ||
        !launcher_icon->icon_original_) {
      launcher_icon->icon_size_ = icon_size;
      launcher_icon->width_ = launcher_icon->icon_size_;
      launcher_icon->height_ = launcher_icon->icon_size_;

      // Get the path for an icon file with the new size
      std::string new_icon_path =
          GetIconPath(launcher_icon->icon_name_, launcher_icon->icon_size_);

      if (new_icon_path.empty()) {
        // Draw a blank icon
        launcher_icon->icon_original_.Free();
        launcher_icon->icon_scaled_.Free();
        new_icon_path = GetIconPath(kIconFallback, launcher_icon->icon_size_);

        if (!new_icon_path.empty()) {
          launcher_icon->icon_original_ =
              imlib_load_image(new_icon_path.c_str());

          util::log::Error() << __FILE__ << ':' << __LINE__ << ": Using icon "
                             << new_icon_path.c_str() << '\n';
        }

        launcher_icon->icon_scaled_ =
            ScaleIcon(launcher_icon->icon_original_, icon_size);
        continue;
      }

      if (!launcher_icon->icon_path_.empty() &&
          new_icon_path == launcher_icon->icon_path_) {
        // If it's the same file just rescale
        launcher_icon->icon_scaled_ =
            ScaleIcon(launcher_icon->icon_original_, icon_size);

        util::log::Error() << __FILE__ << ':' << __LINE__ << ": Using icon "
                           << launcher_icon->icon_path_ << '\n';
      } else {
        // Load the new file and scale
        launcher_icon->icon_original_ = imlib_load_image(new_icon_path.c_str());
        launcher_icon->icon_scaled_ =
            ScaleIcon(launcher_icon->icon_original_, launcher_icon->icon_size_);
        launcher_icon->icon_path_ = new_icon_path;

        util::log::Error() << __FILE__ << ':' << __LINE__ << ": Using icon "
                           << launcher_icon->icon_path_ << '\n';
      }
    }
  }

  size_t count = list_icons_.size();

  if (panel_horizontal) {
    width_ = 0;

    if (count != 0) {
      int height = height_ - 2 * bg_->border.width - 2 * padding_y_;
      // here icons_per_column always higher than 0
      icons_per_column =
          std::max(1, (height + padding_x_) / (icon_size + padding_x_));
      margin = height - (icons_per_column - 1) * (icon_size + padding_x_) -
               icon_size;
      icons_per_row =
          count / icons_per_column + (count % icons_per_column != 0);
      width_ = (2 * bg_->border.width) + (2 * padding_x_lr_) +
               (icon_size * icons_per_row) + ((icons_per_row - 1) * padding_x_);
    }
  } else {
    height_ = 0;

    if (count != 0) {
      int width = width_ - 2 * bg_->border.width - 2 * padding_y_;
      // here icons_per_row always higher than 0
      icons_per_row =
          std::max(1, (width + padding_x_) / (icon_size + padding_x_));
      margin =
          width - (icons_per_row - 1) * (icon_size + padding_x_) - icon_size;
      icons_per_column = count / icons_per_row + (count % icons_per_row != 0);
      height_ = (2 * bg_->border.width) + (2 * padding_x_lr_) +
                (icon_size * icons_per_column) +
                ((icons_per_column - 1) * padding_x_);
    }
  }

  int posx, posy;
  int start = bg_->border.width + padding_y_ + margin / 2;

  if (panel_horizontal) {
    posy = start;
    posx = bg_->border.width + padding_x_lr_;
  } else {
    posx = start;
    posy = bg_->border.width + padding_x_lr_;
  }

  int i = 1;

  for (auto& launcher_icon : list_icons_) {
    launcher_icon->y_ = posy;
    launcher_icon->x_ = posx;

    if (panel_horizontal) {
      if (i % icons_per_column) {
        posy += (icon_size + padding_x_);
      } else {
        posy = start;
        posx += (icon_size + padding_x_);
      }
    } else {
      if (i % icons_per_row) {
        posx += (icon_size + padding_x_);
      } else {
        posx = start;
        posy += (icon_size + padding_x_);
      }
    }

    ++i;
  }

  return 1;
}

// Here we override the default layout of the icons; normally Area layouts its
// children
// in a stack; we need to layout them in a kind of table
void LauncherIcon::OnChangeLayout() {
  panel_y_ = (parent_->panel_y_ + y_);
  panel_x_ = (parent_->panel_x_ + x_);
}

std::string LauncherIcon::GetTooltipText() {
  return launcher_tooltip_enabled ? icon_tooltip_ : std::string();
}

void LauncherIcon::DrawForeground(cairo_t* c) {
  // Render
  imlib_context_set_image(icon_scaled_);

  if (server.real_transparency) {
    RenderImage(pix_, 0, 0, imlib_image_get_width(), imlib_image_get_height());
  } else {
    imlib_context_set_drawable(pix_);
    imlib_render_image_on_drawable(0, 0);
  }
}

#ifdef _TINT3_DEBUG

std::string LauncherIcon::GetFriendlyName() const { return "LauncherIcon"; }

#endif  // _TINT3_DEBUG

util::imlib2::Image ScaleIcon(Imlib_Image original, int icon_size) {
  Imlib_Image icon_scaled;

  if (original) {
    imlib_context_set_image(original);
    icon_scaled = imlib_create_cropped_scaled_image(
        0, 0, imlib_image_get_width(), imlib_image_get_height(), icon_size,
        icon_size);
    imlib_context_set_image(icon_scaled);
    imlib_image_set_has_alpha(1);
    DATA32* data = imlib_image_get_data();
    AdjustAsb(data, icon_size, icon_size, launcher_alpha,
              (float)launcher_saturation / 100,
              (float)launcher_brightness / 100);
    imlib_image_put_back_data(data);
  } else {
    icon_scaled = imlib_create_image(icon_size, icon_size);
    imlib_context_set_image(icon_scaled);
    imlib_context_set_color(255, 255, 255, 255);
    imlib_image_fill_rectangle(0, 0, icon_size, icon_size);
  }

  return icon_scaled;
}

void LauncherAction(LauncherIcon* launcher_icon, XEvent* evt) {
#if HAVE_SN
  auto ctx = sn_launcher_context_new(server.sn_dsp, server.screen);
  sn_launcher_context_set_name(ctx, launcher_icon->icon_tooltip_.c_str());
  sn_launcher_context_set_description(ctx, "Application launched from tint3");
  sn_launcher_context_set_binary_name(ctx, launcher_icon->cmd_.c_str());

  // Get a timestamp from the X event
  Time time;

  if (evt->type == ButtonPress || evt->type == ButtonRelease) {
    time = evt->xbutton.time;
  } else {
    util::log::Error() << "Unknown X event: " << evt->type << '\n';
    return;
  }

  sn_launcher_context_initiate(ctx, "tint3", launcher_icon->cmd_.c_str(), time);
#endif /* HAVE_SN */

  // TODO: make this use tint_exec...
  pid_t pid = fork();

  if (pid < 0) {
    util::log::Error() << "Could not fork\n";
  } else if (pid == 0) {
#if HAVE_SN
    sn_launcher_context_setup_child_process(ctx);
#endif  // HAVE_SN
    // Allow children to exist after parent destruction
    setsid();
    // Run the command
    execl("/bin/sh", "/bin/sh", "-c", launcher_icon->cmd_.c_str(), nullptr);

    util::log::Error() << "Failed to execlp " << launcher_icon->cmd_ << '\n';
#if HAVE_SN
    // TODO: how can this not leak? On a successful execl, this line is
    // never executed.
    sn_launcher_context_unref(ctx);
#endif  // HAVE_SN
    _exit(1);
  }
#if HAVE_SN
  else {
    server.pids[pid] = ctx;
  }

#endif  // HAVE_SN
}

/***************** Freedesktop app.desktop and icon theme handling
 * *********************/
/* http://standards.freedesktop.org/desktop-entry-spec/ */
/* http://standards.freedesktop.org/icon-theme-spec/ */

// Splits line at first '=' and returns 1 if successful, and parts are not empty
// key and value point to the parts
bool ParseDesktopLine(std::string const& line, std::string& key,
                      std::string& value) {
  bool found = false;

  for (auto it = line.cbegin(); it != line.cend(); ++it) {
    if (*it == '=') {
      key.assign(line.cbegin(), it);
      value.assign(it + 1, line.cend());
      found = true;
      break;
    }
  }

  return (found && !key.empty() && !value.empty());
}

bool ParseThemeLine(std::string const& line, std::string& key,
                    std::string& value) {
  return ParseDesktopLine(line, key, value);
}

void ExpandExec(DesktopEntry* entry, std::string const& path) {
  if (entry->exec.empty()) {
    return;
  }

  std::string expanded;

  // p will never point to an escaped char
  for (auto c = entry->exec.begin(); c != entry->exec.end(); ++c) {
    if (*c == '\\') {
      ++c;

      if (*c == '\0') {
        break;
      }

      // Copy the escaped char
      if (*c != '%') {
        expanded.push_back('\\');
      }

      expanded.push_back(*c);
    } else if (*c == '%') {
      ++c;

      if (*c == '\0') {
        break;
      }

      if (*c == 'i' && !entry->icon.empty()) {
        expanded.append(StringBuilder() << "--icon '" << entry->icon << '\'');
      } else if (*c == 'c' && !entry->name.empty()) {
        expanded.append(StringBuilder() << '\'' << entry->name << '\'');
      } else if (*c == 'f' || *c == 'F') {
        // Ignore the expansions in this case, we have no files to pass to the
        // executable.
        // TODO: this could not be true in cases of Drag and Drop.
      } else if (*c == 'u' || *c == 'U') {
        // Ignore the expansions in this case, we have no URLs to pass to the
        // executable.
        // TODO: this could not be true in cases of Drag and Drop.
      }
    } else {
      expanded.push_back(*c);
    }
  }

  entry->exec = expanded;
}

bool LauncherReadDesktopFile(const std::string& path, DesktopEntry* entry) {
  entry->name.clear();
  entry->icon.clear();
  entry->exec.clear();

  gchar** languages = (gchar**)g_get_language_names();
  int i;

  for (i = 0; languages[i]; ++i) {
  }

  // lang_index_default is a constant that encodes the Name key without a
  // language
  int lang_index_default = i;
  // lang_index is the index of the language for the best Name key in the
  // language vector
  // we currently do not know about any Name key at all, so use an invalid index
  int lang_index = lang_index_default + 1;

  bool inside_desktop_entry = false;
  bool read = util::fs::ReadFileByLine(path, [&](std::string const& data) {
    std::string line(data);
    StringTrim(line);

    if (line.empty()) {
      return;
    }

    if (line[0] == '[') {
      inside_desktop_entry = (line == "[Desktop Entry]");
    }

    std::string key, value;

    if (inside_desktop_entry && ParseDesktopLine(line, key, value)) {
      if (key.substr(0, 4) == "Name") {
        if (key == "Name" && lang_index > lang_index_default) {
          entry->name = value;
          lang_index = lang_index_default;
        } else {
          for (i = 0; languages[i] && i < lang_index; i++) {
            std::string localized_key;
            localized_key.append("Name[");
            localized_key.append(languages[i]);
            localized_key.append("]");

            if (key == localized_key) {
              entry->name = value;
              lang_index = i;
            }
          }
        }
      } else if (entry->exec.empty() && key == "Exec") {
        entry->exec = value;
      } else if (entry->icon.empty() && key == "Icon") {
        entry->icon = value;
      }
    }
  });

  if (!read) {
    util::log::Error() << "Could not open file " << path << '\n';
    return false;
  }

  // From this point:
  // entry->name, entry->icon, entry->exec will never be empty strings (can be
  // nullptr though)

  ExpandExec(entry, path);
  return true;
}

IconTheme::~IconTheme() {
  for (auto const& dir : list_directories) {
    delete dir;
  }
}

// TODO Use UTF8 when parsing the file
IconTheme* LoadTheme(std::string const& name) {
  // Look for name/index.theme in $HOME/.icons, /usr/share/icons,
  // /usr/share/pixmaps (stop at the first found)
  // Parse index.theme -> list of IconThemeDir with attributes
  // Return IconTheme*

  if (name.empty()) {
    return nullptr;
  }

  std::string file_name(util::fs::HomeDirectory() / ".icons" / name /
                        "index.theme");

  if (!util::fs::FileExists(file_name)) {
    file_name = util::fs::Path("/usr/share/icons") / name / "index.theme";

    if (!util::fs::FileExists(file_name)) {
      file_name = util::fs::Path("/usr/share/pixmaps") / name / "index.theme";

      if (!util::fs::FileExists(file_name)) {
        file_name.clear();
      }
    }
  }

  if (file_name.empty()) {
    return nullptr;
  }

  auto theme = new IconTheme();
  theme->name = name;
  theme->list_inherits.clear();
  theme->list_directories.clear();

  IconThemeDir* current_dir = nullptr;
  bool inside_header = true;

  bool read = util::fs::ReadFileByLine(file_name, [&](std::string const& data) {
    std::string line(data);
    StringTrim(line);

    if (line.empty()) {
      return;
    }

    std::string key, value;

    if (inside_header) {
      if (ParseThemeLine(line, key, value)) {
        if (key == "Inherits") {
          // value is like oxygen,wood,default
          // TODO: remove these strdup/strtok calls
          char* value_ptr = strdup(value.c_str());
          char* token = strtok(value_ptr, ",\n");

          while (token != nullptr) {
            theme->list_inherits.push_back(token);
            token = strtok(nullptr, ",\n");
          }

          std::free(value_ptr);
        } else if (key == "Directories") {
          // value is like
          // 48x48/apps,48x48/mimetypes,32x32/apps,scalable/apps,scalable/mimetypes
          // TODO: remove these strdup/strtok calls
          char* value_ptr = strdup(value.c_str());
          char* token = strtok(value_ptr, ",\n");

          while (token != nullptr) {
            auto dir = new IconThemeDir();
            dir->name = token;
            dir->max_size = dir->min_size = dir->size = -1;
            dir->type = ICON_DIR_TYPE_THRESHOLD;
            dir->threshold = 2;
            theme->list_directories.push_back(dir);
            token = strtok(nullptr, ",\n");
          }

          std::free(value_ptr);
        }
      }
    } else if (current_dir != nullptr) {
      if (ParseThemeLine(line, key, value)) {
        if (key == "Size") {
          // value is like 24
          current_dir->size = std::stoi(value);

          if (current_dir->max_size == -1) {
            current_dir->max_size = current_dir->size;
          }

          if (current_dir->min_size == -1) {
            current_dir->min_size = current_dir->size;
          }
        } else if (key == "MaxSize") {
          // value is like 24
          current_dir->max_size = std::stoi(value);
        } else if (key == "MinSize") {
          // value is like 24
          current_dir->min_size = std::stoi(value);
        } else if (key == "Threshold") {
          // value is like 2
          current_dir->threshold = std::stoi(value);
        } else if (key == "Type") {
          // value is Fixed, Scalable or Threshold : default to scalable for
          // unknown Type.
          if (value == "Fixed") {
            current_dir->type = ICON_DIR_TYPE_FIXED;
          } else if (value == "Threshold") {
            current_dir->type = ICON_DIR_TYPE_THRESHOLD;
          } else {
            current_dir->type = ICON_DIR_TYPE_SCALABLE;
          }
        } else if (key == "Context") {
          // usual values: Actions, Applications, Devices, FileSystems,
          // MimeTypes
          current_dir->context = value;
        }
      }
    }

    if (line[0] == '[' && line[line.length() - 1] == ']' &&
        line != "[Icon Theme]") {
      inside_header = false;
      current_dir = nullptr;
      std::string dir_name = line.substr(1, line.length() - 2);

      for (auto const& dir : theme->list_directories) {
        if (dir_name == dir->name) {
          current_dir = dir;
          break;
        }
      }
    }
  });

  if (!read) {
    delete theme;
    util::log::Error() << "Could not open theme \"" << file_name << "\"\n";
    return nullptr;
  }

  return theme;
}

// Populates the list_icons list
void Launcher::LoadIcons() {
  // Load apps (.desktop style launcher items)
  for (auto const& app : list_apps_) {
    DesktopEntry entry;
    LauncherReadDesktopFile(app, &entry);

    if (!entry.exec.empty()) {
      auto launcher_icon = new LauncherIcon();
      launcher_icon->parent_ = this;
      launcher_icon->panel_ = panel_;
      launcher_icon->size_mode_ = SizeMode::kByContent;
      launcher_icon->need_resize_ = false;
      launcher_icon->need_redraw_ = true;
      launcher_icon->bg_ = backgrounds.front();
      launcher_icon->on_screen_ = true;

      launcher_icon->is_app_desktop_ = 1;
      launcher_icon->cmd_ = entry.exec;
      launcher_icon->icon_name_ =
          !entry.icon.empty() ? entry.icon : kIconFallback;
      launcher_icon->icon_size_ = 1;
      launcher_icon->icon_tooltip_ =
          !entry.name.empty() ? entry.name : entry.exec;
      list_icons_.push_back(launcher_icon);

      AddChild(launcher_icon);
    }
  }
}

// Populates the list_themes list
void Launcher::LoadThemes() {
  // load the user theme, all the inherited themes recursively (DFS), and the
  // hicolor theme
  // avoid inheritance loops
  if (icon_theme_name.empty()) {
    util::log::Error() << "Missing launcher theme, defaulting to 'hicolor'.\n";
    icon_theme_name = "hicolor";
  } else {
    util::log::Error() << "Loading \"" << icon_theme_name << "\". Icon theme:";
  }

  std::list<std::string> queue{icon_theme_name};
  std::set<std::string> queued{icon_theme_name};

  while (!queue.empty()) {
    if (queue.empty()) {
      if (queued.count("hicolor") != 0) {
        // hicolor loaded
        break;
      }

      queue.push_back("hicolor");
      queued.insert("hicolor");
    }

    std::string name(queue.front());
    queue.pop_front();

    util::log::Error() << " '" << name << "',";
    auto theme = LoadTheme(name);

    if (theme != nullptr) {
      list_themes_.push_back(theme);

      auto position = queue.begin();

      for (auto const& item : theme->list_inherits) {
        if (queued.count(item) == 0) {
          queue.insert(position++, item);
          queued.insert(item);
        }
      }
    }
  }

  util::log::Error() << '\n';
}

int DirectoryMatchesSize(IconThemeDir* dir, int size) {
  if (dir->type == ICON_DIR_TYPE_FIXED) {
    return dir->size == size;
  } else if (dir->type == ICON_DIR_TYPE_SCALABLE) {
    return dir->min_size <= size && size <= dir->max_size;
  } else { /*if (dir->type == ICON_DIR_TYPE_THRESHOLD)*/
    return dir->size - dir->threshold <= size &&
           size <= dir->size + dir->threshold;
  }
}

int DirectorySizeDistance(IconThemeDir* dir, int size) {
  if (dir->type == ICON_DIR_TYPE_FIXED) {
    return abs(dir->size - size);
  } else if (dir->type == ICON_DIR_TYPE_SCALABLE) {
    if (size < dir->min_size) {
      return dir->min_size - size;
    } else if (size > dir->max_size) {
      return size - dir->max_size;
    }

    return 0;
  } else { /*if (dir->type == ICON_DIR_TYPE_THRESHOLD)*/
    if (size < dir->size - dir->threshold) {
      return dir->min_size - size;
    } else if (size > dir->size + dir->threshold) {
      return size - dir->max_size;
    }

    return 0;
  }
}

// Returns the full path to an icon file (or nullptr) given the icon name
std::string Launcher::GetIconPath(std::string const& icon_name, int size) {
  if (icon_name.empty()) {
    return std::string();
  }

  // If the icon_name is already a path and the file exists, return it
  if (icon_name[0] == '/') {
    if (util::fs::FileExists(icon_name)) {
      return icon_name;
    }

    return std::string();
  }

  std::vector<std::string> basenames{
      util::fs::HomeDirectory() / ".icons",
      util::fs::HomeDirectory() / ".local" / "share" / "icons",
      "/usr/local/share/icons",
      "/usr/local/share/pixmaps",
      "/usr/share/icons",
      "/usr/share/pixmaps",
  };

  std::vector<std::string> extensions{".png", ".xpm"};

  // if the icon name already contains one of the extensions (e.g. vlc.png
  // instead of vlc) add a special entry
  for (auto const& extension : extensions) {
    if (icon_name.length() > extension.length() &&
        icon_name.substr(icon_name.length() - extension.length()) ==
            extension) {
      extensions.push_back("");
      break;
    }
  }

  // Stage 1: best size match
  // Contrary to the freedesktop spec, we are not choosing the closest icon in
  // size, but the next larger icon
  // otherwise the quality is usually crap (for size 22, if you can choose 16 or
  // 32, you're better with 32)
  // We do fallback to the closest size if we cannot find a larger or equal icon

  // These 3 variables are used for keeping the closest size match
  int minimal_size = INT_MAX;
  std::string best_file_name;
  IconTheme* best_file_theme = nullptr;

  // These 3 variables are used for keeping the next larger match
  int next_larger_size = -1;
  std::string next_larger;
  IconTheme* next_larger_theme = nullptr;

  for (auto const& theme : list_themes_) {
    for (auto const& dir : theme->list_directories) {
      for (auto const& base_name : basenames) {
        for (auto const& extension : extensions) {
          std::string icon_file_name(icon_name + extension);
          std::string file_name = util::fs::BuildPath(
              {base_name, theme->name, dir->name, icon_file_name});

          if (util::fs::FileExists(file_name)) {
            // Closest match
            if (DirectorySizeDistance(dir, size) < minimal_size &&
                (!best_file_theme || theme == best_file_theme)) {
              best_file_name = file_name;
              minimal_size = DirectorySizeDistance(dir, size);
              best_file_theme = theme;
            }

            // Next larger match
            if (dir->size >= size &&
                (next_larger_size == -1 || dir->size < next_larger_size) &&
                (!next_larger_theme || theme == next_larger_theme)) {
              next_larger = file_name;
              next_larger_size = dir->size;
              next_larger_theme = theme;
            }
          }
        }
      }
    }
  }

  if (!next_larger.empty()) {
    return next_larger;
  }

  if (!best_file_name.empty()) {
    return best_file_name;
  }

  // Stage 2: look in unthemed icons
  for (auto const& base_name : basenames) {
    for (auto const& extension : extensions) {
      std::string icon_file_name(icon_name + extension);
      std::string file_name = util::fs::BuildPath({base_name, icon_file_name});

      if (util::fs::FileExists(file_name)) {
        return file_name;
      }
    }
  }

  util::log::Error() << "Could not find icon " << icon_name.c_str() << '\n';
  return std::string();
}

#ifdef _TINT3_DEBUG

std::string Launcher::GetFriendlyName() const { return "Launcher"; }

#endif  // _TINT3_DEBUG
