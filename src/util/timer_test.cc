#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <chrono>
#include <functional>
#include "util/timer.h"

class FakeClock : public Timer {
 public:
  FakeClock() = delete;
  FakeClock(FakeClock& other) = delete;

  explicit FakeClock(unsigned int seconds_from_epoch)
      : current_time_(std::chrono::seconds(seconds_from_epoch)) {}

  TimePoint Now() const { return current_time_; }

  void AdvanceBy(std::chrono::milliseconds amount) { current_time_ += amount; }

  static TimePoint SecondsFromEpoch(unsigned int seconds) {
    return TimePoint(std::chrono::seconds(seconds));
  }

 private:
  TimePoint current_time_;
};

TEST_CASE("FakeClock", "Test the fake clock has a sane implementation") {
  FakeClock fake_clock{0};
  REQUIRE(fake_clock.Now() == FakeClock::SecondsFromEpoch(0));

  fake_clock.AdvanceBy(std::chrono::seconds(30));
  REQUIRE(fake_clock.Now() == FakeClock::SecondsFromEpoch(30));
}

TEST_CASE("Interval", "Test the Interval class") {
  FakeClock fake_clock{10};
  auto no_op_callback = []() -> bool { return true; };

  SECTION("correctly invokes the registered callback_") {
    bool was_invoked = false;
    Interval interval{fake_clock.Now(), std::chrono::milliseconds(0),
                      [&was_invoked]() -> bool {
                        was_invoked = true;
                        return true;
                      }};

    REQUIRE_FALSE(was_invoked);
    interval.InvokeCallback();
    REQUIRE(was_invoked);
  }

  SECTION("correctly initializes time_point_") {
    Interval interval{fake_clock.Now() + std::chrono::seconds(5),
                      std::chrono::milliseconds(0), no_op_callback};
    REQUIRE(interval.GetTimePoint() == FakeClock::SecondsFromEpoch(15));
  }

  SECTION("correctly initializes repeat_interval_") {
    Duration _100ms = std::chrono::milliseconds(100);
    Interval interval{fake_clock.Now(), _100ms, no_op_callback};
    REQUIRE(interval.GetRepeatInterval() == _100ms);
  }
}

class ChronoTimerTestUtils {
 public:
  virtual ~ChronoTimerTestUtils() = 0;

  static IntervalSet const& GetTimeouts(Timer& timer) {
    return timer.timeouts_;
  }

  static IntervalSet const& GetIntervals(Timer& timer) {
    return timer.intervals_;
  }
};

TEST_CASE("Timer", "Test the Timer class") {
  FakeClock fake_clock{0};
  Timer timer{std::bind(&FakeClock::Now, &fake_clock)};
  auto no_op_callback = []() -> bool { return true; };

  SECTION("correctly registers/unregisters an interval (single)") {
    auto const& timeouts = ChronoTimerTestUtils::GetTimeouts(timer);
    REQUIRE(timeouts.empty());

    Interval* interval =
        timer.SetTimeout(std::chrono::milliseconds(100), no_op_callback);
    REQUIRE(timeouts.size() == 1);

    timer.ClearInterval(interval);
    REQUIRE(timeouts.empty());
  }

  SECTION("correctly registers/unregisters an interval (repeating)") {
    auto const& intervals = ChronoTimerTestUtils::GetIntervals(timer);
    REQUIRE(intervals.empty());

    Interval* interval =
        timer.SetInterval(std::chrono::milliseconds(100), no_op_callback);
    REQUIRE(intervals.size() == 1);

    timer.ClearInterval(interval);
    REQUIRE(intervals.empty());
  }

  SECTION("correctly processes an interval (single)") {
    unsigned int invocations_count = 0;
    timer.SetTimeout(std::chrono::milliseconds(100),
                     [&invocations_count]() -> bool {
                       ++invocations_count;
                       return true;
                     });

    auto const& timeouts = ChronoTimerTestUtils::GetTimeouts(timer);
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 0);
    REQUIRE(timeouts.size() == 1);

    fake_clock.AdvanceBy(std::chrono::milliseconds(300));
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 1);
    REQUIRE(timeouts.empty());
  }

  SECTION("correctly processes an interval (repeating)") {
    unsigned int invocations_count = 0;
    Interval* interval = timer.SetInterval(std::chrono::milliseconds(250),
                                           [&invocations_count]() -> bool {
                                             ++invocations_count;
                                             return true;
                                           });

    auto const& intervals = ChronoTimerTestUtils::GetIntervals(timer);
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 0);
    REQUIRE(intervals.size() == 1);

    fake_clock.AdvanceBy(std::chrono::milliseconds(600));
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 1);
    REQUIRE(intervals.size() == 1);
    REQUIRE(interval->GetTimePoint() ==
            TimePoint(std::chrono::milliseconds(750)));

    fake_clock.AdvanceBy(std::chrono::milliseconds(300));
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 2);
    REQUIRE(intervals.size() == 1);
    REQUIRE(interval->GetTimePoint() ==
            TimePoint(std::chrono::milliseconds(1000)));
  }

  SECTION("correctly returns the next registered interval") {
    // No registered intervals should result in a null pointer
    REQUIRE(timer.GetNextInterval() == nullptr);

    // Unordered insertion should still return the first interval
    Interval* second_timeout =
        timer.SetTimeout(std::chrono::milliseconds(250), no_op_callback);
    Interval* third_timeout =
        timer.SetTimeout(std::chrono::milliseconds(500), no_op_callback);
    Interval* first_timeout =
        timer.SetTimeout(std::chrono::milliseconds(175), no_op_callback);
    REQUIRE(timer.GetNextInterval() == first_timeout);

    // The same applies to repeating intervals
    Interval* first_interval =
        timer.SetInterval(std::chrono::milliseconds(100), no_op_callback);
    REQUIRE(timer.GetNextInterval() == first_interval);
  }
}
