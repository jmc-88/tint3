#ifndef TINT3_TOOLTIP_TOOLTIP_HH
#define TINT3_TOOLTIP_TOOLTIP_HH

#include <cairo/cairo.h>

#include <string>

#include "server.hh"
#include "util/area.hh"
#include "util/color.hh"
#include "util/pango.hh"
#include "util/timer.hh"

struct TooltipConfig {
  Background bg;
  Color font_color = Color{Color::Array{1.0, 1.0, 1.0}, 1.0};
  util::pango::FontDescriptionPtr font_desc;
  int paddingx = 0;
  int paddingy = 0;
  unsigned int show_timeout_msec = 0;
  unsigned int hide_timeout_msec = 0;
};

extern TooltipConfig tooltip_config;

class Tooltip {
 public:
  Tooltip(Server* server, Timer* timer);
  Tooltip() = delete;
  Tooltip(Tooltip const&) = delete;
  Tooltip(Tooltip&&) = delete;
  ~Tooltip();

  // Getter: returns the Window associated with this Tooltip.
  Window window() const;

  // IsBound tells whether the tooltip is bound to any Area.
  bool IsBound() const;

  // IsBoundTo tells whether the tooltip is bound to the given Area.
  bool IsBoundTo(Area const* area) const;

  // Show triggers the show tooltip timeout, which maps the tooltip window on
  // the screen and calls out to Update().
  void Show(Area const* area, XEvent const* e, std::string text);

  // Update binds the tooltip to given Area, resizes and redraws the tooltip
  // window with the given text.
  void Update(Area const* area, XEvent const* e, std::string const& text);

  // Hide triggers the hide tooltip timeout, which unbinds the tooltip from the
  // Area and unmaps the tooltip window from the screen.
  void Hide();

 private:
  Server* server_;
  Timer* timer_;
  Area const* area_;
  util::pango::FontDescriptionPtr font_desc_;
  Window window_;
  Interval::Id timeout_;

  void GetExtents(std::string const& text, int* x, int* y, int* width,
                  int* height);
  void DrawBackground(cairo_t* c, int width, int height);
  void DrawBorder(cairo_t* c, int width, int height);
  void DrawText(cairo_t* c, int width, int height, std::string const& text);
};

#endif  // TINT3_TOOLTIP_TOOLTIP_HH
