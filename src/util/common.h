/**************************************************************************
* Common declarations
*
**************************************************************************/

#ifndef COMMON_H
#define COMMON_H


#define WM_CLASS_TINT   "panel"

#include <Imlib2.h>
#include <signal.h>

#include <functional>
#include <sstream>
#include <string>

#include "util/area.h"

// mouse actions
enum MouseActionEnum {
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
    std::ostringstream ss_;

  public:
    template<typename T>
    StringBuilder& operator<<(T const& value) {
        ss_ << value;
        return (*this);
    }

    operator std::string() const {
        return ss_.str();
    }
};


std::string GetEnvironment(std::string const& variable_name);

bool SignalAction(int signal_number,
                  std::function<void(int)> signal_handler,
                  int flags = 0);

std::string& StringTrim(std::string& str);

// Wrapper around strtol()
long int StringToLongInt(std::string const& str, char** endptr = nullptr);

// Wrapper around strtof()
float StringToFloat(std::string const& str, char** endptr = nullptr);

template<typename T>
std::string StringRepresentation(T const& value) {
    return StringBuilder() << value;
}

// execute a command by calling fork
void TintExec(std::string const& command);


// color conversion
bool GetColor(const std::string& hex, double* rgb);

void ExtractValues(const std::string& value, std::string& v1, std::string& v2,
                   std::string& v3);

// adjust Alpha/Saturation/Brightness on an ARGB icon
// alpha from 0 to 100, satur from 0 to 1, bright from 0 to 1.
void AdjustAsb(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float satur,
               float bright);
void CreateHeuristicMask(DATA32* data, int w, int h);

void RenderImage(Drawable d, int x, int y, int w, int h);

#endif
