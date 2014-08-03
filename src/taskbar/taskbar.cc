/**************************************************************************
*
* Tint3 : taskbar
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <Imlib2.h>

#include <algorithm>

#include "task.h"
#include "taskbar.h"
#include "server.h"
#include "window.h"
#include "panel.h"


/* win_to_task_table holds for every Window an array of tasks. Usually the array contains only one
   element. However for omnipresent windows (windows which are visible in every taskbar) the array
   contains to every Task* on each panel a pointer (i.e. GPtrArray.len == server.nb_desktop)
*/
GHashTable* win_to_task_table;

Task* task_active;
Task* task_drag;
int taskbar_enabled;

guint win_hash(gconstpointer key) {
    return (guint) * ((Window*)key);
}
gboolean win_compare(gconstpointer a, gconstpointer b) {
    return (*((Window*)a) == *((Window*)b));
}
void free_ptr_array(gpointer data) {
    g_ptr_array_free(static_cast<GPtrArray*>(data), 1);
}


void default_taskbar() {
    win_to_task_table = 0;
    urgent_timeout = 0;
    urgent_list = 0;
    taskbar_enabled = 0;
    default_taskbarname();
}

void cleanup_taskbar() {
    cleanup_taskbarname();

    if (win_to_task_table) {
        g_hash_table_foreach(win_to_task_table, taskbar_remove_task, 0);
    }

    for (int i = 0 ; i < nb_panel; ++i) {
        Panel* panel = &panel1[i];

        for (int j = 0 ; j < panel->nb_desktop ; ++j) {
            Taskbar* tskbar = &panel->taskbar[j];

            for (int k = 0; k < TASKBAR_STATE_COUNT; ++k) {
                if (tskbar->state_pix[k]) {
                    XFreePixmap(server.dsp, tskbar->state_pix[k]);
                }
            }

            tskbar->free_area();
            // remove taskbar from the panel
            auto it = std::find(panel->children.begin(),
                                panel->children.end(),
                                tskbar);

            if (it != panel->children.end()) {
                panel->children.erase(it);
            }
        }

        if (panel->taskbar) {
            free(panel->taskbar);
            panel->taskbar = 0;
        }
    }

    if (win_to_task_table) {
        g_hash_table_destroy(win_to_task_table);
        win_to_task_table = 0;
    }
}


void init_taskbar() {
    if (win_to_task_table == 0) {
        win_to_task_table = g_hash_table_new_full(win_hash, win_compare, free,
                            free_ptr_array);
    }

    task_active = 0;
    task_drag = 0;
}


