#include "util/timer_test_utils.hh"

FakeClock::FakeClock(unsigned int seconds_from_epoch)
    : current_time_(std::chrono::seconds(seconds_from_epoch)) {}

TimePoint FakeClock::Now() const { return current_time_; }

void FakeClock::AdvanceBy(std::chrono::milliseconds amount) {
  current_time_ += amount;
}

TimePoint FakeClock::SecondsFromEpoch(unsigned int seconds) {
  return TimePoint(std::chrono::seconds(seconds));
}