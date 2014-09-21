/**************************************************************************
* Common declarations
*
**************************************************************************/

#ifndef COMMON_H
#define COMMON_H


#define WM_CLASS_TINT   "panel"

#include <Imlib2.h>
#include "area.h"

#include <sstream>
#include <string>

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

class StringBuilder {
    std::ostringstream ss;

  public:
    template<typename T>
    StringBuilder& operator<<(T const& value) {
        ss << value;
        return (*this);
    }

    operator std::string() const {
        return ss.str();
    }
};

std::string GetEnvironment(std::string const& variable_name);

std::string& StringTrim(std::string& str);

template<typename T>
std::string StringRepresentation(T const& value) {
    return StringBuilder() << value;
}

// execute a command by calling fork
void TintExec(std::string const& command);


// color conversion
bool GetColor(char const* hex, double* rgb);

void ExtractValues(char* value, char** value1, char** value2, char** value3);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustAsb(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float satur,
               float bright);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Drawable d, int x, int y, int w, int h);

#endif
