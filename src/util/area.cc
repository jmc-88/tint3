/**************************************************************************
*
* Tint3 : area
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
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

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "panel.hh"
#include "server.hh"
#include "util/area.hh"
#include "util/common.hh"
#include "util/log.hh"

namespace {

unsigned int HexCharToInt(char c) {
  c = std::tolower(c);
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return 0;
}

bool HexToRgb(std::string const& hex, unsigned int* r, unsigned int* g,
              unsigned int* b) {
  if (hex.empty() || hex[0] != '#') {
    return false;
  }

  if (hex.length() == 3 + 1) {
    (*r) = HexCharToInt(hex[1]);
    (*g) = HexCharToInt(hex[2]);
    (*b) = HexCharToInt(hex[3]);
  } else if (hex.length() == 6 + 1) {
    (*r) = HexCharToInt(hex[1]) * 16 + HexCharToInt(hex[2]);
    (*g) = HexCharToInt(hex[3]) * 16 + HexCharToInt(hex[4]);
    (*b) = HexCharToInt(hex[5]) * 16 + HexCharToInt(hex[6]);
  } else if (hex.length() == 12 + 1) {
    (*r) = HexCharToInt(hex[1]) * 16 + HexCharToInt(hex[2]);
    (*g) = HexCharToInt(hex[5]) * 16 + HexCharToInt(hex[6]);
    (*b) = HexCharToInt(hex[9]) * 16 + HexCharToInt(hex[10]);
  } else {
    return false;
  }

  return true;
}

}  // namespace

Color::Color() : color_(), alpha_() {}

Color::Color(std::array<double, 3> const& color, double alpha)
    : color_(color), alpha_(alpha) {}

Color::Color(Color const& other) : color_(other.color_), alpha_(other.alpha_) {}

Color::Color(Color&& other)
    : color_(std::move(other.color_)), alpha_(std::move(other.alpha_)) {}

Color& Color::operator=(Color other) {
  std::swap(color_, other.color_);
  std::swap(alpha_, other.alpha_);
  return (*this);
}

bool Color::SetColorFromHexString(std::string const& hex) {
  unsigned int r, g, b;
  if (!HexToRgb(hex, &r, &g, &b)) {
    return false;
  }

  color_[0] = (r / 255.0);
  color_[1] = (g / 255.0);
  color_[2] = (b / 255.0);
  return true;
}

double Color::operator[](unsigned int i) const { return color_.at(i); }

double Color::alpha() const { return alpha_; }

void Color::set_alpha(double alpha) { alpha_ = alpha; }

bool Color::operator==(Color const& other) const {
  return (color_[0] == other.color_[0] && color_[1] == other.color_[1] &&
          color_[2] == other.color_[2] && alpha_ == other.alpha_);
}

bool Color::operator!=(Color const& other) const { return !(*this == other); }

std::ostream& operator<<(std::ostream& os, Color const& color) {
  unsigned char r = (color[0] * 255);
  unsigned char g = (color[1] * 255);
  unsigned char b = (color[2] * 255);
  return os << "rgba(" << static_cast<unsigned int>(r) << ", "
            << static_cast<unsigned int>(g) << ", "
            << static_cast<unsigned int>(b) << ", " << color.alpha() << ")";
}

Border::Border() : width_(0), rounded_(0) {}

Border::Border(Border const& other)
    : width_(other.width_), rounded_(other.rounded_) {}

Border::Border(Border&& other)
    : width_(std::move(other.width_)), rounded_(std::move(other.rounded_)) {}

Border& Border::operator=(Border other) {
  Color::operator=(other);
  std::swap(width_, other.width_);
  std::swap(rounded_, other.rounded_);
  return (*this);
}

void Border::set_color(Color const& other) { Color::operator=(other); }

int Border::width() const { return width_; }

void Border::set_width(int width) { width_ = width; }

int Border::rounded() const { return rounded_; }

void Border::set_rounded(int rounded) { rounded_ = rounded; }

bool Border::operator==(Border const& other) const {
  return Color::operator==(other) && (width_ == other.width_) &&
         (rounded_ == other.rounded_);
}

bool Border::operator!=(Border const& other) const { return !(*this == other); }

Background::Background(Background const& other)
    : fill_color_(other.fill_color_),
      fill_color_hover_(other.fill_color_hover_),
      fill_color_pressed_(other.fill_color_pressed_),
      border_(other.border_),
      border_color_hover_(other.border_color_hover_),
      border_color_pressed_(other.border_color_pressed_) {}

Background::Background(Background&& other)
    : fill_color_(std::move(other.fill_color_)),
      fill_color_hover_(std::move(other.fill_color_hover_)),
      fill_color_pressed_(std::move(other.fill_color_pressed_)),
      border_(std::move(other.border_)),
      border_color_hover_(std::move(other.border_color_hover_)),
      border_color_pressed_(std::move(other.border_color_pressed_)) {}

Background& Background::operator=(Background other) {
  std::swap(fill_color_, other.fill_color_);
  std::swap(fill_color_hover_, other.fill_color_hover_);
  std::swap(fill_color_pressed_, other.fill_color_pressed_);
  std::swap(border_, other.border_);
  std::swap(border_color_hover_, other.border_color_hover_);
  std::swap(border_color_pressed_, other.border_color_pressed_);
  return (*this);
}

Color Background::fill_color() const { return fill_color_; }

void Background::set_fill_color(Color const& color) { fill_color_ = color; }

Color Background::fill_color_hover() const {
  if (!fill_color_hover_) {
    return fill_color_;
  }
  return fill_color_hover_.Unwrap();
}

void Background::set_fill_color_hover(Color const& color) {
  fill_color_hover_ = color;
}

Color Background::fill_color_pressed() const {
  if (!fill_color_pressed_) {
    return fill_color_;
  }
  return fill_color_pressed_.Unwrap();
}

void Background::set_fill_color_pressed(Color const& color) {
  fill_color_pressed_ = color;
}

Border const& Background::border() const { return border_; }

Border& Background::border() { return border_; }

void Background::set_border(Border const& border) { border_ = border; }

Color Background::border_color_hover() const {
  if (!border_color_hover_) {
    return border_;
  }
  return border_color_hover_.Unwrap();
}

void Background::set_border_color_hover(Color const& color) {
  border_color_hover_ = color;
}

Color Background::border_color_pressed() const {
  if (!border_color_pressed_) {
    return border_;
  }
  return border_color_pressed_.Unwrap();
}

void Background::set_border_color_pressed(Color const& color) {
  border_color_pressed_ = color;
}

bool Background::operator==(Background const& other) const {
  return (fill_color_ == other.fill_color_) &&
         (fill_color_hover_ == other.fill_color_hover_) &&
         (fill_color_pressed_ == other.fill_color_pressed_) &&
         (border_ == other.border_) &&
         (border_color_hover_ == other.border_color_hover_) &&
         (border_color_pressed_ == other.border_color_pressed_);
}

bool Background::operator!=(Background const& other) const {
  return !(*this == other);
}

Area::Area()
    : panel_x_(0),
      panel_y_(0),
      width_(0),
      height_(0),
      pix_(None),
      on_screen_(false),
      size_mode_(SizeMode::kByLayout),
      need_resize_(false),
      need_redraw_(false),
      padding_x_lr_(0),
      padding_x_(0),
      padding_y_(0),
      parent_(nullptr),
      panel_(nullptr),
      on_changed_(false),
      has_mouse_effects_(false),
      mouse_state_(MouseState::kMouseNormal) {}

Area::~Area() {}

Area& Area::CloneArea(Area const& other) {
  panel_x_ = other.panel_x_;
  panel_y_ = other.panel_y_;
  width_ = other.width_;
  height_ = other.height_;
  pix_ = other.pix_;
  bg_ = other.bg_;

  for (auto& child : children_) {
    delete child;
  }

  children_ = other.children_;

  on_screen_ = other.on_screen_;
  size_mode_ = other.size_mode_;
  need_resize_ = other.need_resize_;
  need_redraw_ = other.need_redraw_;
  padding_x_lr_ = other.padding_x_lr_;
  padding_x_ = other.padding_x_;
  padding_y_ = other.padding_y_;
  parent_ = other.parent_;
  panel_ = other.panel_;
  on_changed_ = other.on_changed_;
  return (*this);
}

/************************************************************
 * !!! This design is experimental and not yet fully implemented !!!!!!!!!!!!!
 *
 * DATA ORGANISATION :
 * Areas in tint3 are similar to widgets in a GUI.
 * All graphical objects (panel, taskbar, task, systray, clock, ...) 'inherit'
 *an abstract class 'Area'.
 * This class 'Area' manage the background, border, size, position and padding.
 * Area is at the begining of each object (&object == &area).
 *
 * tint3 define one panel per monitor. And each panel have a tree of Area.
 * The root of the tree is Panel.Area. And task, clock, systray, taskbar,... are
 *nodes.
 *
 * The tree give the localisation of each object :
 * - tree's root is in the background while tree's leafe are foreground objects
 * - position of a node/Area depend on the layout : parent's position (posx,
 *posy), size of previous brothers and parent's padding
 * - size of a node/Area depend on the content (kByContent objects) or on
 *the layout (kByLayout objects)
 *
 * DRAWING AND LAYERING ENGINE :
 * Redrawing an object (like the clock) could come from an 'external event'
 *(date change)
 * or from a 'layering event' (position change).
 * The following 'drawing engine' take care of :
 * - posx/posy of all Area
 * - 'layering event' propagation between object
 * 1) browse tree kByContent
 *  - resize kByContent node : children are resized before parent
 *  - if 'size' changed then 'need_resize = true' on the parent
 * 2) browse tree kByLayout and POSITION
 *  - resize kByLayout node : parent is resized before children
 *  - calculate position (posx,posy) : parent is calculated before children
 *  - if 'position' changed then 'need_redraw = 1'
 * 3) browse tree REDRAW
 *  - redraw needed objects : parent is drawn before children
 *
 * CONFIGURE PANEL'S LAYOUT :
 * 'panel_items' parameter (in config) define the list and the order of nodes in
 *tree's panel.
 * 'panel_items = SC' define a panel with just Systray and Clock.
 * So the tree 'Panel.Area' will have 2 children (Systray and Clock).
 *
 ************************************************************/

