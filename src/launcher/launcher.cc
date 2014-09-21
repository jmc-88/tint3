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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <string.h>
#include <stdio.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <glib/gi18n.h>

#ifdef HAVE_SN
#include <libsn/sn.h>
#endif

#include <memory>
#include <string>

#include "server.h"
#include "panel.h"
#include "taskbar.h"
#include "launcher.h"
#include "util/fs.h"

int launcher_enabled;
int launcher_max_icon_size;
int launcher_tooltip_enabled;
int launcher_alpha;
int launcher_saturation;
int launcher_brightness;
std::string icon_theme_name;
XSettingsClient* xsettings_client;

#define ICON_FALLBACK "application-x-executable"

std::string icon_path(Launcher* launcher, std::string const& icon_name,
                      int size);
void launcher_load_themes(Launcher* launcher);
void free_desktop_entry(DesktopEntry* entry);
int launcher_read_desktop_file(const char* path, DesktopEntry* entry);
Imlib_Image scale_icon(Imlib_Image original, int icon_size);
void free_icon(Imlib_Image icon);
void free_icon_theme(IconTheme* theme);


void default_launcher() {
    launcher_enabled = 0;
    launcher_max_icon_size = 0;
    launcher_tooltip_enabled = 0;
    launcher_alpha = 100;
    launcher_saturation = 0;
    launcher_brightness = 0;
    icon_theme_name.clear();
    xsettings_client = nullptr;
}


void init_launcher() {
    if (launcher_enabled) {
        // if XSETTINGS manager running, tint3 read the icon_theme_name.
        xsettings_client = xsettings_client_new(server.dsp, server.screen,
                                                xsettings_notify_cb, nullptr, nullptr);
    }
}


void init_launcher_panel(void* p) {
    Panel* panel = static_cast<Panel*>(p);
    Launcher* launcher = &panel->launcher;

    launcher->parent = panel;
    launcher->panel = panel;
    launcher->size_mode = SIZE_BY_CONTENT;
    launcher->need_resize = true;
    launcher->need_redraw = true;

    if (launcher->bg == 0) {
        launcher->bg = backgrounds.front();
    }

    // check consistency
    if (launcher->list_apps.empty()) {
        return;
    }

    launcher->on_screen = 1;
    panel_refresh = 1;

    launcher_load_themes(launcher);
    launcher_load_icons(launcher);
}


void cleanup_launcher() {
    if (xsettings_client) {
        xsettings_client_destroy(xsettings_client);
    }

    for (int i = 0 ; i < nb_panel ; i++) {
        cleanup_launcher_theme(panel1[i].launcher);
    }

    for (auto const& app : panel_config.launcher.list_apps) {
        free(app);
    }

    panel_config.launcher.list_apps.clear();
    icon_theme_name.clear();
    launcher_enabled = 0;
}


void cleanup_launcher_theme(Launcher& launcher) {
    launcher.free_area();

    for (auto const& launcherIcon : launcher.list_icons) {
        if (launcherIcon) {
            free_icon(launcherIcon->icon_scaled);
            free_icon(launcherIcon->icon_original);
            free(launcherIcon->icon_name);
            free(launcherIcon->icon_path);
            free(launcherIcon->cmd);
            free(launcherIcon->icon_tooltip);
        }

        delete launcherIcon;
    }

    for (auto const& theme : launcher.list_themes) {
        free_icon_theme(theme);
        delete theme;
    }

    launcher.list_icons.clear();
    launcher.list_themes.clear();
}


