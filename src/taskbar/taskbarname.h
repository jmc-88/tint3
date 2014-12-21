/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
**************************************************************************/

#ifndef TASKBARNAME_H
#define TASKBARNAME_H

#include "common.h"
#include "area.h"

extern bool taskbarname_enabled;
extern PangoFontDescription* taskbarname_font_desc;
extern Color taskbarname_font;
extern Color taskbarname_active_font;

void DefaultTaskbarname();
void CleanupTaskbarname();

void InitTaskbarnamePanel(Panel* panel);

#endif