void Area::InitRendering(int pos) {
  const int w = bg_.border().width();
  // initialize fixed position/size
  for (auto& child : children_) {
    if (panel_horizontal) {
      child->panel_y_ = pos + w + padding_y_;
      child->height_ = height_ - (2 * (w + padding_y_));
      child->InitRendering(child->panel_y_);
    } else {
      child->panel_x_ = pos + w + padding_y_;
      child->width_ = width_ - (2 * (w + padding_y_));
      child->InitRendering(child->panel_x_);
    }
  }
}

void Area::SizeByContent() {
  // don't resize hidden objects
  if (!on_screen_) {
    return;
  }

  // children node are resized before its parent
  for (auto& child : children_) {
    child->SizeByContent();
  }

  // calculate area's size
  on_changed_ = false;

  if (need_resize_ && size_mode_ == SizeMode::kByContent) {
    need_resize_ = false;

    if (Resize()) {
      // 'size' changed => 'need_resize = true' on the parent
      parent_->need_resize_ = true;
      on_changed_ = true;
    }
  }
}

void Area::SizeByLayout(int pos, int level) {
  // don't resize hidden objects
  if (!on_screen_) {
    return;
  }

  // parent node is resized before its children
  // calculate area's size
  if (need_resize_ && size_mode_ == SizeMode::kByLayout) {
    need_resize_ = false;

    Resize();

    // resize children with kByLayout
    for (auto& child : children_) {
      if (child->size_mode_ == SizeMode::kByLayout &&
          child->children_.size() != 0) {
        child->need_resize_ = true;
      }
    }
  }

  // update position of children
  pos += padding_x_lr_ + bg_.border().width();

  for (auto& child : children_) {
    if (!child->on_screen_) {
      continue;
    }

    if (panel_horizontal) {
      if (pos != child->panel_x_) {
        child->panel_x_ = pos;
        child->on_changed_ = true;
      }
    } else {
      if (pos != child->panel_y_) {
        child->panel_y_ = pos;
        child->on_changed_ = true;
      }
    }

    child->SizeByLayout(pos, level + 1);

    if (panel_horizontal) {
      pos += child->width_ + padding_x_;
    } else {
      pos += child->height_ + padding_x_;
    }
  }

  if (on_changed_) {
    // pos/size changed
    need_redraw_ = true;
    OnChangeLayout();
  }
}

