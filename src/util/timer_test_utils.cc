#include "util/timer_test_utils.hh"

FakeClock::FakeClock(unsigned int seconds_from_epoch)
    : current_time_(absl::FromUnixSeconds(seconds_from_epoch)) {}

absl::Time FakeClock::Now() const { return current_time_; }

void FakeClock::AdvanceBy(absl::Duration amount) { current_time_ += amount; }