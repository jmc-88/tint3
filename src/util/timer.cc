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
#include <list>
#include <map>

#include "util/log.hh"
#include "util/timer.hh"

Interval::Interval(TimePoint time_point, Duration repeat_interval,
                   Callback callback)
    : time_point_(time_point),
      repeat_interval_(repeat_interval),
      callback_(callback) {}

void Interval::InvokeCallback() const { callback_(); }

TimePoint Interval::GetTimePoint() const { return time_point_; }

Duration Interval::GetRepeatInterval() const { return repeat_interval_; }

bool CompareIntervals::operator()(Interval const* lhs,
                                  Interval const* rhs) const {
  return lhs->GetTimePoint() < rhs->GetTimePoint();
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

Timer::~Timer() {
  for (Interval* interval : timeouts_) {
    delete interval;
  }
  timeouts_.clear();

  for (Interval* interval : intervals_) {
    delete interval;
  }
  intervals_.clear();
}

TimePoint Timer::Now() const { return get_current_time_(); }

Interval* Timer::SetTimeout(Duration timeout_interval,
                            Interval::Callback callback) {
  Interval* interval =
      new Interval(Now() + timeout_interval, std::chrono::milliseconds(0),
                   std::move(callback));
  if (interval) {
    timeouts_.insert(interval);
  }
  return interval;
}

Interval* Timer::SetInterval(Duration repeat_interval,
                             Interval::Callback callback) {
  Interval* interval = new Interval(Now() + repeat_interval, repeat_interval,
                                    std::move(callback));
  if (interval) {
    intervals_.insert(interval);
  }
  return interval;
}

bool Timer::ClearInterval(Interval* interval) {
  size_t erased_count = 0;

  if (interval->repeat_interval_ == interval->repeat_interval_.zero()) {
    erased_count = timeouts_.erase(interval);
  } else {
    erased_count = intervals_.erase(interval);
  }

  if (erased_count != 0) {
    delete interval;
    return true;
  }

  return false;
}

void Timer::ProcessExpiredIntervals() {
  TimePoint now = get_current_time_();

  for (auto it = timeouts_.begin(); it != timeouts_.end();) {
    Interval* interval = (*it);

    if (interval->time_point_ <= now) {
      interval->InvokeCallback();
      delete interval;
      it = timeouts_.erase(it);
    } else {
      ++it;
    }
  }

  for (auto it = intervals_.begin(); it != intervals_.end();) {
    Interval* interval = (*it);

    if (interval->time_point_ <= now) {
      bool should_keep = interval->callback_();
      if (should_keep) {
        do {
          interval->time_point_ += interval->repeat_interval_;
        } while (interval->time_point_ < now);
        ++it;
      } else {
        delete interval;
        it = timeouts_.erase(it);
      }
    } else {
      ++it;
    }
  }
}

Interval* Timer::GetNextInterval() const {
  auto first_timeout = timeouts_.begin();
  auto first_interval = intervals_.begin();

  if (first_timeout == timeouts_.end()) {
    if (first_interval == intervals_.end()) {
      return nullptr;
    }
    return (*first_interval);
  } else if (first_interval == intervals_.end()) {
    if (first_timeout == timeouts_.end()) {
      return nullptr;
    }
    return (*first_timeout);
  } else {
    CompareIntervals less_than;
    return (less_than(*first_timeout, *first_interval) ? (*first_timeout)
                                                       : (*first_interval));
  }
}
