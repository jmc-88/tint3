#include "systray/tray_window.hh"

#include "server.hh"

TrayWindow::TrayWindow(Server* server, Window tray_id, Window child_id)
    : tray_id(tray_id),
      child_id(child_id),
      owned(false),
      x(0),
      y(0),
      width(0),
      height(0),
      hide(false),
      depth(0),
      damage(0),
      server_(server) {}

TrayWindow::~TrayWindow() {
  XSelectInput(server_->dsp, child_id, NoEventMask);

  if (!hide) {
    XUnmapWindow(server_->dsp, child_id);
  }

  if (damage != None) {
    XDamageDestroy(server_->dsp, damage);
  }

  XReparentWindow(server_->dsp, child_id, server_->root_window(), 0, 0);
  XDestroyWindow(server_->dsp, tray_id);
  XSync(server_->dsp, False);
}
