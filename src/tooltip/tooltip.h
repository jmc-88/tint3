#ifndef TINT3_TOOLTIP_TOOLTIP_H
#define TINT3_TOOLTIP_TOOLTIP_H

#include <string>

#include "panel.h"
#include "task.h"
#include "util/timer.h"

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
  Interval* timeout;

  void BindTo(Area* area);
  bool IsBoundTo(Area* area) const;

  void Update(ChronoTimer& timer);

 private:
  Area* area_;
};

extern Tooltip g_tooltip;

// default global data
void DefaultTooltip();

// freed memory
void CleanupTooltip(ChronoTimer& timer);

void InitTooltip();
void TooltipTriggerHide(ChronoTimer& timer);
void TooltipTriggerShow(Area* area, Panel* p, XEvent* e, ChronoTimer& timer);
bool TooltipHide(ChronoTimer& timer);
bool TooltipShow(ChronoTimer& timer);

#endif  // TINT3_TOOLTIP_TOOLTIP_H