void init_taskbar_panel(void* p) {
    Panel* panel = (Panel*)p;
    int j;

    if (panel->g_taskbar.background[TASKBAR_NORMAL] == 0) {
        panel->g_taskbar.background[TASKBAR_NORMAL] = backgrounds.front();
        panel->g_taskbar.background[TASKBAR_ACTIVE] = backgrounds.front();
    }

    if (panel->g_taskbar.background_name[TASKBAR_NORMAL] == 0) {
        panel->g_taskbar.background_name[TASKBAR_NORMAL] = backgrounds.front();
        panel->g_taskbar.background_name[TASKBAR_ACTIVE] = backgrounds.front();
    }

    if (panel->g_task.bg == 0) {
        panel->g_task.bg = backgrounds.front();
    }

    // taskbar name
    panel->g_taskbar.area_name.panel = panel;
    panel->g_taskbar.area_name.size_mode = SIZE_BY_CONTENT;
    panel->g_taskbar.area_name._resize = resize_taskbarname;
    panel->g_taskbar.area_name._draw_foreground = draw_taskbarname;
    panel->g_taskbar.area_name._on_change_layout = 0;
    panel->g_taskbar.area_name.resize = 1;
    panel->g_taskbar.area_name.on_screen = 1;

    // taskbar
    panel->g_taskbar.parent = panel;
    panel->g_taskbar.panel = panel;
    panel->g_taskbar.size_mode = SIZE_BY_LAYOUT;
    panel->g_taskbar._resize = resize_taskbar;
    panel->g_taskbar._draw_foreground = draw_taskbar;
    panel->g_taskbar._on_change_layout = on_change_taskbar;
    panel->g_taskbar.resize = 1;
    panel->g_taskbar.on_screen = 1;

    if (panel_horizontal) {
        panel->g_taskbar.posy = panel->bg->border.width +
                                panel->paddingy;
        panel->g_taskbar.height = panel->height - (2 *
                                  panel->g_taskbar.posy);
        panel->g_taskbar.area_name.posy = panel->g_taskbar.posy;
        panel->g_taskbar.area_name.height = panel->g_taskbar.height;
    } else {
        panel->g_taskbar.posx = panel->bg->border.width +
                                panel->paddingy;
        panel->g_taskbar.width = panel->width - (2 *
                                 panel->g_taskbar.posx);
        panel->g_taskbar.area_name.posx = panel->g_taskbar.posx;
        panel->g_taskbar.area_name.width = panel->g_taskbar.width;
    }

    // task
    panel->g_task.panel = panel;
    panel->g_task.size_mode = SIZE_BY_LAYOUT;
    panel->g_task._draw_foreground = draw_task;
    panel->g_task._on_change_layout = on_change_task;
    panel->g_task.resize = 1;
    panel->g_task.on_screen = 1;

    if ((panel->g_task.config_asb_mask & (1 << TASK_NORMAL)) == 0) {
        panel->g_task.alpha[TASK_NORMAL] = 100;
        panel->g_task.saturation[TASK_NORMAL] = 0;
        panel->g_task.brightness[TASK_NORMAL] = 0;
    }

    if ((panel->g_task.config_asb_mask & (1 << TASK_ACTIVE)) == 0) {
        panel->g_task.alpha[TASK_ACTIVE] = panel->g_task.alpha[TASK_NORMAL];
        panel->g_task.saturation[TASK_ACTIVE] = panel->g_task.saturation[TASK_NORMAL];
        panel->g_task.brightness[TASK_ACTIVE] = panel->g_task.brightness[TASK_NORMAL];
    }

    if ((panel->g_task.config_asb_mask & (1 << TASK_ICONIFIED)) == 0) {
        panel->g_task.alpha[TASK_ICONIFIED] = panel->g_task.alpha[TASK_NORMAL];
        panel->g_task.saturation[TASK_ICONIFIED] =
            panel->g_task.saturation[TASK_NORMAL];
        panel->g_task.brightness[TASK_ICONIFIED] =
            panel->g_task.brightness[TASK_NORMAL];
    }

    if ((panel->g_task.config_asb_mask & (1 << TASK_URGENT)) == 0) {
        panel->g_task.alpha[TASK_URGENT] = panel->g_task.alpha[TASK_ACTIVE];
        panel->g_task.saturation[TASK_URGENT] = panel->g_task.saturation[TASK_ACTIVE];
        panel->g_task.brightness[TASK_URGENT] = panel->g_task.brightness[TASK_ACTIVE];
    }

    if ((panel->g_task.config_font_mask & (1 << TASK_NORMAL)) == 0)
        panel->g_task.font[TASK_NORMAL] = (Color) {
        {0, 0, 0}, 0
    };

    if ((panel->g_task.config_font_mask & (1 << TASK_ACTIVE)) == 0) {
        panel->g_task.font[TASK_ACTIVE] = panel->g_task.font[TASK_NORMAL];
    }

    if ((panel->g_task.config_font_mask & (1 << TASK_ICONIFIED)) == 0) {
        panel->g_task.font[TASK_ICONIFIED] = panel->g_task.font[TASK_NORMAL];
    }

    if ((panel->g_task.config_font_mask & (1 << TASK_URGENT)) == 0) {
        panel->g_task.font[TASK_URGENT] = panel->g_task.font[TASK_ACTIVE];
    }

    if ((panel->g_task.config_background_mask & (1 << TASK_NORMAL)) == 0) {
        panel->g_task.background[TASK_NORMAL] = backgrounds.front();
    }

    if ((panel->g_task.config_background_mask & (1 << TASK_ACTIVE)) == 0) {
        panel->g_task.background[TASK_ACTIVE] = panel->g_task.background[TASK_NORMAL];
    }

    if ((panel->g_task.config_background_mask & (1 << TASK_ICONIFIED)) == 0) {
        panel->g_task.background[TASK_ICONIFIED] =
            panel->g_task.background[TASK_NORMAL];
    }

    if ((panel->g_task.config_background_mask & (1 << TASK_URGENT)) == 0) {
        panel->g_task.background[TASK_URGENT] = panel->g_task.background[TASK_ACTIVE];
    }

    if (panel_horizontal) {
        panel->g_task.posy = panel->g_taskbar.posy +
                             panel->g_taskbar.background[TASKBAR_NORMAL]->border.width +
                             panel->g_taskbar.paddingy;
        panel->g_task.height = panel->height - (2 * panel->g_task.posy);
    } else {
        panel->g_task.posx = panel->g_taskbar.posx +
                             panel->g_taskbar.background[TASKBAR_NORMAL]->border.width +
                             panel->g_taskbar.paddingy;
        panel->g_task.width = panel->width - (2 * panel->g_task.posx);
        panel->g_task.height = panel->g_task.maximum_height;
    }

    for (j = 0; j < TASK_STATE_COUNT; ++j) {
        if (panel->g_task.background[j] == 0) {
            panel->g_task.background[j] = backgrounds.front();
        }

        if (panel->g_task.background[j]->border.rounded > panel->g_task.height /
            2) {
            printf("task%sbackground_id has a too large rounded value. Please fix your tint3rc\n",
                   j == 0 ? "_" : j == 1 ? "_active_" : j == 2 ? "_iconified_" : "_urgent_");
            /* backgrounds.push_back(*panel->g_task.background[j]); */
            /* panel->g_task.background[j] = backgrounds.back(); */
            panel->g_task.background[j]->border.rounded = panel->g_task.height / 2;
        }
    }

    // compute vertical position : text and icon
    int height_ink, height;
    get_text_size(panel->g_task.font_desc, &height_ink, &height, panel->height,
                  "TAjpg", 5);

    if (!panel->g_task.maximum_width && panel_horizontal) {
        panel->g_task.maximum_width = server.monitor[panel->monitor].width;
    }

    panel->g_task.text_posx = panel->g_task.background[0]->border.width +
                              panel->g_task.paddingxlr;
    panel->g_task.text_height = panel->g_task.height -
                                (2 * panel->g_task.paddingy);

    if (panel->g_task.icon) {
        panel->g_task.icon_size1 = panel->g_task.height - (2 *
                                   panel->g_task.paddingy);
        panel->g_task.text_posx += panel->g_task.icon_size1;
        panel->g_task.icon_posy = (panel->g_task.height -
                                   panel->g_task.icon_size1) / 2;
    }

    //printf("monitor %d, task_maximum_width %d\n", panel->monitor, panel->g_task.maximum_width);

    Taskbar* tskbar;
    panel->nb_desktop = server.nb_desktop;
    panel->taskbar = (Taskbar*) calloc(server.nb_desktop, sizeof(Taskbar));

    for (j = 0 ; j < panel->nb_desktop ; j++) {
        tskbar = &panel->taskbar[j];

        // TODO: nuke this from planet Earth ASAP - horrible hack to mimick the
        // original memcpy() call
        tskbar->clone(panel->g_taskbar);

        tskbar->desktop = j;

        if (j == server.desktop) {
            tskbar->bg = panel->g_taskbar.background[TASKBAR_ACTIVE];
        } else {
            tskbar->bg = panel->g_taskbar.background[TASKBAR_NORMAL];
        }
    }

    init_taskbarname_panel(panel);
}