void Area::Refresh() {
  // don't draw and resize hidden objects
  if (!on_screen_) {
    return;
  }

  // don't draw transparent objects (without foreground and without background)
  if (need_redraw_) {
    need_redraw_ = false;
    Draw();
  }

  // draw current Area
  if (pix_ == None) {
    util::log::Debug() << "Empty area at panel_x_ = " << panel_x_
                       << ", width = " << width_ << '\n';
  }

  XCopyArea(server.dsp, pix_, panel_->temp_pmap, server.gc, 0, 0, width_,
            height_, panel_x_, panel_y_);

  // and then refresh child object
  for (auto& child : children_) {
    child->Refresh();
  }
}

int Area::ResizeByLayout(int maximum_size) {
  int size, nb_by_content = 0, nb_by_layout = 0;

  if (panel_horizontal) {
    // detect free size for kByLayout's Area
    size = width_ - (2 * (padding_x_lr_ + bg_.border().width()));

    for (auto& child : children_) {
      if (child->on_screen_) {
        if (child->size_mode_ == SizeMode::kByContent) {
          size -= child->width_;
          nb_by_content++;
        } else if (child->size_mode_ == SizeMode::kByLayout) {
          nb_by_layout++;
        }
      }
    }

    if (nb_by_content + nb_by_layout != 0) {
      size -= ((nb_by_content + nb_by_layout - 1) * padding_x_);
    }

    int width = 0, modulo = 0;

    if (nb_by_layout) {
      width = size / nb_by_layout;
      modulo = size % nb_by_layout;

      if (width > maximum_size && maximum_size != 0) {
        width = maximum_size;
        modulo = 0;
      }
    }

    // resize kByLayout objects
    for (auto& child : children_) {
      if (child->on_screen_ && child->size_mode_ == SizeMode::kByLayout) {
        int old_width = child->width_;
        child->width_ = width;

        if (modulo != 0) {
          child->width_++;
          modulo--;
        }

        if (child->width_ != old_width) {
          child->on_changed_ = true;
        }
      }
    }
  } else {
    // detect free size for kByLayout's Area
    size = height_ - (2 * (padding_x_lr_ + bg_.border().width()));

    for (auto& child : children_) {
      if (child->on_screen_) {
        if (child->size_mode_ == SizeMode::kByContent) {
          size -= child->height_;
          nb_by_content++;
        } else if (child->size_mode_ == SizeMode::kByLayout) {
          nb_by_layout++;
        }
      }
    }

    if (nb_by_content + nb_by_layout != 0) {
      size -= ((nb_by_content + nb_by_layout - 1) * padding_x_);
    }

    int height = 0, modulo = 0;

    if (nb_by_layout) {
      height = size / nb_by_layout;
      modulo = size % nb_by_layout;

      if (height > maximum_size && maximum_size != 0) {
        height = maximum_size;
        modulo = 0;
      }
    }

    // resize kByLayout objects
    for (auto& child : children_) {
      if (child->on_screen_ && child->size_mode_ == SizeMode::kByLayout) {
        int old_height = child->height_;
        child->height_ = height;

        if (modulo != 0) {
          child->height_++;
          modulo--;
        }

        if (child->height_ != old_height) {
          child->on_changed_ = true;
        }
      }
    }
  }

  return 0;
}

