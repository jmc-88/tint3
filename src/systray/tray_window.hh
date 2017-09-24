#ifndef TINT3_SYSTRAYBAR_TRAY_WINDOW_HH
#define TINT3_SYSTRAYBAR_TRAY_WINDOW_HH

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>

#include "util/timer.hh"

// forward declaration
class Server;

class TrayWindow {
 public:
  TrayWindow(Server* server, Window tray_id, Window child_id);
  ~TrayWindow();

  Window tray_id;
  Window child_id;
  bool owned;
  int x, y;
  int width, height;
  // TODO: manage icon's show/hide
  bool hide;
  int depth;
  Damage damage;
  Interval::Id render_timeout;

 private:
  Server* server_;
};

#endif  // TINT3_SYSTRAYBAR_TRAY_WINDOW_HH