void taskbar_remove_task(gpointer key, gpointer value, gpointer user_data) {
    remove_task(task_get_task(*static_cast<Window*>(key)));
}


Task* task_get_task(Window win) {
    GPtrArray* task_group = task_get_tasks(win);

    if (task_group) {
        return static_cast<Task*>(g_ptr_array_index(task_group, 0));
    }

    return 0;
}


GPtrArray* task_get_tasks(Window win) {
    if (win_to_task_table && taskbar_enabled) {
        return static_cast<GPtrArray*>(g_hash_table_lookup(win_to_task_table, &win));
    }

    return 0;
}


void task_refresh_tasklist() {
    if (!taskbar_enabled) {
        return;
    }

    int num_results;
    Window* win = static_cast<Window*>(server_get_property(
                                           server.root_win,
                                           server.atom._NET_CLIENT_LIST,
                                           XA_WINDOW,
                                           &num_results));

    if (!win) {
        return;
    }

    GList* win_list = g_hash_table_get_keys(win_to_task_table);
    int i;

    for (GList* it = win_list; it; it = it->next) {
        for (i = 0; i < num_results; i++) {
            if (*static_cast<Window*>(it->data) == win[i]) {
                break;
            }
        }

        if (i == num_results) {
            taskbar_remove_task(it->data, 0, 0);
        }
    }

    g_list_free(win_list);

    // Add any new
    for (i = 0; i < num_results; i++) {
        if (!task_get_task(win[i])) {
            add_task(win[i]);
        }
    }

    XFree(win);
}


