#ifndef TINT3_EXECP_EXECP_HH
#define TINT3_EXECP_EXECP_HH

#include <memory>
#include <string>

#include "util/area.hh"
#include "util/color.hh"
#include "util/pango.hh"

class Panel;
class Executor : public Area {
 public:
  Executor();

  void InitPanel(Panel* panel);

  std::string GetTooltipText();
  void DrawForeground(cairo_t* c);
  bool Resize();

  void set_cache_icon(bool cache_icon);
  void set_centered(bool centered);

  std::string const& command() const;
  void set_command(std::string const& command);
  void set_command_left_click(std::string const& command);
  void set_command_middle_click(std::string const& command);
  void set_command_right_click(std::string const& command);
  void set_command_up_wheel(std::string const& command);
  void set_command_down_wheel(std::string const& command);

  bool continuous() const;
  void set_continuous(unsigned int continuous);

  void set_font(std::string const& font);
  void set_font_color(Color const& color);
  void set_has_icon(bool has_icon);

  unsigned int icon_height() const;
  void set_icon_height(unsigned int height);

  unsigned int icon_width() const;
  void set_icon_width(unsigned int width);

  unsigned int interval() const;
  void set_interval(unsigned int interval);

  void set_markup(bool continuous);
  void set_tooltip(std::string const& tooltip);

 private:
  Background background_;
  bool cache_icon_;
  bool centered_;
  std::string command_;
  std::string command_left_click_;
  std::string command_middle_click_;
  std::string command_right_click_;
  std::string command_up_wheel_;
  std::string command_down_wheel_;
  bool continuous_;
  util::pango::FontDescriptionPtr font_description_;
  Color font_color_;
  bool has_icon_;
  unsigned int icon_height_;
  unsigned int icon_width_;
  unsigned int interval_;
  bool markup_;
  std::string tooltip_;
};

#endif  // TINT3_EXECP_EXECP_HH