bool Launcher::resize() {
    int icons_per_column = 1, icons_per_row = 1, marging = 0;
    int icon_size;

    if (panel_horizontal) {
        icon_size = height;
    } else {
        icon_size = width;
    }

    icon_size -= (2 * bg->border.width) - (2 * paddingy);

    if (launcher_max_icon_size > 0 && icon_size > launcher_max_icon_size) {
        icon_size = launcher_max_icon_size;
    }

    // Resize icons if necessary
    for (auto& launcherIcon : list_icons) {
        if (launcherIcon->icon_size != icon_size || !launcherIcon->icon_original) {
            launcherIcon->icon_size = icon_size;
            launcherIcon->width = launcherIcon->icon_size;
            launcherIcon->height = launcherIcon->icon_size;

            // Get the path for an icon file with the new size
            std::string new_icon_path = icon_path(this, launcherIcon->icon_name,
                                                  launcherIcon->icon_size);

            if (new_icon_path.empty()) {
                // Draw a blank icon
                free_icon(launcherIcon->icon_original);
                launcherIcon->icon_original = nullptr;
                free_icon(launcherIcon->icon_scaled);
                launcherIcon->icon_scaled = nullptr;
                new_icon_path = icon_path(this, ICON_FALLBACK, launcherIcon->icon_size);

                if (!new_icon_path.empty()) {
                    launcherIcon->icon_original = imlib_load_image(new_icon_path.c_str());
                    fprintf(stderr, "launcher.c %d: Using icon %s\n", __LINE__,
                            new_icon_path.c_str());
                }

                launcherIcon->icon_scaled = scale_icon(launcherIcon->icon_original, icon_size);
                continue;
            }

            if (launcherIcon->icon_path && new_icon_path == launcherIcon->icon_path) {
                // If it's the same file just rescale
                free_icon(launcherIcon->icon_scaled);
                launcherIcon->icon_scaled = scale_icon(launcherIcon->icon_original, icon_size);

                fprintf(stderr,
                        "launcher.c %d: Using icon %s\n", __LINE__,
                        launcherIcon->icon_path);
            } else {
                // Free the old files
                free_icon(launcherIcon->icon_original);
                free_icon(launcherIcon->icon_scaled);
                // Load the new file and scale
                launcherIcon->icon_original = imlib_load_image(new_icon_path.c_str());
                launcherIcon->icon_scaled = scale_icon(launcherIcon->icon_original,
                                                       launcherIcon->icon_size);
                free(launcherIcon->icon_path);
                launcherIcon->icon_path = strdup(new_icon_path.c_str());

                fprintf(stderr,
                        "launcher.c %d: Using icon %s\n", __LINE__,
                        launcherIcon->icon_path);
            }
        }
    }

    size_t count = list_icons.size();

    if (panel_horizontal) {
        if (count == 0) {
            width = 0;
        } else {
            int height_ = height - 2 * bg->border.width - 2 * paddingy;
            // here icons_per_column always higher than 0
            icons_per_column = (height_ + paddingx) / (icon_size + paddingx);
            marging = height_ - (icons_per_column - 1) * (icon_size + paddingx) - icon_size;
            icons_per_row = count / icons_per_column + (count % icons_per_column != 0);
            width = (2 * bg->border.width) + (2 * paddingxlr) + (icon_size * icons_per_row)
                    + ((icons_per_row - 1) * paddingx);
        }
    } else {
        if (count == 0) {
            height = 0;
        } else {
            int width_ = width - 2 * bg->border.width - 2 * paddingy;
            // here icons_per_row always higher than 0
            icons_per_row = (width_ + paddingx) / (icon_size + paddingx);
            marging = width_ - (icons_per_row - 1) * (icon_size + paddingx) - icon_size;
            icons_per_column = count / icons_per_row + (count % icons_per_row != 0);
            height = (2 * bg->border.width) + (2 * paddingxlr) + (icon_size *
                     icons_per_column) + ((icons_per_column - 1) * paddingx);
        }
    }

    int posx, posy;
    int start = bg->border.width + paddingy + marging / 2;

    if (panel_horizontal) {
        posy = start;
        posx = bg->border.width + paddingxlr;
    } else {
        posx = start;
        posy = bg->border.width + paddingxlr;
    }

    int i = 1;

    for (auto& launcherIcon : list_icons) {
        launcherIcon->y = posy;
        launcherIcon->x = posx;

        if (panel_horizontal) {
            if (i % icons_per_column) {
                posy += (icon_size + paddingx);
            } else {
                posy = start;
                posx += (icon_size + paddingx);
            }
        } else {
            if (i % icons_per_column) {
                posx += (icon_size + paddingx);
            } else {
                posx = start;
                posy += (icon_size + paddingx);
            }
        }

        ++i;
    }

    return 1;
}

// Here we override the default layout of the icons; normally Area layouts its children
// in a stack; we need to layout them in a kind of table
void LauncherIcon::on_change_layout() {
    posy = (parent->posy + y);
    posx = (parent->posx + x);
}

