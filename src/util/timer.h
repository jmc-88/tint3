#ifndef TINT3_UTIL_TIMER_H
#define TINT3_UTIL_TIMER_H

#include <sys/select.h>

#include <chrono>
#include <functional>
#include <memory>
#include <set>

extern struct timeval next_timeout;

struct _multi_timeout;
typedef struct _multi_timeout MultiTimeout;

typedef struct {
  int interval_msec;
  struct timespec timeout_expires;
  std::function<void()> callback;
  MultiTimeout* multi_timeout;
} Timeout;

// timer functions
/**
  * Single shot timer (i.e. timer with interval_msec == 0) are deleted
*automatically as soon as they expire
  * i.e. you do not need to stop them, however it is safe to call stop_timeout
*for these timers.
  * Periodic timeouts are aligned to each other whenever possible, i.e. one
*interval_msec is an
  * integral multiple of the other.
**/

/** default global data **/
void DefaultTimeout();

/** freed memory : stops all timeouts **/
void CleanupTimeout();

/** installs a timeout with the first timeout of 'value_msec' and then a
*periodic timeout with
  * 'interval_msec'. 'callback' is the callback function when the timer reaches
*the timeout.
  * returns a pointer to the timeout, which is needed for stopping it again
**/
Timeout* AddTimeout(int value_msec, int interval_msec,
                    std::function<void()> callback);

/** changes timeout 't'. If timeout 't' does not exist, nothing happens **/
void ChangeTimeout(Timeout* t, int value_msec, int interval_msec,
                   std::function<void()> callback);

/** stops the timeout 't' **/
void StopTimeout(Timeout* t);

/** update_next_timeout updates next_timeout to the value, when the next
 * installed timeout will expire **/
struct timeval* UpdateNextTimeout();

/** Callback of all expired timeouts **/
void CallbackTimeoutExpired();

// Rewrite using std::chrono
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
