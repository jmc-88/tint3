#ifndef TINT3_BATTERY_BATTERY_H
#define TINT3_BATTERY_BATTERY_H

#include <fcntl.h>
#include <pango/pangocairo.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "util/area.h"
#include "util/common.h"
#include "util/timer.h"

// forward declarations
class Panel;

// battery drawing parameter (per panel)
class Battery : public Area {
 public:
  Color font;
  int bat1_posy;
  int bat2_posy;

  void DrawForeground(cairo_t*) override;
  bool Resize() override;

  static void InitPanel(Panel* panel);

#ifdef _TINT3_DEBUG

  std::string GetFriendlyName() const override;

#endif  // _TINT3_DEBUG

 private:
  std::string battery_percentage_;
  std::string battery_time_;
};

extern PangoFontDescription* bat1_font_desc;
extern PangoFontDescription* bat2_font_desc;
extern bool battery_enabled;
extern int percentage_hide;

extern int8_t battery_low_status;
extern std::string battery_low_cmd;
extern std::string path_energy_now;
extern std::string path_energy_full;
extern std::string path_current_now;
extern std::string path_status;

// default global data
void DefaultBattery();

// free memory
void CleanupBattery(Timer& timer);

// initialize clock : y position, ...
void UpdateBattery();

void InitBattery(Timer& timer);

#endif  // TINT3_BATTERY_BATTERY_H
