/**************************************************************************
* Copyright (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr)
*
* base class for all graphical objects (panel, taskbar, task, systray, clock, ...).
* Area is at the begining of each object (&object == &area).
*
* Area manage the background and border drawing, size and padding.
* Each Area has one Pixmap (pix).
*
* Area manage the tree of all objects. Parent object drawn before child object.
*   panel -> taskbars -> tasks
*         -> systray -> icons
*         -> clock
*
* draw_foreground(obj) and resize(obj) are virtual function.
*
**************************************************************************/

#ifndef AREA_H
#define AREA_H

#include <glib.h>
#include <X11/Xlib.h>
#include <cairo.h>
#include <cairo-xlib.h>

#include <string>
#include <vector>

struct Border {
    double color[3];
    double alpha;
    int width;
    int rounded;
};


struct Color {
    double color[3];
    double alpha;
};

struct Background {
    Color back;
    Border border;
};


// way to calculate the size
// SIZE_BY_LAYOUT objects : taskbar and task
// SIZE_BY_CONTENT objects : clock, battery, launcher, systray
enum {
    SIZE_BY_LAYOUT,
    SIZE_BY_CONTENT
};

class Panel;
class Area {
  public:
    virtual ~Area() = 0;

    Area& CloneArea(Area const&);

    // coordinate relative to panel window
    int panel_x_;
    int panel_y_;
    // width and height including border
    int width_;
    int height_;
    Pixmap pix_;
    Background* bg_;

    // list of children Area objects
    std::vector<Area*> children_;

    // object visible on screen.
    // An object (like systray) could be enabled but hidden (because no tray icon).
    bool on_screen_;
    // way to calculate the size (SIZE_BY_CONTENT or SIZE_BY_LAYOUT)
    int size_mode_;
    // need to calculate position and width
    bool need_resize_;
    // need redraw Pixmap
    bool need_redraw_;
    // paddingxlr = horizontal padding left/right
    // paddingx = horizontal padding between childs
    int padding_x_lr_, padding_x_, padding_y_;
    // parent Area
    Area* parent_;
    // panel
    Panel* panel_;

    // on startup, initialize fixed pos/size
    void InitRendering(int);

    // update area's content and update size (width/height).
    // returns true if size changed, false otherwise.
    virtual bool Resize();

    // generic resize for SIZE_BY_LAYOUT objects
    int ResizeByLayout(int);

    // after pos/size changed, the rendering engine will call on_change_layout()
    int on_changed_;
    virtual void OnChangeLayout();

    virtual std::string GetTooltipText();

    void RemoveArea();
    void AddArea();

    // draw pixmap
    virtual void Draw();
    virtual void DrawBackground(cairo_t*);
    virtual void DrawForeground(cairo_t*);

    // set 'need_redraw' on an area and children
    void SetRedraw();

    void SizeByContent();
    void SizeByLayout(int pos, int level);

    // draw background and foreground
    void Refresh();

    // hide/unhide area
    void Hide();
    void Show();

    void FreeArea();

    bool IsClickInside(int x, int y) const;
};


// draw rounded rectangle
void draw_rect(cairo_t* c, double x, double y, double w, double h, double r);

// clear pixmap with transparent color
void clear_pixmap(Pixmap p, int x, int y, int w, int h);

#endif