const char* LauncherIcon::get_tooltip_text() {
    return launcher_tooltip_enabled ? icon_tooltip : nullptr;
}

void LauncherIcon::draw_foreground(cairo_t* c) {
    // Render
    imlib_context_set_image(icon_scaled);

    if (server.real_transparency) {
        render_image(pix, 0, 0, imlib_image_get_width(),
                     imlib_image_get_height());
    } else {
        imlib_context_set_drawable(pix);
        imlib_render_image_on_drawable(0, 0);
    }
}

Imlib_Image scale_icon(Imlib_Image original, int icon_size) {
    Imlib_Image icon_scaled;

    if (original) {
        imlib_context_set_image(original);
        icon_scaled = imlib_create_cropped_scaled_image(0, 0, imlib_image_get_width(),
                      imlib_image_get_height(), icon_size, icon_size);
        imlib_context_set_image(icon_scaled);
        imlib_image_set_has_alpha(1);
        DATA32* data = imlib_image_get_data();
        adjust_asb(data, icon_size, icon_size, launcher_alpha,
                   (float)launcher_saturation / 100, (float)launcher_brightness / 100);
        imlib_image_put_back_data(data);
    } else {
        icon_scaled = imlib_create_image(icon_size, icon_size);
        imlib_context_set_image(icon_scaled);
        imlib_context_set_color(255, 255, 255, 255);
        imlib_image_fill_rectangle(0, 0, icon_size, icon_size);
    }

    return icon_scaled;
}

void free_icon(Imlib_Image icon) {
    if (icon) {
        imlib_context_set_image(icon);
        imlib_free_image();
    }
}

void launcher_action(LauncherIcon* icon, XEvent* evt) {
#if HAVE_SN
    auto ctx = sn_launcher_context_new(server.sn_dsp, server.screen);
    sn_launcher_context_set_name(ctx, icon->icon_tooltip);
    sn_launcher_context_set_description(ctx, "Application launched from tint3");
    sn_launcher_context_set_binary_name(ctx, icon->cmd);

    // Get a timestamp from the X event
    Time time;

    if (evt->type == ButtonPress || evt->type == ButtonRelease) {
        time = evt->xbutton.time;
    } else {
        fprintf(stderr, "Unknown X event: %d\n", evt->type);
        return;
    }

    sn_launcher_context_initiate(ctx, "tint3", icon->cmd, time);
#endif /* HAVE_SN */

    // TODO: make this use tint_exec...
    pid_t pid = fork();

    if (pid < 0) {
        fprintf(stderr, "Could not fork\n");
    } else if (pid == 0) {
#if HAVE_SN
        sn_launcher_context_setup_child_process(ctx);
#endif // HAVE_SN
        // Allow children to exist after parent destruction
        setsid();
        // Run the command
        execl("/bin/sh", "/bin/sh", "-c", icon->cmd, nullptr);

        fprintf(stderr, "Failed to execlp %s\n", icon->cmd);
#if HAVE_SN
        // TODO: how can this not leak? On a successful execl, this line is
        // never executed.
        sn_launcher_context_unref(ctx);
#endif // HAVE_SN
        _exit(1);
    }

#if HAVE_SN
    else {
        server.pids[pid] = ctx;
    }

#endif // HAVE_SN
}

/***************** Freedesktop app.desktop and icon theme handling  *********************/
/* http://standards.freedesktop.org/desktop-entry-spec/ */
/* http://standards.freedesktop.org/icon-theme-spec/ */

// Splits line at first '=' and returns 1 if successful, and parts are not empty
// key and value point to the parts
bool parse_desktop_line(std::string const& line, std::string& key,
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

    if (!found || key.empty() || value.empty()) {
        return false;
    }

    return true;
}

bool parse_theme_line(std::string const& line, std::string& key,
                      std::string& value) {
    return parse_desktop_line(line, key, value);
}

