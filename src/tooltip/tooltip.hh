#ifndef TINT3_TOOLTIP_TOOLTIP_HH
#define TINT3_TOOLTIP_TOOLTIP_HH

#include <string>

#include "panel.hh"
#include "task.hh"
#include "util/timer.hh"

class Tooltip {
 public:
  std::string tooltip_text;
  Panel* panel;
  Window window;
  int show_timeout_msec;
  int hide_timeout_msec;
  bool mapped_;
  int paddingx;
  int paddingy;
  PangoFontDescription* font_desc;
  Color font_color;
  Background* bg;
  Interval::Id timeout;

  void BindTo(Area* area);
  bool IsBoundTo(Area* area) const;

  void Update(Timer& timer);

 private:
  Area* area_;
};

extern Tooltip g_tooltip;

// default global data
void DefaultTooltip();

// freed memory
void CleanupTooltip(Timer& timer);

void InitTooltip();
void TooltipTriggerHide(Timer& timer);
void TooltipTriggerShow(Area* area, Panel* p, XEvent* e, Timer& timer);
bool TooltipHide(Timer& timer);
bool TooltipShow(Timer& timer);

#endif  // TINT3_TOOLTIP_TOOLTIP_HH
