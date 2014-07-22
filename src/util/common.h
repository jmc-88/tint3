/**************************************************************************
* Common declarations
*
**************************************************************************/

#ifndef COMMON_H
#define COMMON_H


#define WM_CLASS_TINT   "panel"

#include <Imlib2.h>
#include "area.h"

// mouse actions
enum {
    NONE = 0,
    CLOSE,
    TOGGLE,
    ICONIFY,
    SHADE,
    TOGGLE_ICONIFY,
    MAXIMIZE_RESTORE,
    MAXIMIZE,
    RESTORE,
    DESKTOP_LEFT,
    DESKTOP_RIGHT,
    NEXT_TASK,
    PREV_TASK
};

extern int const ALLDESKTOP;


bool copy_file(char const* from_path, char const* to_path);

// extract key = value
int parse_line(char* line, char** key, char** value);

// execute a command by calling fork
void tint_exec(const char* command);


// conversion
int hex_char_to_int(char c);
bool hex_to_rgb(char const* hex, int* r, int* g, int* b);
bool get_color(char const* hex, double* rgb);

void extract_values(char* value, char** value1, char** value2, char** value3);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void adjust_asb(DATA32* data, unsigned int w, unsigned int h, int alpha,
                float satur,
                float bright);
void createHeuristicMask(DATA32* data, int w, int h);

void render_image(Drawable d, int x, int y, int w, int h);
#endif

