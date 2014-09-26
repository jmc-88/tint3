/**************************************************************************
* Copyright (C) 2009 Sebastian Reichel <elektranox@gmail.com>
*
* Battery with functional data (percentage, time to life) and drawing data
* (area, font, ...). Each panel use his own drawing data.
* Need kernel > 2.6.23.
*
**************************************************************************/

#ifndef BATTERY_H
#define BATTERY_H

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>

#include "common.h"
#include "area.h"


// battery drawing parameter (per panel)
class Battery : public Area {
  public:
    Color font;
    int bat1_posy;
    int bat2_posy;

    void DrawForeground(cairo_t*) override;
    bool Resize() override;
};

enum chargestate {
    BATTERY_UNKNOWN,
    BATTERY_CHARGING,
    BATTERY_DISCHARGING,
    BATTERY_FULL
};

typedef struct battime {
    int16_t hours;
    int8_t minutes;
    int8_t seconds;
} battime;

typedef struct batstate {
    int percentage;
    struct battime time;
    enum chargestate state;
} batstate;

extern struct batstate battery_state;
extern PangoFontDescription* bat1_font_desc;
extern PangoFontDescription* bat2_font_desc;
extern int battery_enabled;
extern int percentage_hide;

extern int8_t battery_low_status;
extern std::string battery_low_cmd;
extern std::string path_energy_now;
extern std::string path_energy_full;
extern std::string path_current_now;
extern std::string path_status;

// default global data
void DefaultBattery();

// freed memory
void CleanupBattery();

// initialize clock : y position, ...
void UpdateBattery();

void InitBattery();
void InitBatteryPanel(void* panel);

#endif
