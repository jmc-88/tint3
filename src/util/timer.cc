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
#include <list>
#include <map>
#include <utility>

#include "util/log.hh"
#include "util/timer.hh"

namespace {

// Atomic counter for Interval identifiers.
std::atomic<uint64_t> interval_id_counter;

}  // namespace

Interval::Interval() {}

Interval::Interval(TimePoint time_point, Duration repeat_interval,
                   Callback callback)
    : time_point_(time_point),
      repeat_interval_(repeat_interval),
      callback_(callback) {}

Interval::Interval(Interval const& other)
    : time_point_(other.time_point_),
      repeat_interval_(other.repeat_interval_),
      callback_(other.callback_) {}

Interval::Interval(Interval&& other)
    : time_point_(std::move(other.time_point_)),
      repeat_interval_(std::move(other.repeat_interval_)),
      callback_(std::move(other.callback_)) {}

Interval& Interval::operator=(Interval other) {
  std::swap(time_point_, other.time_point_);
  std::swap(repeat_interval_, other.repeat_interval_);
  std::swap(callback_, other.callback_);
  return *this;
}

void Interval::InvokeCallback() const { callback_(); }

TimePoint Interval::GetTimePoint() const { return time_point_; }

Duration Interval::GetRepeatInterval() const { return repeat_interval_; }

bool operator<(Interval const& lhs, Interval const& rhs) {
  return lhs.GetTimePoint() < rhs.GetTimePoint();
}

bool CompareIds::operator()(Interval::Id const& lhs,
                            Interval::Id const& rhs) const {
  return static_cast<uint64_t>(lhs) < static_cast<uint64_t>(rhs);
}

std::unique_ptr<struct timeval> ToTimeval(Duration duration) {
  std::chrono::microseconds const usec =
      std::chrono::duration_cast<std::chrono::microseconds>(duration);
  struct timeval* tv = nullptr;

  if (usec != std::chrono::microseconds(0)) {
    tv = new struct timeval;
    tv->tv_sec =
        std::chrono::duration_cast<std::chrono::seconds>(duration - usec)
            .count();
    tv->tv_usec = usec.count();
  }

  return std::unique_ptr<struct timeval>(tv);
}

Timer::Timer() : get_current_time_(std::chrono::steady_clock::now) {}

Timer::Timer(TimerCallback get_current_time_callback)
    : get_current_time_(get_current_time_callback) {}

TimePoint Timer::Now() const { return get_current_time_(); }

Interval::Id Timer::SetTimeout(Duration timeout_interval,
                               Interval::Callback callback) {
  Interval::Id id{++interval_id_counter};
  timeouts_.insert(std::make_pair(
      id, Interval{Now() + timeout_interval, std::chrono::milliseconds(0),
                   std::move(callback)}));
  return id;
}

Interval::Id Timer::SetInterval(Duration repeat_interval,
                                Interval::Callback callback) {
  Interval::Id id{++interval_id_counter};
  intervals_.insert(std::make_pair(
      id,
      Interval{Now() + repeat_interval, repeat_interval, std::move(callback)}));
  return id;
}

bool Timer::ClearInterval(Interval::Id interval_id) {
  auto timeouts_it = timeouts_.find(interval_id);
  if (timeouts_it != timeouts_.end()) {
    timeouts_.erase(timeouts_it);
    return true;
  }

  auto intervals_it = intervals_.find(interval_id);
  if (intervals_it != intervals_.end()) {
    intervals_.erase(intervals_it);
    return true;
  }

  return false;
}

void Timer::ProcessExpiredIntervals() {
  TimePoint now = get_current_time_();

  for (auto it = timeouts_.begin(); it != timeouts_.end();) {
    Interval& interval = it->second;

    if (interval.time_point_ <= now) {
      interval.InvokeCallback();
      it = timeouts_.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = intervals_.begin(); it != intervals_.end();) {
    Interval& interval = it->second;

    if (interval.time_point_ <= now) {
      bool should_keep = interval.callback_();
      if (should_keep) {
        do {
          interval.time_point_ += interval.repeat_interval_;
        } while (interval.time_point_ < now);
        ++it;
      } else {
        it = timeouts_.erase(it);
      }
    } else {
      ++it;
    }
  }
}

util::Nullable<Interval> Timer::GetNextInterval() const {
  auto timeout_it = timeouts_.begin();
  auto interval_it = intervals_.begin();
  util::Nullable<Interval> next;

  while (timeout_it != timeouts_.end() || interval_it != intervals_.end()) {
    if (timeout_it == timeouts_.end()) {
      if (!next) {
        next = interval_it->second;
      } else {
        next = std::min(next.Unwrap(), interval_it->second);
      }
      ++interval_it;
    } else if (interval_it == intervals_.end()) {
      if (!next) {
        next = timeout_it->second;
      } else {
        next = std::min(next.Unwrap(), timeout_it->second);
      }
      ++timeout_it;
    } else {
      if (!next) {
        next = std::min(timeout_it->second, interval_it->second);
      } else {
        next =
            std::min({next.Unwrap(), timeout_it->second, interval_it->second});
      }
      ++timeout_it;
      ++interval_it;
    }
  }

  return next;
}
