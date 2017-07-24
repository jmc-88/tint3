#ifndef TINT3_UTIL_TIMER_TEST_UTILS_HH
#define TINT3_UTIL_TIMER_TEST_UTILS_HH

#include <chrono>
#include "util/timer.hh"

class FakeClock : public Timer {
 public:
  FakeClock() = delete;
  FakeClock(FakeClock& other) = delete;

  explicit FakeClock(unsigned int seconds_from_epoch);

  TimePoint Now() const;
  void AdvanceBy(std::chrono::milliseconds amount);

  static TimePoint SecondsFromEpoch(unsigned int seconds);

 private:
  TimePoint current_time_;
};

#endif  // TINT3_UTIL_TIMER_TEST_UTILS_HH