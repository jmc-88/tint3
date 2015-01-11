#include <fstream>
#include <iostream>

#include "log.h"

namespace {

const util::log::Logger::LoggerMode kDebugLogMode =
#ifndef _TINT3_DEBUG
    util::log::Logger::kDisabled;
#else
    util::log::Logger::kAutoFlush;
#endif  // _TINT3_DEBUG

}  // namespace

namespace util {

namespace log {

Logger& Debug() {
  static std::ofstream stream("/tmp/tint3.dbg",
                              std::ofstream::out | std::ofstream::app);
  static Logger debug(stream, kDebugLogMode);
  return debug;
}

Logger& Error() {
  static Logger error(std::cerr, util::log::Logger::kAutoFlush);
  return error;
}

}  // namespace log

}  // namespace util
