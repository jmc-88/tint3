/**************************************************************************
*
* Tint3 : task
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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
#include <unistd.h>

#include <algorithm>

#include "task.h"
#include "taskbar.h"
#include "server.h"
#include "panel.h"
#include "tooltip.h"
#include "timer.h"
#include "util/window.h"

namespace {

const char kUntitled[] = "Untitled";

} // namespace

Timeout* urgent_timeout;
std::list<Task*> urgent_list;

std::string Task::GetTooltipText() {
    return tooltip_enabled_ ? title_ : std::string();
}

Task& Task::SetTooltipEnabled(bool is_enabled) {
    tooltip_enabled_ = is_enabled;
    return (*this);
}


Task* add_task(Window win) {
    if (!win) {
        return 0;
    }

    if (window_is_hidden(win)) {
        return 0;
    }

    int monitor;

    if (nb_panel > 1) {
        monitor = window_get_monitor(win);

        if (monitor >= nb_panel) {
            monitor = 0;
        }
    } else {
        monitor = 0;
    }

    Task new_tsk;
    new_tsk.win = win;
    new_tsk.desktop = server.GetDesktopFromWindow(win);
    new_tsk.panel = &panel1[monitor];
    new_tsk.current_state = window_is_iconified(win) ? TASK_ICONIFIED : TASK_NORMAL;

    // allocate only one title and one icon
    // even with task_on_all_desktop and with task_on_all_panel
    for (int k = 0; k < TASK_STATE_COUNT; ++k) {
        new_tsk.icon[k] = 0;
        new_tsk.state_pix[k] = 0;
    }

    new_tsk.UpdateTitle();
    get_icon(&new_tsk);

    //printf("task %s : desktop %d, monitor %d\n", new_tsk->title, desktop, monitor);
    XSelectInput(server.dsp, new_tsk.win,
                 PropertyChangeMask | StructureNotifyMask);

    GPtrArray* task_group = g_ptr_array_new();
    Task* new_tsk2 = 0;

    for (int j = 0 ; j < panel1[monitor].nb_desktop ; j++) {
        if (new_tsk.desktop != ALLDESKTOP && new_tsk.desktop != j) {
            continue;
        }

        Taskbar* tskbar = &panel1[monitor].taskbar[j];
        new_tsk2 = new Task();

        // TODO: nuke this from planet Earth ASAP - horrible hack to mimick the
        // original memcpy() call
        new_tsk2->Clone(panel1[monitor].g_task);

        new_tsk2->parent = reinterpret_cast<Area*>(tskbar);
        new_tsk2->win = new_tsk.win;
        new_tsk2->desktop = new_tsk.desktop;

        // to update the current state later in set_task_state...
        new_tsk2->current_state = -1;

        if (new_tsk2->desktop == ALLDESKTOP && server.desktop != j) {
            // hide ALLDESKTOP task on non-current desktop
            new_tsk2->on_screen = 0;
        }

        new_tsk2->SetTitle(new_tsk.GetTitle());
        new_tsk2->SetTooltipEnabled(panel1[monitor].g_task.tooltip_enabled);

        for (int k = 0; k < TASK_STATE_COUNT; ++k) {
            new_tsk2->icon[k] = new_tsk.icon[k];
            new_tsk2->state_pix[k] = 0;
        }

        new_tsk2->icon_width = new_tsk.icon_width;
        new_tsk2->icon_height = new_tsk.icon_height;
        tskbar->children.push_back(new_tsk2);
        tskbar->need_resize = true;
        g_ptr_array_add(task_group, new_tsk2);
        //printf("add_task panel %d, desktop %d, task %s\n", i, j, new_tsk2->title);
    }

    auto key = (Window*) malloc(sizeof(Window));
    (*key) = new_tsk.win;
    g_hash_table_insert(win_to_task_table, key, task_group);
    set_task_state(new_tsk2, new_tsk.current_state);

    if (window_is_urgent(win)) {
        add_urgent(new_tsk2);
    }

    return new_tsk2;
}


void remove_task(Task* tsk) {
    if (!tsk) {
        return;
    }

    // free title and icon just for the first task
    // even with task_on_all_desktop and with task_on_all_panel
    //printf("remove_task %s %d\n", tsk->title, tsk->desktop);
    tsk->SetTitle("");

    for (int k = 0; k < TASK_STATE_COUNT; ++k) {
        if (tsk->icon[k]) {
            imlib_context_set_image(tsk->icon[k]);
            imlib_free_image();
            tsk->icon[k] = 0;

            if (tsk->state_pix[k]) {
                XFreePixmap(server.dsp, tsk->state_pix[k]);
            }
        }
    }

    Window win = tsk->win;
    GPtrArray* task_group = (GPtrArray*) g_hash_table_lookup(win_to_task_table,
                            &win);

    for (size_t i = 0; i < task_group->len; ++i) {
        auto tsk2 = static_cast<Task*>(g_ptr_array_index(task_group, i));
        auto tskbar = reinterpret_cast<Taskbar*>(tsk2->parent);

        auto tsk2_iter = std::find(tskbar->children.begin(),
                                   tskbar->children.end(),
                                   tsk2);

        if (tsk2_iter != tskbar->children.end()) {
            tskbar->children.erase(tsk2_iter);
        }

        tskbar->need_resize = true;

        if (tsk2 == task_active) {
            task_active = 0;
        }

        if (tsk2 == task_drag) {
            task_drag = 0;
        }

        auto it = std::find(urgent_list.begin(),
                            urgent_list.end(),
                            tsk2);

        if (it != urgent_list.end()) {
            del_urgent(tsk2);
        }

        delete tsk2;
    }

    g_hash_table_remove(win_to_task_table, &win);
}


bool Task::UpdateTitle() {
    if (!panel->g_task.text && !panel->g_task.tooltip_enabled) {
        return false;
    }

    const char* name = (char*) ServerGetProperty(win,
                       server.atom._NET_WM_VISIBLE_NAME,
                       server.atom.UTF8_STRING,
                       0);

    if (!name || *name == '\0') {
        name = (char*) ServerGetProperty(win,
                                         server.atom._NET_WM_NAME,
                                         server.atom.UTF8_STRING,
                                         0);
    }

    if (!name || *name == '\0') {
        name = (char*) ServerGetProperty(win,
                                         server.atom.WM_NAME,
                                         XA_STRING,
                                         0);
    }

    if (!name || *name == '\0') {
        name = kUntitled;
    }

    // add space before title
    char* title = (char*) malloc(strlen(name) + 2);

    if (panel->g_task.icon) {
        strcpy(title, " ");
    } else {
        title[0] = '\0';
    }

    strcat(title, name);

    if (name != nullptr && name != kUntitled) {
        XFree((void*) name);
    }

    // check unecessary title change
    if (title_ == title) {
        free(title);
        return false;
    }

    title_ = title;
    GPtrArray* task_group = task_get_tasks(win);

    if (task_group) {
        for (size_t i = 0; i < task_group->len; ++i) {
            auto tsk2 = static_cast<Task*>(g_ptr_array_index(task_group, i));
            tsk2->title_ = title_;
            set_task_redraw(tsk2);
        }
    }

    return true;
}


std::string Task::GetTitle() const {
    return title_;
}


void Task::SetTitle(std::string const& title) {
    title_.assign(title);
}


void get_icon(Task* tsk) {
    Panel* panel = tsk->panel;

    if (!panel->g_task.icon) {
        return;
    }

    int k;

    for (k = 0; k < TASK_STATE_COUNT; ++k) {
        if (tsk->icon[k]) {
            imlib_context_set_image(tsk->icon[k]);
            imlib_free_image();
            tsk->icon[k] = 0;
        }
    }

    Imlib_Image img = nullptr;
    XWMHints* hints = 0;
    int i;
    gulong* data = (gulong*) ServerGetProperty(tsk->win, server.atom._NET_WM_ICON,
                   XA_CARDINAL, &i);

    if (data) {
        // get ARGB icon
        int w, h;
        gulong* tmp_data = get_best_icon(data, get_icon_count(data, i), i, &w, &h,
                                         panel->g_task.icon_size1);
#ifdef __x86_64__
        DATA32 icon_data[w * h];
        int length = w * h;

        for (i = 0; i < length; ++i) {
            icon_data[i] =  tmp_data[i];
        }

        img = imlib_create_image_using_copied_data(w, h, icon_data);
#else
        img = imlib_create_image_using_data(w, h, (DATA32*)tmp_data);
#endif
    } else {
        // get Pixmap icon
        hints = XGetWMHints(server.dsp, tsk->win);

        if (hints) {
            if (hints->flags & IconPixmapHint && hints->icon_pixmap != 0) {
                // get width, height and depth for the pixmap
                Window root;
                int  icon_x, icon_y;
                uint border_width, bpp;
                uint w, h;

                //printf("  get pixmap\n");
                XGetGeometry(server.dsp, hints->icon_pixmap, &root, &icon_x, &icon_y, &w, &h,
                             &border_width, &bpp);
                imlib_context_set_drawable(hints->icon_pixmap);
                img = imlib_create_image_from_drawable(hints->icon_mask, 0, 0, w, h, 0);
            }
        }
    }

    if (img == nullptr) {
        imlib_context_set_image(default_icon);
        img = imlib_clone_image();
    }

    // transform icons
    imlib_context_set_image(img);
    imlib_image_set_has_alpha(1);

    int w = imlib_image_get_width();
    int h = imlib_image_get_height();
    Imlib_Image orig_image = imlib_create_cropped_scaled_image(0, 0, w, h,
                             panel->g_task.icon_size1, panel->g_task.icon_size1);
    imlib_free_image();

    imlib_context_set_image(orig_image);
    tsk->icon_width = imlib_image_get_width();
    tsk->icon_height = imlib_image_get_height();

    for (k = 0; k < TASK_STATE_COUNT; ++k) {
        imlib_context_set_image(orig_image);
        tsk->icon[k] = imlib_clone_image();
        imlib_context_set_image(tsk->icon[k]);
        DATA32* data32;

        if (panel->g_task.alpha[k] != 100 || panel->g_task.saturation[k] != 0
            || panel->g_task.brightness[k] != 0) {
            data32 = imlib_image_get_data();
            AdjustAsb(data32, tsk->icon_width, tsk->icon_height, panel->g_task.alpha[k],
                      (float)panel->g_task.saturation[k] / 100,
                      (float)panel->g_task.brightness[k] / 100);
            imlib_image_put_back_data(data32);
        }
    }

    imlib_context_set_image(orig_image);
    imlib_free_image();

    if (hints) {
        XFree(hints);
    }

    if (data) {
        XFree(data);
    }

    GPtrArray* task_group = task_get_tasks(tsk->win);

    if (task_group) {
        for (i = 0; i < task_group->len; ++i) {
            auto tsk2 = static_cast<Task*>(g_ptr_array_index(task_group, i));
            tsk2->icon_width = tsk->icon_width;
            tsk2->icon_height = tsk->icon_height;

            for (k = 0; k < TASK_STATE_COUNT; ++k) {
                tsk2->icon[k] = tsk->icon[k];
            }

            set_task_redraw(tsk2);
        }
    }
}


void Task::DrawIcon(int text_width) {
    if (icon[current_state] == 0) {
        return;
    }

    // Find pos
    int pos_x;

    if (panel->g_task.centered) {
        if (panel->g_task.text) {
            pos_x = (width - text_width - panel->g_task.icon_size1) / 2;
        } else {
            pos_x = (width - panel->g_task.icon_size1) / 2;
        }
    } else {
        pos_x = panel->g_task.paddingxlr + bg->border.width;
    }

    // Render
    imlib_context_set_image(icon[current_state]);

    if (server.real_transparency) {
        RenderImage(pix, pos_x, panel->g_task.icon_posy,
                    imlib_image_get_width(), imlib_image_get_height());
    } else {
        imlib_context_set_drawable(pix);
        imlib_render_image_on_drawable(pos_x, panel->g_task.icon_posy);
    }
}


void Task::DrawForeground(cairo_t* c) {
    state_pix[current_state] = pix;

    int width = 0;
    int height = 0;
    //printf("draw_task %d %d\n", posx, posy);

    if (panel->g_task.text) {
        /* Layout */
        PangoLayout* layout = pango_cairo_create_layout(c);
        pango_layout_set_font_description(layout, panel->g_task.font_desc);
        pango_layout_set_text(layout, title_.c_str(), -1);

        /* Drawing width and Cut text */
        // pango use U+22EF or U+2026
        pango_layout_set_width(layout,
                               ((Taskbar*)parent)->text_width * PANGO_SCALE);
        pango_layout_set_height(layout, panel->g_task.text_height * PANGO_SCALE);
        pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);
        pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

        /* Center text */
        if (panel->g_task.centered) {
            pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        } else {
            pango_layout_set_alignment(layout, PANGO_ALIGN_LEFT);
        }

        pango_layout_get_pixel_size(layout, &width, &height);

        Color* config_text = &panel->g_task.font[current_state];
        cairo_set_source_rgba(c, config_text->color[0], config_text->color[1],
                              config_text->color[2], config_text->alpha);

        pango_cairo_update_layout(c, layout);
        double text_posy = (panel->g_task.height - height) / 2.0;
        cairo_move_to(c, panel->g_task.text_posx, text_posy);
        pango_cairo_show_layout(c, layout);

        if (panel->g_task.font_shadow) {
            cairo_set_source_rgba(c, 0.0, 0.0, 0.0, 0.5);
            pango_cairo_update_layout(c, layout);
            cairo_move_to(c, panel->g_task.text_posx + 1, text_posy + 1);
            pango_cairo_show_layout(c, layout);
        }

        g_object_unref(layout);
    }

    if (panel->g_task.icon) {
        DrawIcon(width);
    }
}


