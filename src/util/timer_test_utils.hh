#ifndef TINT3_UTIL_TIMER_TEST_UTILS_HH
#define TINT3_UTIL_TIMER_TEST_UTILS_HH

#include "absl/time/time.h"

#include "util/timer.hh"

class FakeClock : public Timer {
 public:
  FakeClock() = delete;
  FakeClock(FakeClock& other) = delete;

  explicit FakeClock(unsigned int seconds_from_epoch);

  absl::Time Now() const;
  void AdvanceBy(absl::Duration amount);

 private:
  absl::Time current_time_;
};

#endif  // TINT3_UTIL_TIMER_TEST_UTILS_HH