void Area::SetRedraw() {
  need_redraw_ = true;

  for (auto& child : children_) {
    child->SetRedraw();
  }
}

void Area::Hide() {
  on_screen_ = false;
  parent_->need_resize_ = true;

  if (panel_horizontal) {
    width_ = 0;
  } else {
    height_ = 0;
  }
}

void Area::Show() {
  on_screen_ = true;
  need_resize_ = true;
  parent_->need_resize_ = true;
}

void Area::Draw() {
  if (pix_) {
    XFreePixmap(server.dsp, pix_);
  }

  pix_ =
      XCreatePixmap(server.dsp, server.root_win, width_, height_, server.depth);

  // add layer of root pixmap (or clear pixmap if real_transparency==true)
  if (server.real_transparency) {
    ClearPixmap(pix_, 0, 0, width_, height_);
  }

  XCopyArea(server.dsp, panel_->temp_pmap, pix_, server.gc, panel_x_, panel_y_,
            width_, height_, 0, 0);

  cairo_surface_t* cs = cairo_xlib_surface_create(
      server.dsp, pix_, server.visual, width_, height_);
  cairo_t* c = cairo_create(cs);

  DrawBackground(c);
  DrawForeground(c);

  cairo_destroy(c);
  cairo_surface_destroy(cs);
}