void Task::OnChangeLayout() {
    long value[] = {
        panel->posx + posx,
        panel->posy + posy,
        width,
        height
    };

    XChangeProperty(server.dsp, win, server.atom._NET_WM_ICON_GEOMETRY,
                    XA_CARDINAL, 32, PropModeReplace, (unsigned char*)value, 4);

    // reset Pixmap when position/size changed
    set_task_redraw(this);
}

// Given a pointer to the active task (active_task) and a pointer
// to the task that is currently under the mouse (current_task),
// return a pointer to the active task that is on the same desktop
// as current_task. Normally this is simply active_task, except when
// it is set to appear on all desktops. In that case we search for
// another Task on current_task's taskbar, with the same window as
// active_task.
Task* find_active_task(Task* current_task, Task* active_task) {
    if (active_task == 0) {
        return current_task;
    }

    if (active_task->desktop != ALLDESKTOP) {
        return active_task;
    }

    if (current_task == 0) {
        return active_task;
    }

    Taskbar* tskbar = reinterpret_cast<Taskbar*>(current_task->parent);
    auto it = tskbar->children.begin();

    if (taskbarname_enabled) {
        ++it;
    }

    for (; it != tskbar->children.end(); ++it) {
        auto tsk = static_cast<Task*>(*it);

        if (tsk->win == active_task->win) {
            return tsk;
        }
    }

    return active_task;
}

