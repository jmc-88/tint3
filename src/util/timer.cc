/**************************************************************************
 *
 * Copyright (C) 2009 Andreas.Fink (Andreas.Fink85@gmail.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 *USA.
 **************************************************************************/

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <list>
#include <map>
#include <utility>

#include "absl/time/clock.h"

#include "util/log.hh"
#include "util/timer.hh"

namespace {

// Atomic counter for Interval identifiers.
std::atomic<uint64_t> interval_id_counter;

}  // namespace

Interval::Interval() {}

Interval::Interval(Interval::Id interval_id, absl::Time time_point,
                   absl::Duration repeat_interval, Callback callback)
    : id_(interval_id),
      time_point_(time_point),
      repeat_interval_(repeat_interval),
      callback_(callback) {}

Interval::Interval(Interval const& other)
    : id_(other.id_),
      time_point_(other.time_point_),
      repeat_interval_(other.repeat_interval_),
      callback_(other.callback_) {}

Interval::Interval(Interval&& other)
    : id_(std::move(other.id_)),
      time_point_(std::move(other.time_point_)),
      repeat_interval_(std::move(other.repeat_interval_)),
      callback_(std::move(other.callback_)) {}

Interval& Interval::operator=(Interval other) {
  std::swap(id_, other.id_);
  std::swap(time_point_, other.time_point_);
  std::swap(repeat_interval_, other.repeat_interval_);
  std::swap(callback_, other.callback_);
  return *this;
}

void Interval::InvokeCallback() const { callback_(); }

absl::Time Interval::GetTimePoint() const { return time_point_; }

absl::Duration Interval::GetRepeatInterval() const { return repeat_interval_; }

std::ostream& operator<<(std::ostream& os, Interval const& interval) {
  os << '[' << interval.GetTimePoint();

  auto ms = absl::ToInt64Milliseconds(interval.GetRepeatInterval());
  if (ms != 0) os << "; " << ms << " ms";

  os << ']';
  return os;
}

bool operator<(Interval const& lhs, Interval const& rhs) {
  return lhs.GetTimePoint() < rhs.GetTimePoint();
}

bool CompareIds::operator()(Interval::Id const& lhs,
                            Interval::Id const& rhs) const {
  if (!rhs) {
    return true;
  }
  if (!lhs) {
    return false;
  }
  return lhs.value() < rhs.value();
}

bool CompareIntervals::operator()(Interval const& lhs,
                                  Interval const& rhs) const {
  if (lhs.GetTimePoint() < rhs.GetTimePoint()) {
    return true;
  }
  if (lhs.GetTimePoint() > rhs.GetTimePoint()) {
    return false;
  }
  return CompareIds()(lhs.id_, rhs.id_);
}

Timer::Timer() : get_current_time_(absl::Now) {}

Timer::Timer(TimerCallback get_current_time_callback)
    : get_current_time_(get_current_time_callback) {}

absl::Time Timer::Now() const { return get_current_time_(); }

Interval::Id Timer::SetTimeout(absl::Duration timeout_interval,
                               Interval::Callback callback) {
  Interval::Id id{++interval_id_counter};
  timeouts_.insert(id, Interval{id, Now() + timeout_interval,
                                absl::Milliseconds(0), std::move(callback)});
  return id;
}

Interval::Id Timer::SetInterval(absl::Duration repeat_interval,
                                Interval::Callback callback) {
  Interval::Id id{++interval_id_counter};
  intervals_.insert(id, Interval{id, Now() + repeat_interval, repeat_interval,
                                 std::move(callback)});
  return id;
}

bool Timer::ClearInterval(Interval::Id interval_id) {
  if (timeouts_.left.erase(interval_id)) {
    return true;
  }
  if (intervals_.left.erase(interval_id)) {
    return true;
  }
  return false;
}

void Timer::ProcessExpiredIntervals() {
  absl::Time now = get_current_time_();

  while (true) {
    auto it = timeouts_.right.begin();
    if (it == timeouts_.right.end()) {
      break;
    }

    Interval const& interval = it->first;
    if (interval.time_point_ > now) {
      break;
    }
    interval.InvokeCallback();
    timeouts_.right.erase(interval);
  }

  while (true) {
    auto it = intervals_.right.begin();
    if (it == intervals_.right.end()) {
      break;
    }

    Interval interval{it->first};
    if (interval.time_point_ > now) {
      break;
    }
    bool should_keep = interval.callback_();
    intervals_.right.erase(interval);
    if (should_keep) {
      do {
        interval.time_point_ += interval.repeat_interval_;
      } while (interval.time_point_ < now);
      intervals_.insert(interval.id_, interval);
    }
  }
}

absl::optional<Interval> Timer::GetNextInterval() const {
  if (timeouts_.empty() && intervals_.empty()) {
    return {};
  }
  if (intervals_.empty()) {
    return timeouts_.right.begin()->first;
  }
  if (timeouts_.empty()) {
    return intervals_.right.begin()->first;
  }
  return std::min(timeouts_.right.begin()->first,
                  intervals_.right.begin()->first);
}