void expand_exec(DesktopEntry* entry, char const* path) {
    // Expand % in exec
    // %i -> --icon Icon
    // %c -> Name
    // %k -> path
    if (!entry->exec) {
        return;
    }

    std::string exec2;

    // p will never point to an escaped char
    for (char const* p = entry->exec; *p != '\0'; ++p) {
        exec2.push_back(*p);

        if (*p == '\\') {
            ++p;

            if (*p == '\0') {
                break;
            }

            // Copy the escaped char
            if (*p == '%') { // For % we delete the backslash, i.e. write % over it
                exec2[exec2.length() - 1] = '%';
            } else {
                exec2.push_back(*p);
            }
        } else if (*p == '%') {
            ++p;

            if (*p == '\0') {
                break;
            }

            if (*p == 'i' && entry->icon != nullptr) {
                exec2.append(StringBuilder()
                             << "--icon '" << entry->icon << '\'');
            } else if (*p == 'c' && entry->name != nullptr) {
                exec2.append(StringBuilder() << '\'' << entry->name << '\'');
            } else {
                exec2.append(StringBuilder() << '\'' << path << '\'');
            }
        }
    }

    std::free(entry->exec);
    entry->exec = strdup(exec2.c_str());
}

int launcher_read_desktop_file(const char* path, DesktopEntry* entry) {
    entry->name = entry->icon = entry->exec = nullptr;

    gchar** languages = (gchar**)g_get_language_names();
    int i;

    for (i = 0; languages[i]; ++i) {}

    // lang_index_default is a constant that encodes the Name key without a language
    int lang_index_default = i;
    // lang_index is the index of the language for the best Name key in the language vector
    // we currently do not know about any Name key at all, so use an invalid index
    int lang_index = lang_index_default + 1;

    bool inside_desktop_entry = false;
    bool read = fs::ReadFileByLine(path, [&](std::string const & data) {
        std::string line(data);
        StringTrim(line);

        if (line.empty()) {
            return;
        }

        if (line[0] == '[') {
            inside_desktop_entry = (line == "[Desktop Entry]");
        }

        std::string key, value;

        if (inside_desktop_entry && parse_desktop_line(line, key, value)) {
            if (key.substr(0, 4) == "Name") {
                if (key == "Name" && lang_index > lang_index_default) {
                    entry->name = strdup(value.c_str());
                    lang_index = lang_index_default;
                } else {
                    for (i = 0; languages[i] && i < lang_index; i++) {
                        std::string localized_key;
                        localized_key.append("Name[");
                        localized_key.append(languages[i]);
                        localized_key.append("]");

                        if (key == localized_key) {
                            if (entry->name) {
                                free(entry->name);
                            }

                            entry->name = strdup(value.c_str());
                            lang_index = i;
                        }
                    }
                }
            } else if (!entry->exec && key == "Exec") {
                entry->exec = strdup(value.c_str());
            } else if (!entry->icon && key == "Icon") {
                entry->icon = strdup(value.c_str());
            }
        }
    });

    if (!read) {
        fprintf(stderr, "Could not open file %s\n", path);
        return 0;
    }

    // From this point:
    // entry->name, entry->icon, entry->exec will never be empty strings (can be nullptr though)

    expand_exec(entry, path);
    return 1;
}

void free_desktop_entry(DesktopEntry* entry) {
    free(entry->name);
    free(entry->icon);
    free(entry->exec);
}

void test_launcher_read_desktop_file() {
    fprintf(stdout, "\033[1;33m");
    DesktopEntry entry;
    launcher_read_desktop_file("/usr/share/applications/firefox.desktop", &entry);
    printf("Name:%s Icon:%s Exec:%s\n", entry.name, entry.icon, entry.exec);
    fprintf(stdout, "\033[0m");
}