Task* next_task(Task* tsk) {
    if (tsk == nullptr) {
        return nullptr;
    }

    auto tskbar = reinterpret_cast<Taskbar*>(tsk->parent);
    auto first = tskbar->children.begin();

    if (taskbarname_enabled) {
        ++first;
    }

    auto it = std::find(first, tskbar->children.end(), tsk);

    if (it == tskbar->children.end()) {
        return nullptr;
    }

    if (++it == tskbar->children.end()) {
        return static_cast<Task*>(*it);
    }

    return static_cast<Task*>(*first);
}

Task* prev_task(Task* tsk) {
    if (tsk == nullptr) {
        return nullptr;
    }

    auto tskbar = reinterpret_cast<Taskbar*>(tsk->parent);
    auto first = tskbar->children.begin();

    if (taskbarname_enabled) {
        ++first;
    }

    auto it = std::find(tskbar->children.begin(), tskbar->children.end(), tsk);

    if (it == tskbar->children.end()) {
        return nullptr;
    }

    if (it-- == first) {
        return static_cast<Task*>(tskbar->children.back());
    }

    return static_cast<Task*>(*it);
}


void active_task() {
    if (task_active) {
        set_task_state(task_active,
                       window_is_iconified(task_active->win) ? TASK_ICONIFIED : TASK_NORMAL);
        task_active = 0;
    }

    Window w1 = window_get_active();
    //printf("Change active task %ld\n", w1);

    if (w1) {
        if (!task_get_tasks(w1)) {
            Window w2;

            while (XGetTransientForHint(server.dsp, w1, &w2)) {
                w1 = w2;
            }
        }

        set_task_state((task_active = task_get_task(w1)), TASK_ACTIVE);
    }
}


