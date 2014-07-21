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

#include <vector>


typedef struct {
    double color[3];
    double alpha;
    int width;
    int rounded;
} Border;


typedef struct {
    double color[3];
    double alpha;
} Color;

typedef struct {
    Color back;
    Border border;
} Background;


// way to calculate the size
// SIZE_BY_LAYOUT objects : taskbar and task
// SIZE_BY_CONTENT objects : clock, battery, launcher, systray
enum { SIZE_BY_LAYOUT, SIZE_BY_CONTENT };

class Panel;
class Area {
  public:
    Area& clone(Area const&);

    // coordinate relative to panel window
    int posx, posy;
    // width and height including border
    int width, height;
    Pixmap pix;
    Background* bg;

    // list of children Area objects
    std::vector<Area*> children;

    // object visible on screen.
    // An object (like systray) could be enabled but hidden (because no tray icon).
    int on_screen;
    // way to calculate the size (SIZE_BY_CONTENT or SIZE_BY_LAYOUT)
    int size_mode;
    // need to calculate position and width
    int resize;
    // need redraw Pixmap
    int redraw;
    // paddingxlr = horizontal padding left/right
    // paddingx = horizontal padding between childs
    int paddingxlr, paddingx, paddingy;
    // parent Area
    Area* parent;
    // panel
    Panel* panel;

    // each object can overwrite following function
    void (*_draw_foreground)(void* obj, cairo_t* c);
    // update area's content and update size (width/heith).
    // return '1' if size changed, '0' otherwise.
    int (*_resize)(void* obj);
    // after pos/size changed, the rendering engine will call _on_change_layout(Area*)
    int on_changed;
    void (*_on_change_layout)(void* obj);
    const char* (*_get_tooltip_text)(void* obj);

    void remove_area();
    void add_area();

    // draw pixmap
    void draw();
    void draw_background(cairo_t* c);

    // set 'redraw' on an area and children
    void set_redraw();

    void size_by_content();
    void size_by_layout(int pos, int level);

    // draw background and foreground
    void refresh();

    // hide/unhide area
    void hide();
    void show();

    void free_area();
};

// on startup, initialize fixed pos/size
void init_rendering(void* obj, int pos);

// generic resize for SIZE_BY_LAYOUT objects
int resize_by_layout(void* obj, int maximum_size);

// draw rounded rectangle
void draw_rect(cairo_t* c, double x, double y, double w, double h, double r);

// clear pixmap with transparent color
void clear_pixmap(Pixmap p, int x, int y, int w, int h);

#endif

