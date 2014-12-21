/*
 * Copyright © 2001 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Red Hat not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Red Hat makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL RED HAT
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Owen Taylor, Red Hat, Inc.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmd.h>        /* For CARD16 */

#include "xsettings-client.h"
#include "server.h"
#include "panel.h"
#include "launcher.h"
#include "util/log.h"

struct _XSettingsClient {
    Display* display;
    int screen;
    XSettingsNotifyFunc notify;
    XSettingsWatchFunc watch;
    void* cb_data;

    Window manager_window;
    XSettingsList* settings;
};


void XSettingsNotifyCallback(const char* name, XSettingsAction action,
                             XSettingsSetting* setting, void* data) {
    if ((action == XSETTINGS_ACTION_NEW || action == XSETTINGS_ACTION_CHANGED)
        && name != nullptr && setting != nullptr) {
        if (strcmp(name, "Net/IconThemeName") == 0
            && setting->type == XSETTINGS_TYPE_STRING) {
            if (!icon_theme_name.empty()) {
                if (icon_theme_name == setting->data.v_string) {
                    return;
                }
            }

            icon_theme_name = setting->data.v_string;

            for (int i = 0 ; i < nb_panel ; ++i) {
                Launcher& launcher = panel1[i].launcher_;
                launcher.CleanupTheme();
                launcher.LoadThemes();
                launcher.LoadIcons();
                launcher.need_resize_ = true;
            }
        }
    }
}


static void NotifyChanges(XSettingsClient* client, XSettingsList* old_list) {
    XSettingsList* old_iter = old_list;
    XSettingsList* new_iter = client->settings;

    if (!client->notify) {
        return;
    }

    while (old_iter || new_iter) {
        int cmp;

        if (old_iter && new_iter) {
            cmp = strcmp(old_iter->setting->name, new_iter->setting->name);
        } else if (old_iter) {
            cmp = -1;
        } else {
            cmp = 1;
        }

        if (cmp < 0) {
            client->notify(old_iter->setting->name, XSETTINGS_ACTION_DELETED, nullptr,
                           client->cb_data);
        } else if (cmp == 0) {
            if (!XSettingsSettingEqual(old_iter->setting, new_iter->setting)) {
                client->notify(old_iter->setting->name, XSETTINGS_ACTION_CHANGED,
                               new_iter->setting, client->cb_data);
            }
        } else {
            client->notify(new_iter->setting->name, XSETTINGS_ACTION_NEW,
                           new_iter->setting, client->cb_data);
        }

        if (old_iter) {
            old_iter = old_iter->next;
        }

        if (new_iter) {
            new_iter = new_iter->next;
        }
    }
}


static int IgnoreErrors(Display* display, XErrorEvent* event) {
    return True;
}

static char local_byte_order = '\0';

#define BYTES_LEFT(buffer) ((buffer)->data + (buffer)->len - (buffer)->pos)

static XSettingsResult FetchCard16(XSettingsBuffer* buffer, CARD16* result) {
    CARD16 x;

    if (BYTES_LEFT(buffer) < 2) {
        return XSETTINGS_ACCESS;
    }

    x = *(CARD16*)buffer->pos;
    buffer->pos += 2;

    if (buffer->byte_order == local_byte_order) {
        *result = x;
    } else {
        *result = (x << 8) | (x >> 8);
    }

    return XSETTINGS_SUCCESS;
}


static XSettingsResult FetchUshort(XSettingsBuffer* buffer,
                                   unsigned short*  result) {
    CARD16 x;
    XSettingsResult r;

    r = FetchCard16(buffer, &x);

    if (r == XSETTINGS_SUCCESS) {
        *result = x;
    }

    return r;
}