void set_task_state(Task* tsk, int state) {
    if (tsk == 0 || state < 0 || state >= TASK_STATE_COUNT) {
        return;
    }

    if (tsk->current_state != state) {
        GPtrArray* task_group = task_get_tasks(tsk->win);

        if (task_group) {
            for (size_t i = 0; i < task_group->len; ++i) {
                auto tsk1 = static_cast<Task*>(g_ptr_array_index(task_group, i));
                tsk1->current_state = state;
                tsk1->bg = panel1[0].g_task.background[state];
                tsk1->pix = tsk1->state_pix[state];

                if (tsk1->state_pix[state] == 0) {
                    tsk1->need_redraw = true;
                }

                auto it = std::find(urgent_list.begin(),
                                    urgent_list.end(),
                                    tsk1);

                if (state == TASK_ACTIVE && it != urgent_list.end()) {
                    del_urgent(tsk1);
                }
            }

            panel_refresh = 1;
        }
    }
}


void set_task_redraw(Task* tsk) {
    int k;

    for (k = 0; k < TASK_STATE_COUNT; ++k) {
        if (tsk->state_pix[k]) {
            XFreePixmap(server.dsp, tsk->state_pix[k]);
        }

        tsk->state_pix[k] = 0;
    }

    tsk->pix = 0;
    tsk->need_redraw = true;
}