//TODO Use UTF8 when parsing the file
IconTheme* load_theme(char const* name) {
    // Look for name/index.theme in $HOME/.icons, /usr/share/icons, /usr/share/pixmaps (stop at the first found)
    // Parse index.theme -> list of IconThemeDir with attributes
    // Return IconTheme*

    if (name == nullptr) {
        return nullptr;
    }

    auto file_name = fs::BuildPath({
        fs::HomeDirectory(), ".icons", name, "index.theme"
    });

    if (!fs::FileExists(file_name)) {
        file_name = fs::BuildPath({ "/usr/share/icons", name, "index.theme" });

        if (!fs::FileExists(file_name)) {
            file_name = fs::BuildPath({ "/usr/share/pixmaps", name, "index.theme" });

            if (!fs::FileExists(file_name)) {
                file_name.clear();
            }
        }
    }

    if (file_name.empty()) {
        return nullptr;
    }

    auto theme = new IconTheme();
    theme->name = name;
    theme->list_inherits = nullptr;
    theme->list_directories = nullptr;

    IconThemeDir* current_dir = nullptr;
    bool inside_header = true;

    bool read = fs::ReadFileByLine(file_name, [&](std::string const & data) {
        std::string line(data);
        StringTrim(line);

        if (line.empty()) {
            return;
        }

        std::string key, value;

        if (inside_header) {
            if (parse_theme_line(line, key, value)) {
                if (key == "Inherits") {
                    // value is like oxygen,wood,default
                    // TODO: remove these strdup/strtok calls
                    char* value_ptr = strdup(value.c_str());
                    char* token = strtok(value_ptr, ",\n");

                    while (token != nullptr) {
                        theme->list_inherits = g_slist_append(theme->list_inherits, strdup(token));
                        token = strtok(nullptr, ",\n");
                    }

                    std::free(value_ptr);
                } else if (key == "Directories") {
                    // value is like 48x48/apps,48x48/mimetypes,32x32/apps,scalable/apps,scalable/mimetypes
                    // TODO: remove these strdup/strtok calls
                    char* value_ptr = strdup(value.c_str());
                    char* token = strtok(value_ptr, ",\n");

                    while (token != nullptr) {
                        auto dir = new IconThemeDir();
                        dir->name = strdup(token);
                        dir->max_size = dir->min_size = dir->size = -1;
                        dir->type = ICON_DIR_TYPE_THRESHOLD;
                        dir->threshold = 2;
                        theme->list_directories = g_slist_append(theme->list_directories, dir);
                        token = strtok(nullptr, ",\n");
                    }

                    std::free(value_ptr);
                }
            }
        } else if (current_dir != nullptr) {
            if (parse_theme_line(line, key, value)) {
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
                    // value is Fixed, Scalable or Threshold : default to scalable for unknown Type.
                    if (value == "Fixed") {
                        current_dir->type = ICON_DIR_TYPE_FIXED;
                    } else if (value == "Threshold") {
                        current_dir->type = ICON_DIR_TYPE_THRESHOLD;
                    } else {
                        current_dir->type = ICON_DIR_TYPE_SCALABLE;
                    }
                } else if (key == "Context") {
                    // usual values: Actions, Applications, Devices, FileSystems, MimeTypes
                    current_dir->context = strdup(value.c_str());
                }
            }
        }

        if (line[0] == '[' && line[line.length() - 1] == ']'
            && line != "[Icon Theme]") {
            inside_header = false;
            current_dir = nullptr;
            GSList* dir_item = theme->list_directories;
            std::string dir_name = line.substr(1, line.length() - 2);

            while (dir_item != nullptr) {
                IconThemeDir* dir = static_cast<IconThemeDir*>(dir_item->data);

                if (dir_name == dir->name) {
                    current_dir = dir;
                    break;
                }

                dir_item = g_slist_next(dir_item);
            }
        }
    });

    if (!read) {
        fprintf(stderr, "Could not open theme '%s'\n", file_name.c_str());
        delete theme;
        return nullptr;
    }

    return theme;
}

void free_icon_theme(IconTheme* theme) {
    for (auto l_inherits = theme->list_inherits; l_inherits ;
         l_inherits = l_inherits->next) {
        free(l_inherits->data);
    }

    for (auto l_dir = theme->list_directories; l_dir ; l_dir = l_dir->next) {
        IconThemeDir* dir = (IconThemeDir*)l_dir->data;
        free(dir->name);
        free(dir->context);
        delete dir;
    }
}

void test_launcher_read_theme_file() {
    fprintf(stdout, "\033[1;33m");
    auto theme = load_theme("oxygen");

    if (!theme) {
        printf("Could not load theme\n");
        return;
    }

    printf("Loaded theme: %s\n", theme->name.c_str());
    GSList* item = theme->list_inherits;

    while (item != nullptr) {
        printf("Inherits:%s\n", (char*)item->data);
        item = g_slist_next(item);
    }

    item = theme->list_directories;

    while (item != nullptr) {
        IconThemeDir* dir = static_cast<IconThemeDir*>(item->data);
        printf("Dir:%s Size=%d MinSize=%d MaxSize=%d Threshold=%d Type=%s Context=%s\n",
               dir->name, dir->size, dir->min_size, dir->max_size, dir->threshold,
               dir->type == ICON_DIR_TYPE_FIXED ? "Fixed" :
               dir->type == ICON_DIR_TYPE_SCALABLE ? "Scalable" :
               dir->type == ICON_DIR_TYPE_THRESHOLD ? "Threshold" : "?????",
               dir->context);
        item = g_slist_next(item);
    }

    fprintf(stdout, "\033[0m");
}


