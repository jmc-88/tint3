/**************************************************************************
*
* Tint3 : area
*
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "area.h"
#include "panel.h"
#include "../server.h"

Area::~Area() {
}

Area& Area::CloneArea(Area const& other) {
    posx_ = other.posx_;
    posy_ = other.posy_;
    width_ = other.width_;
    height_ = other.height_;
    pix_ = other.pix_;
    bg_ = other.bg_;

    // TODO: this should probably free all Area*
    // children before copying the others over
    children_.clear();
    std::copy(other.children_.begin(),
              other.children_.end(),
              children_.begin());

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
    return *this;
}

/************************************************************
 * !!! This design is experimental and not yet fully implemented !!!!!!!!!!!!!
 *
 * DATA ORGANISATION :
 * Areas in tint3 are similar to widgets in a GUI.
 * All graphical objects (panel, taskbar, task, systray, clock, ...) 'inherit' an abstract class 'Area'.
 * This class 'Area' manage the background, border, size, position and padding.
 * Area is at the begining of each object (&object == &area).
 *
 * tint3 define one panel per monitor. And each panel have a tree of Area.
 * The root of the tree is Panel.Area. And task, clock, systray, taskbar,... are nodes.
 *
 * The tree give the localisation of each object :
 * - tree's root is in the background while tree's leafe are foreground objects
 * - position of a node/Area depend on the layout : parent's position (posx, posy), size of previous brothers and parent's padding
 * - size of a node/Area depend on the content (SIZE_BY_CONTENT objects) or on the layout (SIZE_BY_LAYOUT objects)
 *
 * DRAWING AND LAYERING ENGINE :
 * Redrawing an object (like the clock) could come from an 'external event' (date change)
 * or from a 'layering event' (position change).
 * The following 'drawing engine' take care of :
 * - posx/posy of all Area
 * - 'layering event' propagation between object
 * 1) browse tree SIZE_BY_CONTENT
 *  - resize SIZE_BY_CONTENT node : children are resized before parent
 *  - if 'size' changed then 'need_resize = true' on the parent
 * 2) browse tree SIZE_BY_LAYOUT and POSITION
 *  - resize SIZE_BY_LAYOUT node : parent is resized before children
 *  - calculate position (posx,posy) : parent is calculated before children
 *  - if 'position' changed then 'need_redraw = 1'
 * 3) browse tree REDRAW
 *  - redraw needed objects : parent is drawn before children
 *
 * CONFIGURE PANEL'S LAYOUT :
 * 'panel_items' parameter (in config) define the list and the order of nodes in tree's panel.
 * 'panel_items = SC' define a panel with just Systray and Clock.
 * So the tree 'Panel.Area' will have 2 childs (Systray and Clock).
 *
 ************************************************************/

