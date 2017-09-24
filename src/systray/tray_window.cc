#include "systray/tray_window.hh"

TrayWindow::TrayWindow(Window parent_id, Window child_id)
    : id(parent_id),
      child_id(child_id),
      x(0),
      y(0),
      width(0),
      height(0),
      hide(false),
      depth(0),
      damage(0) {}