// Populates the list_icons list
void launcher_load_icons(Launcher* launcher) {
    // Load apps (.desktop style launcher items)
    for (auto const& app : launcher->list_apps) {
        DesktopEntry entry;
        launcher_read_desktop_file(app, &entry);

        if (entry.exec) {
            auto launcherIcon = new LauncherIcon();
            launcherIcon->parent = launcher;
            launcherIcon->panel = launcher->panel;
            launcherIcon->size_mode = SIZE_BY_CONTENT;
            launcherIcon->need_resize = false;
            launcherIcon->need_redraw = true;
            launcherIcon->bg = backgrounds.front();
            launcherIcon->on_screen = 1;

            launcherIcon->is_app_desktop = 1;
            launcherIcon->cmd = strdup(entry.exec);
            launcherIcon->icon_name = entry.icon ? strdup(entry.icon) : strdup(
                                          ICON_FALLBACK);
            launcherIcon->icon_size = 1;
            launcherIcon->icon_tooltip = entry.name ? strdup(entry.name) : strdup(
                                             entry.exec);
            free_desktop_entry(&entry);
            launcher->list_icons.push_back(launcherIcon);
            launcherIcon->add_area();
        }
    }
}


// Populates the list_themes list
void launcher_load_themes(Launcher* launcher) {
    // load the user theme, all the inherited themes recursively (DFS), and the hicolor theme
    // avoid inheritance loops
    if (icon_theme_name.empty()) {
        fprintf(stderr, "Missing launcher theme, defaulting to 'hicolor'.\n");
        icon_theme_name = "hicolor";
    } else {
        fprintf(stderr, "Loading %s. Icon theme:", icon_theme_name.c_str());
    }

    GSList* queue = g_slist_append(nullptr, strdup(icon_theme_name.c_str()));
    GSList* queued = g_slist_append(nullptr, strdup(icon_theme_name.c_str()));

    bool hicolor_loaded = false;

    while (queue || !hicolor_loaded) {
        if (!queue) {
            GSList* queued_item = queued;

            while (queued_item != nullptr) {
                if (strcmp(static_cast<char*>(queued_item->data), "hicolor") == 0) {
                    hicolor_loaded = true;
                    break;
                }

                queued_item = g_slist_next(queued_item);
            }

            if (hicolor_loaded) {
                break;
            }

            queue = g_slist_append(queue, strdup("hicolor"));
            queued = g_slist_append(queued, strdup("hicolor"));
        }

        char* name = static_cast<char*>(queue->data);
        queue = g_slist_remove(queue, name);

        fprintf(stderr, " '%s',", name);
        auto theme = load_theme(name);

        if (theme != nullptr) {
            launcher->list_themes.push_back(theme);

            GSList* item = theme->list_inherits;
            int pos = 0;

            while (item != nullptr) {
                char* parent = static_cast<char*>(item->data);
                int duplicate = 0;
                GSList* queued_item = queued;

                while (queued_item != nullptr) {
                    if (strcmp(static_cast<char*>(queued_item->data), parent) == 0) {
                        duplicate = 1;
                        break;
                    }

                    queued_item = g_slist_next(queued_item);
                }

                if (!duplicate) {
                    queue = g_slist_insert(queue, strdup(parent), pos);
                    pos++;
                    queued = g_slist_append(queued, strdup(parent));
                }

                item = g_slist_next(item);
            }
        }
    }

    fprintf(stderr, "\n");

    // Free the queue
    GSList* l;

    for (l = queue; l ; l = l->next) {
        free(l->data);
    }

    g_slist_free(queue);

    for (l = queued; l ; l = l->next) {
        free(l->data);
    }

    g_slist_free(queued);
}

