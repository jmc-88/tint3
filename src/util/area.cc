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

Area& Area::Clone(Area const& other) {
    *this = other;
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
    for (auto& child : children) {
        if (panel_horizontal) {
            child->posy = pos + bg->border.width + paddingy;
            child->height = height - (2 * (bg->border.width + paddingy));
            child->InitRendering(child->posy);
        } else {
            child->posx = pos + bg->border.width + paddingy;
            child->width = width - (2 * (bg->border.width + paddingy));
            child->InitRendering(child->posx);
        }
    }
}


void Area::SizeByContent() {
    // don't resize hidden objects
    if (!on_screen) {
        return;
    }

    // children node are resized before its parent
    for (auto& child : children) {
        child->SizeByContent();
    }

    // calculate area's size
    on_changed = 0;

    if (need_resize && size_mode == SIZE_BY_CONTENT) {
        need_resize = false;

        if (Resize()) {
            // 'size' changed => 'need_resize = true' on the parent
            parent->need_resize = true;
            on_changed = 1;
        }
    }
}


void Area::SizeByLayout(int pos, int level) {
    // don't resize hiden objects
    if (!on_screen) {
        return;
    }

    // parent node is resized before its children
    // calculate area's size
    if (need_resize && size_mode == SIZE_BY_LAYOUT) {
        need_resize = false;

        Resize();

        // resize children with SIZE_BY_LAYOUT
        for (auto& child : children) {
            if (child->size_mode == SIZE_BY_LAYOUT && child->children.size() != 0) {
                child->need_resize = true;
            }
        }
    }

    // update position of childs
    pos += paddingxlr + bg->border.width;
    int i = 0;

    for (auto& child : children) {
        if (!child->on_screen) {
            return;
        }

        i++;

        if (panel_horizontal) {
            if (pos != child->posx) {
                // pos changed => redraw
                child->posx = pos;
                child->on_changed = 1;
            }
        } else {
            if (pos != child->posy) {
                // pos changed => redraw
                child->posy = pos;
                child->on_changed = 1;
            }
        }

        /*// position of each visible object
        int k;
        for (k=0 ; k < level ; k++) printf("  ");
        printf("tree level %d, object %d, pos %d, %s\n", level, i, pos, (child->size_mode == SIZE_BY_LAYOUT) ? "SIZE_BY_LAYOUT" : "SIZE_BY_CONTENT");*/
        child->SizeByLayout(pos, level + 1);

        if (panel_horizontal) {
            pos += child->width + paddingx;
        } else {
            pos += child->height + paddingx;
        }
    }

    if (on_changed) {
        // pos/size changed
        need_redraw = true;
        OnChangeLayout();
    }
}


void Area::Refresh() {
    // don't draw and resize hidden objects
    if (!on_screen) {
        return;
    }

    // don't draw transparent objects (without foreground and without background)
    if (need_redraw) {
        need_redraw = false;
        Draw();
    }

    // draw current Area
    if (pix == 0) {
        printf("empty area posx %d, width %d\n", posx, width);
    }

    XCopyArea(
        server.dsp, pix,
        panel->temp_pmap, server.gc, 0, 0,
        width, height, posx, posy);

    // and then refresh child object
    for (auto& child : children) {
        child->Refresh();
    }
}


