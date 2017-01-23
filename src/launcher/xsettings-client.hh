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

#ifndef TINT3_LAUNCHER_XSETTINGS_CLIENT_HH
#define TINT3_LAUNCHER_XSETTINGS_CLIENT_HH

#include <X11/Xlib.h>
#include "xsettings-common.hh"

#include "server.hh"

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

typedef struct _XSettingsClient XSettingsClient;

typedef enum {
  XSETTINGS_ACTION_NEW,
  XSETTINGS_ACTION_CHANGED,
  XSETTINGS_ACTION_DELETED
} XSettingsAction;

typedef void (*XSettingsNotifyFunc)(const char* name, XSettingsAction action,
                                    XSettingsSetting* setting, void* cb_data);
typedef void (*XSettingsWatchFunc)(Window window, Bool is_start, long mask,
                                   void* cb_data);

XSettingsClient* XSettingsClientNew(Server const& server,
                                    XSettingsNotifyFunc notify,
                                    XSettingsWatchFunc watch, void* cb_data);
void XSettingsClientDestroy(XSettingsClient* client);
Bool XSettingsClientProcessEvent(XSettingsClient* client, XEvent* xev);

void XSettingsNotifyCallback(const char* name, XSettingsAction action,
                             XSettingsSetting* setting, void* data);

XSettingsResult XSettingsClientGetSetting(XSettingsClient* client,
                                          const char* name,
                                          XSettingsSetting** setting);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // TINT3_LAUNCHER_XSETTINGS_CLIENT_HH