void blink_urgent(void* arg) {
    for (auto& t : urgent_list) {
        if (t->urgent_tick < max_tick_urgent) {
            if (t->urgent_tick++ % 2) {
                set_task_state(t, TASK_URGENT);
            } else {
                set_task_state(t, window_is_iconified(t->win) ? TASK_ICONIFIED : TASK_NORMAL);
            }
        }
    }

    panel_refresh = 1;
}


void add_urgent(Task* tsk) {
    if (!tsk) {
        return;
    }

    // some programs set urgency hint although they are active
    if (task_active && task_active->win == tsk->win) {
        return;
    }

    // always add the first tsk for a task group (omnipresent windows)
    tsk = task_get_task(tsk->win);
    tsk->urgent_tick = 0;

    auto it = std::find(urgent_list.begin(),
                        urgent_list.end(),
                        tsk);

    if (it == urgent_list.end()) {
        // not yet in the list, so we have to add it
        urgent_list.push_front(tsk);

        if (urgent_timeout == 0) {
            urgent_timeout = add_timeout(10, 1000, blink_urgent, 0);
        }
    }
}


void del_urgent(Task* tsk) {
    urgent_list.erase(std::remove(urgent_list.begin(), urgent_list.end(), tsk),
                      urgent_list.end());

    if (urgent_list.empty()) {
        stop_timeout(urgent_timeout);
        urgent_timeout = 0;
    }
}