int Area::ResizeByLayout(int maximum_size) {
    int size, nb_by_content = 0, nb_by_layout = 0;

    if (panel_horizontal) {
        // detect free size for SIZE_BY_LAYOUT's Area
        size = width - (2 * (paddingxlr + bg->border.width));

        for (auto& child : children) {
            if (child->on_screen && child->size_mode == SIZE_BY_CONTENT) {
                size -= child->width;
                nb_by_content++;
            }

            if (child->on_screen && child->size_mode == SIZE_BY_LAYOUT) {
                nb_by_layout++;
            }
        }

        //printf("  resize_by_layout Deb %d, %d\n", nb_by_content, nb_by_layout);
        if (nb_by_content + nb_by_layout) {
            size -= ((nb_by_content + nb_by_layout - 1) * paddingx);
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
        for (auto& child : children) {
            if (child->on_screen && child->size_mode == SIZE_BY_LAYOUT) {
                old_width = child->width;
                child->width = width;

                if (modulo) {
                    child->width++;
                    modulo--;
                }

                if (child->width != old_width) {
                    child->on_changed = 1;
                }
            }
        }
    } else {
        // detect free size for SIZE_BY_LAYOUT's Area
        size = height - (2 * (paddingxlr + bg->border.width));

        for (auto& child : children) {
            if (child->on_screen && child->size_mode == SIZE_BY_CONTENT) {
                size -= child->height;
                nb_by_content++;
            }

            if (child->on_screen && child->size_mode == SIZE_BY_LAYOUT) {
                nb_by_layout++;
            }
        }

        if (nb_by_content + nb_by_layout) {
            size -= ((nb_by_content + nb_by_layout - 1) * paddingx);
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
        for (auto& child : children) {
            if (child->on_screen && child->size_mode == SIZE_BY_LAYOUT) {
                old_height = child->height;
                child->height = height;

                if (modulo) {
                    child->height++;
                    modulo--;
                }

                if (child->height != old_height) {
                    child->on_changed = 1;
                }
            }
        }
    }

    return 0;
}


void Area::SetRedraw() {
    need_redraw = true;

    for (auto& child : children) {
        child->SetRedraw();
    }
}

void Area::Hide() {
    on_screen = 0;
    parent->need_resize = true;

    if (panel_horizontal) {
        width = 0;
    } else {
        height = 0;
    }
}

void Area::Show() {
    on_screen = 1;
    need_resize = true;
    parent->need_resize = true;
}

void Area::Draw() {
    if (pix) {
        XFreePixmap(server.dsp, pix);
    }

    pix = XCreatePixmap(server.dsp, server.root_win, width, height,
                        server.depth);

    // add layer of root pixmap (or clear pixmap if real_transparency==true)
    if (server.real_transparency) {
        clear_pixmap(pix, 0 , 0, width, height);
    }

    XCopyArea(server.dsp, panel->temp_pmap, pix, server.gc,
              posx, posy, width, height, 0, 0);

    cairo_surface_t* cs = cairo_xlib_surface_create(server.dsp, pix, server.visual,
                          width,
                          height);
    cairo_t* c = cairo_create(cs);

    DrawBackground(c);
    DrawForeground(c);

    cairo_destroy(c);
    cairo_surface_destroy(cs);
}


void Area::DrawBackground(cairo_t* c) {
    if (bg->back.alpha > 0.0) {
        //printf("    draw_background (%d %d) RGBA (%lf, %lf, %lf, %lf)\n", posx, posy, pix->back.color[0], pix->back.color[1], pix->back.color[2], pix->back.alpha);
        draw_rect(c, bg->border.width, bg->border.width,
                  width - (2.0 * bg->border.width), height - (2.0 * bg->border.width),
                  bg->border.rounded - bg->border.width / 1.571);
        cairo_set_source_rgba(c, bg->back.color[0], bg->back.color[1],
                              bg->back.color[2], bg->back.alpha);
        cairo_fill(c);
    }

    if (bg->border.width > 0 && bg->border.alpha > 0.0) {
        cairo_set_line_width(c, bg->border.width);

        // draw border inside (x, y, width, height)
        draw_rect(c, bg->border.width / 2.0, bg->border.width / 2.0,
                  width - bg->border.width, height - bg->border.width,
                  bg->border.rounded);

        cairo_set_source_rgba(c, bg->border.color[0], bg->border.color[1],
                              bg->border.color[2], bg->border.alpha);
        cairo_stroke(c);
    }
}


void Area::RemoveArea() {
    auto it = std::find(parent->children.begin(), parent->children.end(), this);

    if (it != parent->children.end()) {
        parent->children.erase(it);
    }

    parent->SetRedraw();
}


void Area::AddArea() {
    parent->children.push_back(this);
    parent->SetRedraw();
}


void Area::FreeArea() {
    for (auto& child : children) {
        child->FreeArea();
    }

    children.clear();

    if (pix) {
        XFreePixmap(server.dsp, pix);
        pix = 0;
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
