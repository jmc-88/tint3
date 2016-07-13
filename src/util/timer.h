#ifndef TINT3_UTIL_TIMER_H
#define TINT3_UTIL_TIMER_H

#include <sys/select.h>

#include <chrono>
#include <functional>
#include <memory>
#include <set>

using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::nanoseconds;

class ChronoTimer;
class Interval {
 public:
  friend class ChronoTimer;
  using Callback = std::function<bool()>;

  Interval() = delete;
  Interval(TimePoint time_point, Duration repeat_interval, Callback callback);

  void InvokeCallback() const;
  TimePoint GetTimePoint() const;
  Duration GetRepeatInterval() const;

 private:
  TimePoint time_point_;
  Duration repeat_interval_;
  Callback callback_;
};

class CompareIntervals {
 public:
  bool operator()(Interval const* lhs, Interval const* rhs) const;
};

std::unique_ptr<struct timeval> ToTimeval(Duration duration);

using IntervalSet = std::set<Interval*, CompareIntervals>;

class ChronoTimerTestUtils;
class ChronoTimer {
 public:
  friend class ChronoTimerTestUtils;
  using TimerCallback = std::function<TimePoint()>;

  ChronoTimer();
  ChronoTimer(TimerCallback get_current_time_callback);
  ~ChronoTimer();

  // Returns the current time point as given by the registered callback.
  TimePoint Now() const;

  // Registers a new single-shot callback.
  // Will be called at (or after) now + timeout_interval.
  //
  // DO NOT call ClearInterval() from inside the registered callback function,
  // as this kind of timeout is removed automatically after having been
  // processed. The return value of the callback function is ignored.
  Interval* SetTimeout(Duration timeout_interval, Interval::Callback callback);

  // Registers a new single-shot callback.
  // Will be called at (or after) now + repeat_interval, and the next callback
  // time point will be adjusted accordingly for the next invocation.
  //
  // DO NOT call ClearInterval() from inside the registered callback function;
  // instead, have the callback function return a boolean indicating whether
  // the interval should be kept (true) or deleted (false);
  Interval* SetInterval(Duration repeat_interval, Interval::Callback callback);

  // Unregister the given interval from the timeout/repeat queues.
  //
  // DO NOT call this method from inside a timer callback function; see the
  // remarks for SetTimeout() and SetInterval() for info.
  bool ClearInterval(Interval* interval_id);

  // Go over the timeouts/repeat queues and invoke the callback functions for
  // intervals that have expired. Handles removing the expired intervals and
  // updating the expiration times.
  void ProcessExpiredIntervals();

  // Returns the next registered interval, if any, otherwise nullptr.
  Interval* GetNextInterval() const;

 private:
  TimerCallback get_current_time_;

  IntervalSet timeouts_;
  IntervalSet intervals_;
};

#endif  // TINT3_UTIL_TIMER_H
