#include "x11.h"

namespace util {
namespace x11 {

ScopedErrorHandler::ScopedErrorHandler(XErrorHandler new_handler)
    : old_handler_(XSetErrorHandler(new_handler)) {
}

ScopedErrorHandler::~ScopedErrorHandler() {
    XSetErrorHandler(old_handler_);
}

}  // namespace x11
}  // namespace util
