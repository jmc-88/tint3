#ifndef TINT3_UTIL_LOG_H
#define TINT3_UTIL_LOG_H

#include <ostream>

namespace util {
namespace log {

class Logger {
    friend Logger& Debug();
    friend Logger& Error();

  public:
    enum LoggerMode {
        kDefault = 0,
        kDisabled = 1,
        kAutoFlush = 2,
    };

  private:
    std::ostream& stream_;
    LoggerMode mode_;

    Logger(std::ostream& stream, LoggerMode mode = kDefault)
        : stream_(stream)
        , mode_(mode) {
    }

  public:
    template<typename V>
    Logger& operator<<(V const& val) {
        if (!(mode_ & kDisabled)) {
            stream_ << val;

            if (mode_ & kAutoFlush) {
                stream_.flush();
            }
        }

        return (*this);
    }
};

Logger& Debug();
Logger& Error();

} // namespace log
} // namespace util

#endif // TINT3_UTIL_LOG_H