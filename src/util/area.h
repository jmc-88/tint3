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
enum {
    SIZE_BY_LAYOUT,
    SIZE_BY_CONTENT
};

class Panel;
class Area {
  public:
    virtual ~Area();

    Area& Clone(Area const&);

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
    bool need_resize;
    // need redraw Pixmap
    bool need_redraw;
    // paddingxlr = horizontal padding left/right
    // paddingx = horizontal padding between childs
    int paddingxlr, paddingx, paddingy;
    // parent Area
    Area* parent;
    // panel
    Panel* panel;

    // on startup, initialize fixed pos/size
    void InitRendering(int);

    // update area's content and update size (width/height).
    // returns true if size changed, false otherwise.
    virtual bool Resize();

    // generic resize for SIZE_BY_LAYOUT objects
    int ResizeByLayout(int);

    // after pos/size changed, the rendering engine will call on_change_layout()
    int on_changed;
    virtual void OnChangeLayout();

    virtual const char* GetTooltipText();

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
};


// draw rounded rectangle
void draw_rect(cairo_t* c, double x, double y, double w, double h, double r);

// clear pixmap with transparent color
void clear_pixmap(Pixmap p, int x, int y, int w, int h);

#endif