void Area::DrawBackground(cairo_t* c) {
  const int w = bg_.border().width();

  Color fill_color;
  if (mouse_state_ == MouseState::kMouseNormal) {
    fill_color = bg_.fill_color();
  } else if (mouse_state_ == MouseState::kMouseOver) {
    fill_color = bg_.fill_color_hover();
  } else {
    fill_color = bg_.fill_color_pressed();
  }

  if (fill_color.alpha() > 0.0) {
    DrawRect(c, w, w, width_ - (2.0 * w), height_ - (2.0 * w),
             bg_.border().rounded() - w / 1.571);
    cairo_set_source_rgba(c, fill_color[0], fill_color[1], fill_color[2],
                          fill_color.alpha());
    cairo_fill(c);
  }

  Color border_color;
  if (mouse_state_ == MouseState::kMouseNormal) {
    border_color = bg_.border();
  } else if (mouse_state_ == MouseState::kMouseOver) {
    border_color = bg_.border_color_hover();
  } else {
    border_color = bg_.border_color_pressed();
  }

  if (w > 0) {
    cairo_set_line_width(c, w);

    // draw border inside (x, y, width, height)
    DrawRect(c, w / 2.0, w / 2.0, width_ - w, height_ - w,
             bg_.border().rounded());

    cairo_set_source_rgba(c, border_color[0], border_color[1], border_color[2],
                          border_color.alpha());
    cairo_stroke(c);
  }
}

bool Area::RemoveChild(Area* child) {
  auto const& it = std::find(children_.begin(), children_.end(), child);

  if (it != children_.end()) {
    children_.erase(it);
    SetRedraw();
    return true;
  }

  return false;
}

void Area::AddChild(Area* child) {
  children_.push_back(child);
  SetRedraw();
}

void Area::FreeArea() {
  for (auto& child : children_) {
    child->FreeArea();
  }

  children_.clear();

  if (pix_) {
    XFreePixmap(server.dsp, pix_);
    pix_ = None;
  }
}

