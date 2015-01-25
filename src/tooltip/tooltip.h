/**************************************************************************
*
* Copyright (C) 2009 Andreas.Fink (Andreas.Fink85@gmail.com)
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <string>

#include "task.h"
#include "panel.h"
#include "util/timer.h"

class Tooltip {
 public:
  std::string tooltip_text;
  Panel* panel;
  Window window;
  int show_timeout_msec;
  int hide_timeout_msec;
  Bool mapped;
  int paddingx;
  int paddingy;
  PangoFontDescription* font_desc;
  Color font_color;
  Background* bg;
  Timeout* timeout;

  Area* area_;
  void CopyText(Area* area);

  void Update();
};

extern Tooltip g_tooltip;

// default global data
void DefaultTooltip();

// freed memory
void CleanupTooltip();

void InitTooltip();
void TooltipTriggerHide();
void TooltipTriggerShow(Area* area, Panel* p, XEvent* e);
void TooltipHide(void* /*arg*/);
void TooltipShow(void* /*arg*/);

#endif  // TOOLTIP_H
