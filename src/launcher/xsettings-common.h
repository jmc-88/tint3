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
#ifndef XSETTINGS_COMMON_H
#define XSETTINGS_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Types of settings possible. Enum values correspond to
 * protocol values.
 */
typedef enum {
  XSETTINGS_TYPE_INT = 0,
  XSETTINGS_TYPE_STRING = 1,
  XSETTINGS_TYPE_COLOR = 2,
  XSETTINGS_TYPE_NONE = 0xff
} XSettingsType;

typedef enum {
  XSETTINGS_SUCCESS,
  XSETTINGS_NO_MEM,
  XSETTINGS_ACCESS,
  XSETTINGS_FAILED,
  XSETTINGS_NO_ENTRY,
  XSETTINGS_DUPLICATE_ENTRY
} XSettingsResult;

struct XSettingsColor {
  unsigned short red, green, blue, alpha;
};

struct XSettingsSetting {
  char* name;
  XSettingsType type;

  union {
    int v_int;
    char* v_string;
    XSettingsColor v_color;
  } data;

  unsigned long last_change_serial;
};

struct XSettingsList {
  XSettingsSetting* setting;
  XSettingsList* next;
};

struct XSettingsBuffer {
  char byte_order;
  size_t len;
  unsigned char* data;
  unsigned char* pos;
};

XSettingsSetting* XSettingsSettingCopy(XSettingsSetting* setting);
void XSettingsSettingFree(XSettingsSetting* setting);
bool XSettingsSettingEqual(XSettingsSetting* setting_a,
                           XSettingsSetting* setting_b);

void XSettingsListFree(XSettingsList* list);
XSettingsList* XSettingsListCopy(XSettingsList* list);
XSettingsResult XSettingsListInsert(XSettingsList** list,
                                    XSettingsSetting* setting);
XSettingsSetting* XSettingsListLookup(XSettingsList* list, const char* name);
XSettingsResult XSettingsListDelete(XSettingsList** list, const char* name);

char XSettingsByteOrder();

#define XSETTINGS_PAD(n, m) ((n + m - 1) & (~(m - 1)))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* XSETTINGS_COMMON_H */
