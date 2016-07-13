#ifndef TINT3_SYSTRAYBAR_TRAY_WINDOW_H
#define TINT3_SYSTRAYBAR_TRAY_WINDOW_H

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

#include "util/timer.h"

class TrayWindow {
 public:
  TrayWindow(Window parent_id, Window tray_id);

  Window id;
  Window tray_id;
  int x, y;
  int width, height;
  // TODO: manage icon's show/hide
  bool hide;
  int depth;
  Damage damage;
  Interval* render_timeout;
};

#endif  // TINT3_SYSTRAYBAR_TRAY_WINDOW_H
