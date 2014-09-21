/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
*
* Clock with fonctionnal data (timeval, precision) and drawing data (area, font, ...).
* Each panel use his own drawing data.
*
**************************************************************************/

#ifndef CLOCK_H
#define CLOCK_H

#include <sys/time.h>

#include <string>

#include "common.h"
#include "area.h"


class Clock : public Area {
  public:
    Color font;
    int time1_posy;
    int time2_posy;

    void DrawForeground(cairo_t*);
    std::string GetTooltipText();
    bool Resize();
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
void InitClockPanel(void* panel);

void clock_action(int button);

#endif