void Area::DrawForeground(cairo_t*) { /* defaults to a no-op */
}

std::string Area::GetTooltipText() {
  /* defaults to a no-op */
  return std::string();
}

bool Area::Resize() {
  /* defaults to a no-op */
  return false;
}

void Area::OnChangeLayout() { /* defaults to a no-op */
}

bool Area::IsPointInside(int x, int y) const {
  bool inside_x = (x >= panel_x_ && x <= panel_x_ + width_);
  bool inside_y = (y >= panel_y_ && y <= panel_y_ + height_);
  return on_screen_ && inside_x && inside_y;
}

Area* Area::InnermostAreaUnderPoint(int x, int y) {
  if (!IsPointInside(x, y)) {
    return nullptr;
  }

  // Try looking for the innermost child that contains the given point.
  for (auto& child : children_) {
    Area* result = child->InnermostAreaUnderPoint(x, y);
    if (result != nullptr) {
      return result;
    }
  }

  // If no child has it, it has to be contained in this Area object itself.
  return this;
}

Area* Area::MouseOver(Area* previous_area, bool button_pressed) {
  if (!has_mouse_effects_) {
    return previous_area;
  }
  if (previous_area != nullptr && previous_area != this) {
    previous_area->MouseLeave();
  }
  set_mouse_state(button_pressed ? MouseState::kMousePressed
                                 : MouseState::kMouseOver);
  return this;
}

void Area::MouseLeave() { set_mouse_state(MouseState::kMouseNormal); }

void Area::set_has_mouse_effects(bool has_mouse_effects) {
  has_mouse_effects_ = has_mouse_effects;
}

void Area::set_mouse_state(MouseState new_state) {
  if (new_state != mouse_state_) {
    panel_refresh = true;
  }
  mouse_state_ = new_state;
  need_redraw_ = true;
}

#ifdef _TINT3_DEBUG

std::string Area::GetFriendlyName() const { return "Area"; }

void Area::PrintTreeLevel(unsigned int level) const {
  for (unsigned int i = 0; i < level; ++i) {
    util::log::Debug() << "  ";
  }

  util::log::Debug() << GetFriendlyName() << '('
                     << static_cast<void const*>(this) << ", "
                     << (size_mode_ == SizeMode::kByContent ? "kByContent"
                                                            : "kByLayout")
                     << ")\n";

  for (auto& child : children_) {
    child->PrintTreeLevel(level + 1);
  }
}

void Area::PrintTree() const { PrintTreeLevel(0); }

#endif  // _TINT3_DEBUG

void DrawRect(cairo_t* c, double x, double y, double w, double h, double r) {
  if (r > 0.0) {
    double c1 = 0.55228475 * r;

    cairo_move_to(c, x + r, y);
    cairo_rel_line_to(c, w - 2 * r, 0);
    cairo_rel_curve_to(c, c1, 0.0, r, c1, r, r);
    cairo_rel_line_to(c, 0, h - 2 * r);
    cairo_rel_curve_to(c, 0.0, c1, c1 - r, r, -r, r);
    cairo_rel_line_to(c, -w + 2 * r, 0);
    cairo_rel_curve_to(c, -c1, 0, -r, -c1, -r, -r);
    cairo_rel_line_to(c, 0, -h + 2 * r);
    cairo_rel_curve_to(c, 0, -c1, r - c1, -r, r, -r);
  } else {
    cairo_rectangle(c, x, y, w, h);
  }
}

void ClearPixmap(Pixmap p, int x, int y, int w, int h) {
  Picture pict = XRenderCreatePicture(
      server.dsp, p, XRenderFindVisualFormat(server.dsp, server.visual), 0, 0);
  XRenderColor col = {.red = 0, .green = 0, .blue = 0, .alpha = 0};
  XRenderFillRectangle(server.dsp, PictOpSrc, pict, &col, x, y, w, h);
  XRenderFreePicture(server.dsp, pict);
}