void Area::InitRendering(int pos) {
    // initialize fixed position/size
    for (auto& child : children_) {
        if (panel_horizontal) {
            child->posy_ = pos + bg_->border.width + padding_y_;
            child->height_ = height_ - (2 * (bg_->border.width + padding_y_));
            child->InitRendering(child->posy_);
        } else {
            child->posx_ = pos + bg_->border.width + padding_y_;
            child->width_ = width_ - (2 * (bg_->border.width + padding_y_));
            child->InitRendering(child->posx_);
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
    on_changed_ = 0;

    if (need_resize_ && size_mode_ == SIZE_BY_CONTENT) {
        need_resize_ = false;

        if (Resize()) {
            // 'size' changed => 'need_resize = true' on the parent
            parent_->need_resize_ = true;
            on_changed_ = 1;
        }
    }
}


void Area::SizeByLayout(int pos, int level) {
    // don't resize hiden objects
    if (!on_screen_) {
        return;
    }

    // parent node is resized before its children
    // calculate area's size
    if (need_resize_ && size_mode_ == SIZE_BY_LAYOUT) {
        need_resize_ = false;

        Resize();

        // resize children with SIZE_BY_LAYOUT
        for (auto& child : children_) {
            if (child->size_mode_ == SIZE_BY_LAYOUT && child->children_.size() != 0) {
                child->need_resize_ = true;
            }
        }
    }

    // update position of childs
    pos += padding_x_lr_ + bg_->border.width;

    for (auto& child : children_) {
        if (!child->on_screen_) {
            return;
        }

        if (panel_horizontal) {
            if (pos != child->posx_) {
                // pos changed => redraw
                child->posx_ = pos;
                child->on_changed_ = 1;
            }
        } else {
            if (pos != child->posy_) {
                // pos changed => redraw
                child->posy_ = pos;
                child->on_changed_ = 1;
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
    if (pix_ == 0) {
        printf("empty area posx %d, width %d\n", posx_, width_);
    }

    XCopyArea(
        server.dsp, pix_,
        panel_->temp_pmap, server.gc, 0, 0,
        width_, height_, posx_, posy_);

    // and then refresh child object
    for (auto& child : children_) {
        child->Refresh();
    }
}


int Area::ResizeByLayout(int maximum_size) {
    int size, nb_by_content = 0, nb_by_layout = 0;

    if (panel_horizontal) {
        // detect free size for SIZE_BY_LAYOUT's Area
        size = width_ - (2 * (padding_x_lr_ + bg_->border.width));

        for (auto& child : children_) {
            if (child->on_screen_ && child->size_mode_ == SIZE_BY_CONTENT) {
                size -= child->width_;
                nb_by_content++;
            }

            if (child->on_screen_ && child->size_mode_ == SIZE_BY_LAYOUT) {
                nb_by_layout++;
            }
        }

        //printf("  resize_by_layout Deb %d, %d\n", nb_by_content, nb_by_layout);
        if (nb_by_content + nb_by_layout) {
            size -= ((nb_by_content + nb_by_layout - 1) * padding_x_);
        }

        int width = 0, modulo = 0, old_width;

        if (nb_by_layout) {
            width = size / nb_by_layout;
            modulo = size % nb_by_layout;

            if (width > maximum_size && maximum_size != 0) {
                width = maximum_size;
                modulo = 0;
            }
        }

        // resize SIZE_BY_LAYOUT objects
        for (auto& child : children_) {
            if (child->on_screen_ && child->size_mode_ == SIZE_BY_LAYOUT) {
                old_width = child->width_;
                child->width_ = width;

                if (modulo) {
                    child->width_++;
                    modulo--;
                }

                if (child->width_ != old_width) {
                    child->on_changed_ = 1;
                }
            }
        }
    } else {
        // detect free size for SIZE_BY_LAYOUT's Area
        size = height_ - (2 * (padding_x_lr_ + bg_->border.width));

        for (auto& child : children_) {
            if (child->on_screen_ && child->size_mode_ == SIZE_BY_CONTENT) {
                size -= child->height_;
                nb_by_content++;
            }

            if (child->on_screen_ && child->size_mode_ == SIZE_BY_LAYOUT) {
                nb_by_layout++;
            }
        }

        if (nb_by_content + nb_by_layout) {
            size -= ((nb_by_content + nb_by_layout - 1) * padding_x_);
        }

        int height = 0, modulo = 0, old_height;

        if (nb_by_layout) {
            height = size / nb_by_layout;
            modulo = size % nb_by_layout;

            if (height > maximum_size && maximum_size != 0) {
                height = maximum_size;
                modulo = 0;
            }
        }

        // resize SIZE_BY_LAYOUT objects
        for (auto& child : children_) {
            if (child->on_screen_ && child->size_mode_ == SIZE_BY_LAYOUT) {
                old_height = child->height_;
                child->height_ = height;

                if (modulo) {
                    child->height_++;
                    modulo--;
                }

                if (child->height_ != old_height) {
                    child->on_changed_ = 1;
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
    on_screen_ = 0;
    parent_->need_resize_ = true;

    if (panel_horizontal) {
        width_ = 0;
    } else {
        height_ = 0;
    }
}

void Area::Show() {
    on_screen_ = 1;
    need_resize_ = true;
    parent_->need_resize_ = true;
}

void Area::Draw() {
    if (pix_) {
        XFreePixmap(server.dsp, pix_);
    }

    pix_ = XCreatePixmap(server.dsp, server.root_win, width_, height_,
                         server.depth);

    // add layer of root pixmap (or clear pixmap if real_transparency==true)
    if (server.real_transparency) {
        clear_pixmap(pix_, 0 , 0, width_, height_);
    }

    XCopyArea(server.dsp, panel_->temp_pmap, pix_, server.gc,
              posx_, posy_, width_, height_, 0, 0);

    cairo_surface_t* cs = cairo_xlib_surface_create(server.dsp, pix_, server.visual,
                          width_,
                          height_);
    cairo_t* c = cairo_create(cs);

    DrawBackground(c);
    DrawForeground(c);

    cairo_destroy(c);
    cairo_surface_destroy(cs);
}


void Area::DrawBackground(cairo_t* c) {
    if (bg_->back.alpha > 0.0) {
        //printf("    draw_background (%d %d) RGBA (%lf, %lf, %lf, %lf)\n", posx, posy, pix->back.color[0], pix->back.color[1], pix->back.color[2], pix->back.alpha);
        draw_rect(c, bg_->border.width, bg_->border.width,
                  width_ - (2.0 * bg_->border.width), height_ - (2.0 * bg_->border.width),
                  bg_->border.rounded - bg_->border.width / 1.571);
        cairo_set_source_rgba(c, bg_->back.color[0], bg_->back.color[1],
                              bg_->back.color[2], bg_->back.alpha);
        cairo_fill(c);
    }

    if (bg_->border.width > 0 && bg_->border.alpha > 0.0) {
        cairo_set_line_width(c, bg_->border.width);

        // draw border inside (x, y, width, height)
        draw_rect(c, bg_->border.width / 2.0, bg_->border.width / 2.0,
                  width_ - bg_->border.width, height_ - bg_->border.width,
                  bg_->border.rounded);

        cairo_set_source_rgba(c, bg_->border.color[0], bg_->border.color[1],
                              bg_->border.color[2], bg_->border.alpha);
        cairo_stroke(c);
    }
}


void Area::RemoveArea() {
    auto it = std::find(parent_->children_.begin(), parent_->children_.end(), this);

    if (it != parent_->children_.end()) {
        parent_->children_.erase(it);
    }

    parent_->SetRedraw();
}


void Area::AddArea() {
    parent_->children_.push_back(this);
    parent_->SetRedraw();
}


void Area::FreeArea() {
    for (auto& child : children_) {
        child->FreeArea();
    }

    children_.clear();

    if (pix_) {
        XFreePixmap(server.dsp, pix_);
        pix_ = 0;
    }
}


void Area::DrawForeground(cairo_t*) {
    /* defaults to a no-op */
}


std::string Area::GetTooltipText() {
    /* defaults to a no-op */
    return std::string();
}


bool Area::Resize() {
    /* defaults to a no-op */
    return false;
}


void Area::OnChangeLayout() {
    /* defaults to a no-op */
}


void draw_rect(cairo_t* c, double x, double y, double w, double h, double r) {
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


void clear_pixmap(Pixmap p, int x, int y, int w, int h) {
    Picture pict = XRenderCreatePicture(server.dsp, p,
                                        XRenderFindVisualFormat(server.dsp, server.visual), 0, 0);
    XRenderColor col = { .red = 0, .green = 0, .blue = 0, .alpha = 0 };
    XRenderFillRectangle(server.dsp, PictOpSrc, pict, &col, x, y, w, h);
    XRenderFreePicture(server.dsp, pict);
}
