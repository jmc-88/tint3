#ifndef TINT3_UTIL_TIMER_HH
#define TINT3_UTIL_TIMER_HH

#include <sys/select.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <map>

#include "util/nullable.hh"

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::nanoseconds;

class Timer;
class Interval {
 public:
  friend class Timer;
  friend bool operator<(Interval const& lhs, Interval const& rhs);

  using Id = util::Nullable<uint64_t>;
  using Callback = std::function<bool()>;

  Interval();
  Interval(TimePoint time_point, Duration repeat_interval, Callback callback);
  Interval(Interval const& other);
  Interval(Interval&& other);

  Interval& operator=(Interval other);

  void InvokeCallback() const;
  TimePoint GetTimePoint() const;
  Duration GetRepeatInterval() const;

 private:
  TimePoint time_point_;
  Duration repeat_interval_;
  Callback callback_;
};

bool operator<(Interval const& lhs, Interval const& rhs);

class CompareIds {
 public:
  bool operator()(Interval::Id const& lhs, Interval::Id const& rhs) const;
};

std::unique_ptr<struct timeval> ToTimeval(Duration duration);

using IntervalSet = std::map<Interval::Id, Interval, CompareIds>;

class ChronoTimerTestUtils;
class Timer {
 public:
  friend class ChronoTimerTestUtils;
  using TimerCallback = std::function<TimePoint()>;

  Timer();
  Timer(TimerCallback get_current_time_callback);

  // Returns the current time point as given by the registered callback.
  TimePoint Now() const;

  // Registers a new single-shot callback.
  // Will be called at (or after) now + timeout_interval.
  //
  // DO NOT call ClearInterval() from inside the registered callback function,
  // as this kind of timeout is removed automatically after having been
  // processed. The return value of the callback function is ignored.
  Interval::Id SetTimeout(Duration timeout_interval,
                          Interval::Callback callback);

  // Registers a new single-shot callback.
  // Will be called at (or after) now + repeat_interval, and the next callback
  // time point will be adjusted accordingly for the next invocation.
  //
  // DO NOT call ClearInterval() from inside the registered callback function;
  // instead, have the callback function return a boolean indicating whether
  // the interval should be kept (true) or deleted (false);
  Interval::Id SetInterval(Duration repeat_interval,
                           Interval::Callback callback);

  // Unregister the given interval from the timeout/repeat queues.
  //
  // DO NOT call this method from inside a timer callback function; see the
  // remarks for SetTimeout() and SetInterval() for info.
  bool ClearInterval(Interval::Id interval_id);

  // Go over the timeouts/repeat queues and invoke the callback functions for
  // intervals that have expired. Handles removing the expired intervals and
  // updating the expiration times.
  void ProcessExpiredIntervals();

  // Returns the next registered interval, if any, or a nulled-out object.
  util::Nullable<Interval> GetNextInterval() const;

 private:
  TimerCallback get_current_time_;

  IntervalSet timeouts_;
  IntervalSet intervals_;
};

#endif  // TINT3_UTIL_TIMER_HH