int directory_matches_size(IconThemeDir* dir, int size) {
    if (dir->type == ICON_DIR_TYPE_FIXED) {
        return dir->size == size;
    } else if (dir->type == ICON_DIR_TYPE_SCALABLE) {
        return dir->min_size <= size && size <= dir->max_size;
    } else { /*if (dir->type == ICON_DIR_TYPE_THRESHOLD)*/
        return dir->size - dir->threshold <= size && size <= dir->size + dir->threshold;
    }
}

int directory_size_distance(IconThemeDir* dir, int size) {
    if (dir->type == ICON_DIR_TYPE_FIXED) {
        return abs(dir->size - size);
    } else if (dir->type == ICON_DIR_TYPE_SCALABLE) {
        if (size < dir->min_size) {
            return dir->min_size - size;
        } else if (size > dir->max_size) {
            return size - dir->max_size;
        } else {
            return 0;
        }
    } else { /*if (dir->type == ICON_DIR_TYPE_THRESHOLD)*/
        if (size < dir->size - dir->threshold) {
            return dir->min_size - size;
        } else if (size > dir->size + dir->threshold) {
            return size - dir->max_size;
        } else {
            return 0;
        }
    }
}

// Returns the full path to an icon file (or nullptr) given the icon name
std::string icon_path(Launcher* launcher, std::string const& icon_name,
                      int size) {
    if (icon_name.empty()) {
        return std::string();
    }

    // If the icon_name is already a path and the file exists, return it
    if (icon_name[0] == '/') {
        if (fs::FileExists(icon_name)) {
            return icon_name;
        }

        return std::string();
    }

    std::vector<std::string> basenames {
        fs::BuildPath({ fs::HomeDirectory(), ".icons" }),
        fs::BuildPath({ fs::HomeDirectory(), ".local", "share", "icons" }),
        "/usr/local/share/icons",
        "/usr/local/share/pixmaps",
        "/usr/share/icons",
        "/usr/share/pixmaps",
    };

    std::vector<std::string> extensions {
        ".png", ".xpm"
    };

    // if the icon name already contains one of the extensions (e.g. vlc.png instead of vlc) add a special entry
    for (auto const& extension : extensions) {
        if (icon_name.length() > extension.length() &&
            icon_name.substr(icon_name.length() - extension.length()) == extension) {
            extensions.push_back("");
            break;
        }
    }

    // Stage 1: best size match
    // Contrary to the freedesktop spec, we are not choosing the closest icon in size, but the next larger icon
    // otherwise the quality is usually crap (for size 22, if you can choose 16 or 32, you're better with 32)
    // We do fallback to the closest size if we cannot find a larger or equal icon

    // These 3 variables are used for keeping the closest size match
    int minimal_size = INT_MAX;
    std::string best_file_name;
    IconTheme* best_file_theme = nullptr;

    // These 3 variables are used for keeping the next larger match
    int next_larger_size = -1;
    std::string next_larger;
    IconTheme* next_larger_theme = nullptr;

    for (auto const& theme : launcher->list_themes) {
        GSList* dir;

        for (dir = theme->list_directories; dir; dir = g_slist_next(dir)) {
            for (auto const& base_name : basenames) {
                for (auto const& extension : extensions) {
                    char* dir_name = ((IconThemeDir*)dir->data)->name;
                    std::string icon_file_name(icon_name + extension);
                    std::string file_name = fs::BuildPath({
                        base_name, theme->name, dir_name, icon_file_name
                    });

                    if (fs::FileExists(file_name)) {
                        // Closest match
                        if (directory_size_distance((IconThemeDir*)dir->data, size) < minimal_size
                            && (!best_file_theme ? true : theme == best_file_theme)) {
                            best_file_name = file_name;
                            minimal_size = directory_size_distance((IconThemeDir*)dir->data, size);
                            best_file_theme = theme;
                        }

                        // Next larger match
                        if (((IconThemeDir*)dir->data)->size >= size &&
                            (next_larger_size == -1 || ((IconThemeDir*)dir->data)->size < next_larger_size)
                            &&
                            (!next_larger_theme ? 1 : theme == next_larger_theme)) {
                            next_larger = file_name;
                            next_larger_size = ((IconThemeDir*)dir->data)->size;
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
            std::string file_name = fs::BuildPath({
                base_name, icon_file_name
            });

            if (fs::FileExists(file_name)) {
                return file_name;
            }
        }
    }

    fprintf(stderr, "Could not find icon %s\n", icon_name.c_str());
    return std::string();
}

