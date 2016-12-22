#ifndef TINT3_SYSTRAYBAR_SYSTRAYBAR_HH
#define TINT3_SYSTRAYBAR_SYSTRAYBAR_HH

#include <X11/extensions/Xdamage.h>

#include <list>

#include "systray/tray_window.hh"
#include "util/area.hh"
#include "util/common.hh"
#include "util/timer.hh"

// XEMBED messages
#define XEMBED_EMBEDDED_NOTIFY 0
// Flags for _XEMBED_INFO
#define XEMBED_MAPPED (1 << 0)

class Systraybar : public Area {
  void RemoveIconInternal(TrayWindow* traywin, Timer& timer);

 public:
  std::list<TrayWindow*> list_icons;
  int sort;
  int alpha, saturation, brightness;
  int icon_size, icons_per_column, icons_per_row, margin_;

  bool should_refresh() const;
  void set_should_refresh(bool should_refresh);

  void DrawForeground(cairo_t*) override;
  void OnChangeLayout() override;
  bool Resize() override;

  size_t VisibleIcons() const;
  bool AddIcon(Window id);
  void RemoveIcon(TrayWindow* traywin, Timer& timer);
  void Clear(Timer& timer);

  // systray protocol
  // many tray icon don't manage stop/restart of the systray manager
  void StartNet(Timer& timer);
  void StopNet(Timer& timer);
  void NetMessage(XClientMessageEvent* e);

  static void InitPanel(Panel* panel);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG

 private:
  bool should_refresh_;
};

// net_sel_win != None when protocol started
extern Window net_sel_win;
extern Systraybar systray;
extern bool systray_enabled;
extern int systray_max_icon_size;

// default global data
void DefaultSystray();

// freed memory
void CleanupSystray(Timer& timer);

// initialize protocol and panel position
void InitSystray(Timer& timer);

void RefreshSystrayIcon(Timer& timer);
void SystrayRenderIcon(TrayWindow* traywin, Timer& timer);

#endif  // TINT3_SYSTRAYBAR_SYSTRAYBAR_HH