void draw_taskbar(void* obj, cairo_t* c) {
    Taskbar* taskbar = static_cast<Taskbar*>(obj);
    int state = (taskbar->desktop == server.desktop ? TASKBAR_ACTIVE :
                 TASKBAR_NORMAL);
    taskbar->state_pix[state] = taskbar->pix;
}


int resize_taskbar(void* obj) {
    Taskbar* taskbar = static_cast<Taskbar*>(obj);
    Panel* panel = taskbar->panel;
    int text_width;

    //printf("resize_taskbar %d %d\n", taskbar->posx, taskbar->posy);
    if (panel_horizontal) {
        resize_by_layout(obj, panel->g_task.maximum_width);

        text_width = panel->g_task.maximum_width;
        auto it = taskbar->children.begin();

        if (taskbarname_enabled) {
            ++it;
        }

        if (it != taskbar->children.end()) {
            text_width = static_cast<Task*>(*it)->width;
        }

        taskbar->text_width = text_width - panel->g_task.text_posx -
                              panel->g_task.bg->border.width - panel->g_task.paddingx;
    } else {
        resize_by_layout(obj, panel->g_task.maximum_height);
        taskbar->text_width = taskbar->width - (2 * panel->g_taskbar.paddingy)
                              - panel->g_task.text_posx - panel->g_task.bg->border.width -
                              panel->g_task.paddingx;
    }

    return 0;
}


void on_change_taskbar(void* obj) {
    Taskbar* tskbar = static_cast<Taskbar*>(obj);

    // reset Pixmap when position/size changed
    for (int k = 0; k < TASKBAR_STATE_COUNT; ++k) {
        if (tskbar->state_pix[k]) {
            XFreePixmap(server.dsp, tskbar->state_pix[k]);
        }

        tskbar->state_pix[k] = 0;
    }

    tskbar->pix = 0;
    tskbar->redraw = 1;
}


void set_taskbar_state(Taskbar* tskbar, int state) {
    tskbar->bg = panel1[0].g_taskbar.background[state];
    tskbar->pix = tskbar->state_pix[state];

    if (taskbarname_enabled) {
        tskbar->bar_name.bg = panel1[0].g_taskbar.background_name[state];
        tskbar->bar_name.pix = tskbar->bar_name.state_pix[state];
    }

    if (panel_mode != MULTI_DESKTOP) {
        tskbar->on_screen = (state == TASKBAR_NORMAL ? 0 : 1);
    }

    if (tskbar->on_screen == 1) {
        if (tskbar->state_pix[state] == 0) {
            tskbar->redraw = 1;
        }

        if (taskbarname_enabled && tskbar->bar_name.state_pix[state] == 0) {
            tskbar->bar_name.redraw = 1;
        }

        if (panel_mode == MULTI_DESKTOP
            && panel1[0].g_taskbar.background[TASKBAR_NORMAL] !=
            panel1[0].g_taskbar.background[TASKBAR_ACTIVE]) {
            auto it = tskbar->children.begin();

            if (taskbarname_enabled) {
                ++it;
            }

            for (; it != tskbar->children.end(); ++it) {
                set_task_redraw(static_cast<Task*>(*it));
            }
        }
    }

    panel_refresh = 1;
}


void visible_taskbar(void* p) {
    Panel* panel = static_cast<Panel*>(p);

    for (int j = 0 ; j < panel->nb_desktop ; j++) {
        Taskbar* taskbar = &panel->taskbar[j];

        if (panel_mode != MULTI_DESKTOP && taskbar->desktop != server.desktop) {
            // SINGLE_DESKTOP and not current desktop
            taskbar->on_screen = 0;
        } else {
            taskbar->on_screen = 1;
        }
    }

    panel_refresh = 1;
}

