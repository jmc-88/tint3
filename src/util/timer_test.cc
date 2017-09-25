#include "catch.hpp"

#include <chrono>
#include <functional>
#include <limits>

#include "util/timer.hh"
#include "util/timer_test_utils.hh"

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
    Interval interval{123, fake_clock.Now(), std::chrono::milliseconds(0),
                      [&was_invoked]() -> bool {
                        was_invoked = true;
                        return true;
                      }};

    REQUIRE_FALSE(was_invoked);
    interval.InvokeCallback();
    REQUIRE(was_invoked);
  }

  SECTION("correctly initializes time_point_") {
    Interval interval{123, fake_clock.Now() + std::chrono::seconds(5),
                      std::chrono::milliseconds(0), no_op_callback};
    REQUIRE(interval.GetTimePoint() == FakeClock::SecondsFromEpoch(15));
  }

  SECTION("correctly initializes repeat_interval_") {
    Duration _100ms = std::chrono::milliseconds(100);
    Interval interval{123, fake_clock.Now(), _100ms, no_op_callback};
    REQUIRE(interval.GetRepeatInterval() == _100ms);
  }

  SECTION("correctly assigns to an interval") {
    Interval first_interval{1, fake_clock.Now(), std::chrono::milliseconds(0),
                            no_op_callback};
    Interval second_interval{2, fake_clock.Now(), std::chrono::milliseconds(50),
                             no_op_callback};
    REQUIRE(first_interval.GetRepeatInterval() !=
            second_interval.GetRepeatInterval());
    first_interval = second_interval;
    REQUIRE(first_interval.GetRepeatInterval() ==
            std::chrono::milliseconds(50));
    REQUIRE(second_interval.GetRepeatInterval() ==
            std::chrono::milliseconds(50));
  }
}

TEST_CASE("CompareIds", "Sanity of nullable ids comparisons") {
  CompareIds compare_fn;
  // Left-hand side is nulled-out: right-hand side always comes first.
  REQUIRE(!compare_fn(Interval::Id{}, Interval::Id{0}));
  // Right-hand side is nulled-out: left-hand side always comes first.
  REQUIRE(compare_fn(Interval::Id{0}, Interval::Id{}));
  // Smaller left-hand side comes first.
  REQUIRE(compare_fn(Interval::Id{0}, Interval::Id{1}));
  // Smaller right-hand side comes first.
  REQUIRE(!compare_fn(Interval::Id{1}, Interval::Id{0}));
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
  Timer timer{[&]() { return fake_clock.Now(); }};
  auto no_op_callback = []() -> bool { return true; };

  SECTION("correctly registers/unregisters an interval (single)") {
    auto const& timeouts = ChronoTimerTestUtils::GetTimeouts(timer);
    REQUIRE(timeouts.empty());

    Interval::Id interval =
        timer.SetTimeout(std::chrono::milliseconds(100), no_op_callback);
    REQUIRE(timeouts.size() == 1);

    REQUIRE(timer.ClearInterval(interval));
    REQUIRE(timeouts.empty());
  }

  SECTION("correctly registers/unregisters an interval (repeating)") {
    auto const& intervals = ChronoTimerTestUtils::GetIntervals(timer);
    REQUIRE(intervals.empty());

    Interval::Id interval =
        timer.SetInterval(std::chrono::milliseconds(100), no_op_callback);
    REQUIRE(intervals.size() == 1);

    REQUIRE(timer.ClearInterval(interval));
    REQUIRE(intervals.empty());
  }

  SECTION("fails clearing a non-existing interval") {
    Interval::Id nulled_interval;
    REQUIRE(!timer.ClearInterval(nulled_interval));
    Interval::Id no_such_interval{std::numeric_limits<uint64_t>::max()};
    REQUIRE(!timer.ClearInterval(no_such_interval));
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
    timer.SetInterval(std::chrono::milliseconds(250),
                      [&invocations_count]() -> bool {
                        ++invocations_count;
                        return true;
                      });

    // time: 0 ms
    // next invocation: >= 250 ms
    // no invocations yet, interval is present
    auto const& intervals = ChronoTimerTestUtils::GetIntervals(timer);
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 0);
    REQUIRE(intervals.size() == 1);

    // time: skip to 600 ms
    // next invocation: >= 750 ms
    // 1 invocation, interval was not erased
    fake_clock.AdvanceBy(std::chrono::milliseconds(600));
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 1);
    REQUIRE(intervals.size() == 1);

    // time: skip to 900 ms
    // next invocation: >= 1000 ms
    // 2 invocations, interval was not erased
    fake_clock.AdvanceBy(std::chrono::milliseconds(300));
    timer.ProcessExpiredIntervals();
    REQUIRE(invocations_count == 2);
    REQUIRE(intervals.size() == 1);
  }

  SECTION("correctly returns the next registered interval") {
    // No registered intervals should result in a nulled-out object
    REQUIRE(timer.GetNextInterval() == false);

    // Unordered insertion should still return the first interval
    timer.SetTimeout(std::chrono::milliseconds(250), no_op_callback);
    timer.SetTimeout(std::chrono::milliseconds(500), no_op_callback);
    timer.SetTimeout(std::chrono::milliseconds(175), no_op_callback);

    auto next_timeout = timer.GetNextInterval();
    REQUIRE(next_timeout);
    REQUIRE(next_timeout.Unwrap().GetTimePoint() ==
            TimePoint(std::chrono::milliseconds(175)));

    // The same applies to repeating intervals
    timer.SetInterval(std::chrono::milliseconds(100), no_op_callback);

    auto next_interval = timer.GetNextInterval();
    REQUIRE(next_interval);
    REQUIRE(next_interval.Unwrap().GetRepeatInterval() ==
            Duration(std::chrono::milliseconds(100)));
  }
}
