#ifndef TINT3_CLOCK_CLOCK_H
#define TINT3_CLOCK_CLOCK_H

#include <sys/time.h>

#include <string>

#include "common.h"
#include "area.h"

class Clock : public Area {
 public:
  Color font_;
  int time1_posy_;
  int time2_posy_;

  void DrawForeground(cairo_t*) override;
  std::string GetTooltipText() override;
  bool Resize() override;

  static void InitPanel(Panel* panel);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG

 private:
  std::string time1_;
  std::string time2_;
};

extern std::string time1_format;
extern std::string time1_timezone;
extern std::string time2_format;
extern std::string time2_timezone;
extern std::string time_tooltip_format;
extern std::string time_tooltip_timezone;
extern std::string clock_lclick_command;
extern std::string clock_rclick_command;
extern PangoFontDescription* time1_font_desc;
extern PangoFontDescription* time2_font_desc;
extern bool clock_enabled;

// default global data
void DefaultClock();

// freed memory
void CleanupClock();

// initialize clock : y position, precision, ...
void InitClock();

void ClockAction(int button);

#endif  // TINT3_CLOCK_CLOCK_H
