#include <X11/Xutil.h>

#include "panel.hh"
#include "server.hh"
#include "util/log.hh"
#include "util/x11.hh"

#include <cstring>

// For waitpid
#include <sys/types.h>
#include <sys/wait.h>

// For select, pipe and fcntl
#include <fcntl.h>
#include <sys/select.h>
#include <unistd.h>

int signal_pending = 0;
bool pending_children = false;

namespace util {
namespace x11 {

ScopedErrorHandler::ScopedErrorHandler(XErrorHandler new_handler)
    : old_handler_(XSetErrorHandler(new_handler)) {}

ScopedErrorHandler::~ScopedErrorHandler() { XSetErrorHandler(old_handler_); }

void XFreeDeleter::operator()(void* data) const { XFree(data); }

EventLoop::EventLoop(Server const* const server, Timer& timer)
    : alive_(true),
      server_(server),
      x11_file_descriptor_(ConnectionNumber(server_->dsp)),
      timer_(timer) {
  if (!self_pipe_.IsAlive()) {
    alive_ = false;
    return;
  }
}

bool EventLoop::IsAlive() const { return alive_; }

bool EventLoop::RunLoop() {
  bool hidden_dnd = true;

  while (true) {
    if (panel_refresh) {
      panel_refresh = false;

      for (Panel& panel : panels) {
        if (panel.hidden()) {
          XCopyArea(server_->dsp, panel.hidden_pixmap_, panel.main_win_,
                    server_->gc, 0, 0, panel.hidden_width_,
                    panel.hidden_height_, 0, 0);
          XSetWindowBackgroundPixmap(server_->dsp, panel.main_win_,
                                     panel.hidden_pixmap_);
        } else {
          if (panel.temp_pmap) {
            XFreePixmap(server_->dsp, panel.temp_pmap);
          }

          panel.temp_pmap =
              XCreatePixmap(server_->dsp, server_->root_window(), panel.width_,
                            panel.height_, server_->depth);
          panel.Render();
          XCopyArea(server_->dsp, panel.temp_pmap, panel.main_win_, server_->gc,
                    0, 0, panel.width_, panel.height_, 0, 0);
        }
      }

      XFlush(server_->dsp);

      Panel* panel = systray.panel_;

      if (systray.should_refresh() && panel != nullptr && !panel->hidden()) {
        systray.set_should_refresh(false);
        // tint3 doesn't draw systray icons. it just redraw background.
        XSetWindowBackgroundPixmap(server_->dsp, panel->main_win_,
                                   panel->temp_pmap);
        // force refresh of the icons
        systray.RefreshIcons(timer_);
      }
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(x11_file_descriptor_, &fdset);
    FD_SET(self_pipe_.ReadEnd(), &fdset);

    int max_fd_ = std::max(x11_file_descriptor_, self_pipe_.ReadEnd());

    auto next_interval = timer_.GetNextInterval();
    std::unique_ptr<struct timeval> next_timeval;

    if (next_interval) {
      auto now = timer_.Now();
      auto until = next_interval.Unwrap().GetTimePoint();

      Duration duration{until - now};
      next_timeval = ToTimeval(duration);
    }

    if (XPending(server_->dsp) ||
        select(max_fd_ + 1, &fdset, 0, 0, next_timeval.get()) > 0) {
      // Remove bytes written by WakeUp()
      if (FD_ISSET(self_pipe_.ReadEnd(), &fdset)) {
        self_pipe_.ReadPendingBytes();
      }

      if (pending_children) {
        ReapChildPIDs();
      }

      while (XPending(server_->dsp)) {
        XEvent e;
        XNextEvent(server_->dsp, &e);

#if HAVE_SN
        sn_display_process_event(server_->sn_dsp, &e);
#endif  // HAVE_SN

        Panel* panel = GetPanel(e.xany.window);

        if (panel != nullptr && panel_autohide) {
          if (e.type == EnterNotify) {
            AutohideTriggerShow(panel, timer_);
          } else if (e.type == LeaveNotify) {
            AutohideTriggerHide(panel, timer_);
          }

          auto XdndPosition = server_->atom("XdndPosition");
          auto XdndLeave = server_->atom("XdndLeave");

          if (panel->hidden()) {
            if (e.type == ClientMessage &&
                e.xclient.message_type == XdndPosition) {
              hidden_dnd = true;
              panel->AutohideShow();
            } else {
              // discard further processing of this event because
              // the panel is not visible yet
              continue;
            }
          } else if (hidden_dnd && e.type == ClientMessage &&
                     e.xclient.message_type == XdndLeave) {
            hidden_dnd = false;
            panel->AutohideHide();
          }
        }

        auto it = handler_map_.find(e.type);

        if (it != handler_map_.end()) {
          if (it->second != nullptr) {
            it->second(e);
          }
        }
      }
    }

    timer_.ProcessExpiredIntervals();

    if (signal_pending) {
      // Handle incoming signals:
      //
      //  * SIGUSR1 is used in case of user-sent signal, or some X11 events, and
      //  causes tint3 to reload.
      //
      //  * SIGUSR2 is sent from the user to trigger a complete restart of tint3
      // (replacing this process's contents with a new instance of self, useful
      // in cases where the binary itself has change, for instance because of a
      // package upgrade).
      //
      //  * anything else results in an unsuccessful process termination.

      FD_ZERO(&fdset);
      return (signal_pending == SIGUSR1 || signal_pending == SIGUSR2);
    }
  }

  return false;
}

void EventLoop::WakeUp() { self_pipe_.WriteOneByte(); }

EventLoop& EventLoop::RegisterHandler(int event,
                                      EventLoop::EventHandler handler) {
  handler_map_[event] = std::move(handler);
  return (*this);
}

EventLoop& EventLoop::RegisterHandler(std::initializer_list<int> event_list,
                                      EventLoop::EventHandler handler) {
  for (auto event : event_list) {
    RegisterHandler(event, handler);
  }

  return (*this);
}

void EventLoop::ReapChildPIDs() const {
  pid_t pid;
  while ((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {
#ifdef HAVE_SN
    auto it = server.pids.find(pid);
    if (it != server.pids.end()) {
      it->second.Complete();
      server.pids.erase(it);
    }
#endif  // HAVE_SN
  }
}

bool GetWMName(Display* display, Window window, std::string* output) {
  XTextProperty wm_name_prop;
  if (!XGetWMName(display, window, &wm_name_prop)) {
    return false;
  }

  output->assign(reinterpret_cast<const char*>(wm_name_prop.value));
  XFree(wm_name_prop.value);
  return true;
}

pid_t GetWindowPID(Window window) {
  unsigned char* prop = nullptr;
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  int ret = XGetWindowProperty(server.dsp, window, server.atom("_NET_WM_PID"),
                               0, 1024, False, AnyPropertyType, &actual_type,
                               &actual_format, &nitems, &bytes_after, &prop);

  if (ret == Success && prop != nullptr) {
    return (prop[1] << 8) | prop[0];
  }

  return -1;
}

int SetWindowPID(Window window) {
  pid_t pid = getpid();
  return XChangeProperty(server.dsp, window, server.atom("_NET_WM_PID"),
                         XA_CARDINAL, 32, PropModeReplace,
                         reinterpret_cast<unsigned char*>(&pid), 1);
}

Window CreateSimpleWindow(Window parent, int x, int y, unsigned int width,
                          unsigned int height, unsigned int border_width,
                          unsigned long border, unsigned long background) {
  Window window = XCreateSimpleWindow(server.dsp, parent, x, y, width, height,
                                      border_width, border, background);
  SetWindowPID(window);
  return window;
}

Window CreateWindow(Window parent, int x, int y, unsigned int width,
                    unsigned int height, unsigned int border_width, int depth,
                    unsigned int window_class, Visual* visual,
                    unsigned long valuemask, XSetWindowAttributes* attributes) {
  Window window =
      XCreateWindow(server.dsp, parent, x, y, width, height, border_width,
                    depth, window_class, visual, valuemask, attributes);
  SetWindowPID(window);
  return window;
}

}  // namespace x11
}  // namespace util