static XSettingsResult FetchCard32(XSettingsBuffer* buffer, CARD32* result) {
    CARD32 x;

    if (BYTES_LEFT(buffer) < 4) {
        return XSETTINGS_ACCESS;
    }

    x = *(CARD32*)buffer->pos;
    buffer->pos += 4;

    if (buffer->byte_order == local_byte_order) {
        *result = x;
    } else {
        *result = (x << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | (x >> 24);
    }

    return XSETTINGS_SUCCESS;
}

static XSettingsResult FetchCard8(XSettingsBuffer* buffer, CARD8* result) {
    if (BYTES_LEFT(buffer) < 1) {
        return XSETTINGS_ACCESS;
    }

    *result = *(CARD8*)buffer->pos;
    buffer->pos += 1;

    return XSETTINGS_SUCCESS;
}

#define XSETTINGS_PAD(n,m) ((n + m - 1) & (~(m-1)))

static XSettingsList* ParseSettings(unsigned char* data, size_t len) {
    XSettingsBuffer buffer;
    XSettingsResult result = XSETTINGS_SUCCESS;
    XSettingsList* settings = nullptr;
    CARD32 serial;
    CARD32 n_entries;
    CARD32 i;
    XSettingsSetting* setting = nullptr;

    local_byte_order = XSettingsByteOrder();

    buffer.pos = buffer.data = data;
    buffer.len = len;

    result = FetchCard8(&buffer, (CARD8*)&buffer.byte_order);

    if (buffer.byte_order != MSBFirst && buffer.byte_order != LSBFirst) {
        util::log::Error()
                << "Invalid byte order "
                << buffer.byte_order
                << " in XSETTINGS property\n";
        result = XSETTINGS_FAILED;
        goto out;
    }

    buffer.pos += 3;

    result = FetchCard32(&buffer, &serial);

    if (result != XSETTINGS_SUCCESS) {
        goto out;
    }

    result = FetchCard32(&buffer, &n_entries);

    if (result != XSETTINGS_SUCCESS) {
        goto out;
    }

    for (i = 0; i < n_entries; i++) {
        CARD8 type;
        CARD16 name_len;
        CARD32 v_int;

        result = FetchCard8(&buffer, &type);

        if (result != XSETTINGS_SUCCESS) {
            goto out;
        }

        buffer.pos += 1;

        result = FetchCard16(&buffer, &name_len);

        if (result != XSETTINGS_SUCCESS) {
            goto out;
        }

        int pad_len = XSETTINGS_PAD(name_len, 4);

        if (BYTES_LEFT(&buffer) < pad_len) {
            result = XSETTINGS_ACCESS;
            goto out;
        }

        setting = (XSettingsSetting*) malloc(sizeof * setting);

        if (!setting) {
            result = XSETTINGS_NO_MEM;
            goto out;
        }

        setting->type = XSETTINGS_TYPE_INT; /* No allocated memory */

        setting->name = (char*) malloc(name_len + 1);

        if (!setting->name) {
            result = XSETTINGS_NO_MEM;
            goto out;
        }

        memcpy(setting->name, buffer.pos, name_len);
        setting->name[name_len] = '\0';
        buffer.pos += pad_len;

        result = FetchCard32(&buffer, &v_int);

        if (result != XSETTINGS_SUCCESS) {
            goto out;
        }

        setting->last_change_serial = v_int;

        switch (type) {
            case XSETTINGS_TYPE_INT:
                result = FetchCard32(&buffer, &v_int);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                setting->data.v_int = (INT32)v_int;
                break;

            case XSETTINGS_TYPE_STRING:
                result = FetchCard32(&buffer, &v_int);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                pad_len = XSETTINGS_PAD(v_int, 4);

                if (v_int + 1 == 0 || /* Guard against wrap-around */
                    BYTES_LEFT(&buffer) < pad_len) {
                    result = XSETTINGS_ACCESS;
                    goto out;
                }

                setting->data.v_string = (char*) malloc(v_int + 1);

                if (!setting->data.v_string) {
                    result = XSETTINGS_NO_MEM;
                    goto out;
                }

                memcpy(setting->data.v_string, buffer.pos, v_int);
                setting->data.v_string[v_int] = '\0';
                buffer.pos += pad_len;
                break;

            case XSETTINGS_TYPE_COLOR:
                result = FetchUshort(&buffer, &setting->data.v_color.red);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                result = FetchUshort(&buffer, &setting->data.v_color.green);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                result = FetchUshort(&buffer, &setting->data.v_color.blue);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                result = FetchUshort(&buffer, &setting->data.v_color.alpha);

                if (result != XSETTINGS_SUCCESS) {
                    goto out;
                }

                break;

            default:
                /* Quietly ignore unknown types */
                break;
        }

        setting->type = static_cast<XSettingsType>(type);

        result = XSettingsListInsert(&settings, setting);

        if (result != XSETTINGS_SUCCESS) {
            goto out;
        }

        setting = nullptr;
    }

out:

    if (result != XSETTINGS_SUCCESS) {
        switch (result) {
            case XSETTINGS_NO_MEM:
                util::log::Error() << "Out of memory reading XSETTINGS property\n";
                break;

            case XSETTINGS_ACCESS:
                util::log::Error() << "Invalid XSETTINGS property (read off end)\n";
                break;

            case XSETTINGS_DUPLICATE_ENTRY:
                util::log::Error() << "Duplicate XSETTINGS entry for '" << setting->name <<
                                   "'\n";

            case XSETTINGS_FAILED:
            case XSETTINGS_SUCCESS:
            case XSETTINGS_NO_ENTRY:
                break;
        }

        if (setting) {
            XSettingsSettingFree(setting);
        }

        XSettingsListFree(settings);
        settings = nullptr;
    }

    return settings;
}


static void ReadSettings(XSettingsClient* client) {
    Atom type;
    int format;
    unsigned long n_items;
    unsigned long bytes_after;
    unsigned char* data;
    int result;

    int (*old_handler)(Display*, XErrorEvent*);

    XSettingsList* old_list = client->settings;
    client->settings = nullptr;

    old_handler = XSetErrorHandler(IgnoreErrors);
    result = XGetWindowProperty(client->display, client->manager_window,
                                server.atoms_["_XSETTINGS_SETTINGS"], 0, LONG_MAX, False,
                                server.atoms_["_XSETTINGS_SETTINGS"], &type, &format, &n_items, &bytes_after,
                                &data);
    XSetErrorHandler(old_handler);

    if (result == Success && type == server.atoms_["_XSETTINGS_SETTINGS"]) {
        if (format != 8) {
            util::log::Error() << "Invalid format for XSETTINGS property " << format;
        } else {
            client->settings = ParseSettings(data, n_items);
        }

        XFree(data);
    }

    NotifyChanges(client, old_list);
    XSettingsListFree(old_list);
}


static void CheckManagerWindow(XSettingsClient* client) {
    if (client->manager_window && client->watch) {
        client->watch(client->manager_window, False, 0, client->cb_data);
    }

    XGrabServer(client->display);

    client->manager_window = XGetSelectionOwner(server.dsp,
                             server.atoms_["_XSETTINGS_SCREEN"]);

    if (client->manager_window) {
        XSelectInput(server.dsp, client->manager_window,
                     PropertyChangeMask | StructureNotifyMask);
    }

    XUngrabServer(client->display);
    XFlush(client->display);

    if (client->manager_window && client->watch) {
        client->watch(client->manager_window, True,
                      PropertyChangeMask | StructureNotifyMask, client->cb_data);
    }

    ReadSettings(client);
}


XSettingsClient* XSettingsClientNew(Display* display, int screen,
                                    XSettingsNotifyFunc notify, XSettingsWatchFunc watch, void* cb_data) {
    XSettingsClient* client = (XSettingsClient*) std::malloc(sizeof(*client));

    if (!client) {
        return nullptr;
    }

    client->display = display;
    client->screen = screen;
    client->notify = notify;
    client->watch = watch;
    client->cb_data = cb_data;

    client->manager_window = None;
    client->settings = nullptr;

    if (client->watch) {
        client->watch(RootWindow(display, screen), True, StructureNotifyMask,
                      client->cb_data);
    }

    CheckManagerWindow(client);

    if (client->manager_window == None) {
        printf("NO XSETTINGS manager, tint3 use config 'launcher_icon_theme'.\n");
        free(client);
        return nullptr;
    }

    return client;
}


void XSettingsClientDestroy(XSettingsClient* client) {
    if (client->watch) {
        client->watch(RootWindow(client->display, client->screen), False, 0,
                      client->cb_data);
    }

    if (client->manager_window && client->watch) {
        client->watch(client->manager_window, False, 0, client->cb_data);
    }

    XSettingsListFree(client->settings);
    free(client);
}


XSettingsResult XSettingsClientGetSetting(XSettingsClient* client,
        const char* name, XSettingsSetting** setting) {
    XSettingsSetting* search = XSettingsListLookup(client->settings, name);

    if (search) {
        *setting = XSettingsSettingCopy(search);
        return *setting ? XSETTINGS_SUCCESS : XSETTINGS_NO_MEM;
    } else {
        return XSETTINGS_NO_ENTRY;
    }
}


Bool XSettingsClientProcessEvent(XSettingsClient* client, XEvent* xev) {
    /* The checks here will not unlikely cause us to reread
    * the properties from the manager window a number of
    * times when the manager changes from A->B. But manager changes
    * are going to be pretty rare.
    */
    if (xev->xany.window == RootWindow(server.dsp, server.screen)) {
        if (xev->xany.type == ClientMessage
            && xev->xclient.message_type == server.atoms_["MANAGER"]) {
            CheckManagerWindow(client);
            return True;
        }
    } else if (xev->xany.window == client->manager_window) {
        if (xev->xany.type == DestroyNotify) {
            CheckManagerWindow(client);
            return True;
        } else if (xev->xany.type == PropertyNotify) {
            ReadSettings(client);
            return True;
        }
    }

    return False;
}

