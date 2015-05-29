#ifndef TINT3_TASKBAR_TASKBARNAME_H
#define TINT3_TASKBAR_TASKBARNAME_H

#include "taskbar/taskbarbase.h"
#include "util/common.h"
#include "util/area.h"

extern bool taskbarname_enabled;
extern PangoFontDescription* taskbarname_font_desc;
extern Color taskbarname_font;
extern Color taskbarname_active_font;

class Taskbarname : public TaskbarBase {
  std::string name_;

 public:
  std::string const& name() const;
  Taskbarname& set_name(std::string const& name);

  void DrawForeground(cairo_t*) override;
  bool Resize() override;

  static void Default();
  static void Cleanup();
  static void InitPanel(Panel* panel);
};

#endif  // TINT3_TASKBAR_TASKBARNAME_H
