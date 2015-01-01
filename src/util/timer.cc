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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include <algorithm>
#include <list>
#include <map>

#include "timer.h"


// functions and structs for multi timeouts
struct _multi_timeout {
    int current_count;
    int count_to_expiration;
};

typedef struct {
    std::list<Timeout*> timeout_list;
    Timeout* parent_timeout;
} MultiTimeoutHandler;

std::list<Timeout*> timeout_list;
struct timeval next_timeout;
std::map<Timeout*, MultiTimeoutHandler*> multi_timeouts;

namespace {

void UpdateMultiTimeoutValues(MultiTimeoutHandler* mth);

int CompareTimespecs(struct timespec const& t1, struct timespec const& t2) {
    if (t1.tv_sec < t2.tv_sec) {
        return -1;
    }

    if (t1.tv_sec == t2.tv_sec) {
        if (t1.tv_nsec < t2.tv_nsec) {
            return -1;
        }

        if (t1.tv_nsec == t2.tv_nsec) {
            return 0;
        }

        return 1;
    }

    return 1;
}

bool CompareTimeouts(Timeout const* t1, Timeout const* t2) {
    return (CompareTimespecs(t1->timeout_expires, t2->timeout_expires) < 0);
}

struct timespec AddMsecToTimespec(struct timespec ts, int msec) {
    ts.tv_sec += msec / 1000;
    ts.tv_nsec += (msec % 1000) * 1000000;

    if (ts.tv_nsec >= 1000000000) {  // 10^9
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }

    return ts;
}

void CreateMultiTimeout(Timeout* t1, Timeout* t2) {
    auto mt1 = new MultiTimeout();
    auto mt2 = new MultiTimeout();
    auto real_timeout = new Timeout();

    auto mth = new MultiTimeoutHandler();
    mth->timeout_list.push_front(t1);
    mth->timeout_list.push_front(t2);
    mth->parent_timeout = real_timeout;

    multi_timeouts.insert(std::make_pair(t1, mth));
    multi_timeouts.insert(std::make_pair(t2, mth));
    multi_timeouts.insert(std::make_pair(real_timeout, mth));

    t1->multi_timeout = mt1;
    t2->multi_timeout = mt2;
    // set real_timeout->multi_timeout to something, such that we see in add_timeout_intern that
    // it is already a multi_timeout (we never use it, except of checking for 0 ptr)
    real_timeout->multi_timeout = (MultiTimeout*) real_timeout;

    timeout_list.erase(std::remove(timeout_list.begin(), timeout_list.end(), t1),
                       timeout_list.end());
    timeout_list.erase(std::remove(timeout_list.begin(), timeout_list.end(), t2),
                       timeout_list.end());

    UpdateMultiTimeoutValues(mth);
}

void AppendMultiTimeout(Timeout* t1, Timeout* t2) {
    if (t2->multi_timeout) {
        // swap t1 and t2 such that t1 is the multi timeout
        auto tmp = t2;
        t2 = t1;
        t1 = tmp;
    }

    auto mth = multi_timeouts[t1];
    mth->timeout_list.push_front(t2);
    multi_timeouts.insert(std::make_pair(t2, mth));
    t2->multi_timeout = new MultiTimeout();

    UpdateMultiTimeoutValues(mth);
}

bool AlignWithExistingTimeouts(Timeout* t) {
    for (auto const& t2 : timeout_list) {
        if (t2->interval_msec > 0) {
            if (t->interval_msec % t2->interval_msec == 0
                || t2->interval_msec % t->interval_msec == 0) {
                if (!t->multi_timeout && !t2->multi_timeout) {
                    // both timeouts can be aligned, but there is no multi timeout for them
                    CreateMultiTimeout(t, t2);
                } else {
                    // there is already a multi timeout, so we append the new timeout to the multi timeout
                    AppendMultiTimeout(t, t2);
                }

                return true;
            }
        }
    }

    return false;
}

void AddTimeoutInternal(int value_msec, int interval_msec,
                        void(*_callback)(void*), void* arg, Timeout* t) {
    t->interval_msec = interval_msec;
    t->_callback = _callback;
    t->arg = arg;
    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);
    t->timeout_expires = AddMsecToTimespec(cur_time, value_msec);

    bool can_align = false;

    if (interval_msec > 0 && t->multi_timeout == nullptr) {
        can_align = AlignWithExistingTimeouts(t);
    }

    if (!can_align) {
        auto it = std::lower_bound(
                      timeout_list.begin(),
                      timeout_list.end(),
                      t,
        [](Timeout * t1, Timeout * t2) {
            return CompareTimeouts(t1, t2);
        });
        timeout_list.insert(it, t);
    }
}

int TimespecSubtract(struct timespec* result, struct timespec* x,
                     struct timespec* y) {
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_nsec < y->tv_nsec) {
        int nsec = (y->tv_nsec - x->tv_nsec) / 1000000000 + 1;
        y->tv_nsec -= 1000000000 * nsec;
        y->tv_sec += nsec;
    }

    if (x->tv_nsec - y->tv_nsec > 1000000000) {
        int nsec = (x->tv_nsec - y->tv_nsec) / 1000000000;
        y->tv_nsec += 1000000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait. tv_nsec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_nsec = x->tv_nsec - y->tv_nsec;

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

int CalcMultiTimeoutInterval(MultiTimeoutHandler* mth) {
    auto const& front = mth->timeout_list.front();
    int min_interval = front->interval_msec;

    for (auto const& t : mth->timeout_list) {
        if (t->interval_msec < min_interval) {
            min_interval = t->interval_msec;
        }
    }

    return min_interval;
}

void CallbackMultiTimeout(void* arg) {
    auto mth = static_cast<MultiTimeoutHandler*>(arg);

    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    for (auto& t : mth->timeout_list) {
        if (++t->multi_timeout->current_count >=
            t->multi_timeout->count_to_expiration) {
            t->_callback(t->arg);
            t->multi_timeout->current_count = 0;
            t->timeout_expires = AddMsecToTimespec(cur_time, t->interval_msec);
        }
    }
}

void UpdateMultiTimeoutValues(MultiTimeoutHandler* mth) {
    int interval = CalcMultiTimeoutInterval(mth);
    int next_timeout_msec = interval;

    struct timespec cur_time;
    clock_gettime(CLOCK_MONOTONIC, &cur_time);

    for (auto& t : mth->timeout_list) {
        t->multi_timeout->count_to_expiration = t->interval_msec / interval;

        struct timespec diff_time;
        TimespecSubtract(&diff_time, &t->timeout_expires, &cur_time);

        int msec_to_expiration = diff_time.tv_sec * 1000 + diff_time.tv_nsec / 1000000;
        int count_left = msec_to_expiration / interval + (msec_to_expiration % interval
                         != 0);
        t->multi_timeout->current_count = t->multi_timeout->count_to_expiration -
                                          count_left;

        if (msec_to_expiration < next_timeout_msec) {
            next_timeout_msec = msec_to_expiration;
        }
    }

    mth->parent_timeout->interval_msec = interval;
    timeout_list.erase(std::remove(timeout_list.begin(), timeout_list.end(),
                                   mth->parent_timeout), timeout_list.end());
    AddTimeoutInternal(next_timeout_msec, interval, CallbackMultiTimeout, mth,
                       mth->parent_timeout);
}

void RemoveFromMultiTimeout(Timeout* t) {
    auto mth = multi_timeouts[t];

    multi_timeouts.erase(t);
    mth->timeout_list.erase(std::remove(mth->timeout_list.begin(),
                                        mth->timeout_list.end(), t), mth->timeout_list.end());

    delete t->multi_timeout;
    t->multi_timeout = nullptr;

    if (mth->timeout_list.size() == 1) {
        auto last_timeout = mth->timeout_list.front();
        mth->timeout_list.erase(std::remove(mth->timeout_list.begin(),
                                            mth->timeout_list.end(), last_timeout), mth->timeout_list.end());
        delete last_timeout->multi_timeout;
        last_timeout->multi_timeout = nullptr;
        multi_timeouts.erase(last_timeout);
        multi_timeouts.erase(mth->parent_timeout);
        mth->parent_timeout->multi_timeout = nullptr;
        StopTimeout(mth->parent_timeout);
        delete mth;

        struct timespec cur_time, diff_time;
        clock_gettime(CLOCK_MONOTONIC, &cur_time);
        TimespecSubtract(&diff_time, &t->timeout_expires, &cur_time);
        int msec_to_expiration = diff_time.tv_sec * 1000 + diff_time.tv_nsec / 1000000;
        AddTimeoutInternal(msec_to_expiration, last_timeout->interval_msec,
                           last_timeout->_callback, last_timeout->arg, last_timeout);
    } else {
        UpdateMultiTimeoutValues(mth);
    }
}

void StopMultiTimeout(Timeout* t) {
    auto mth = multi_timeouts[t];
    multi_timeouts.erase(mth->parent_timeout);

    for (auto const& t1 : mth->timeout_list) {
        mth->timeout_list.erase(std::remove(mth->timeout_list.begin(),
                                            mth->timeout_list.end(), t1), mth->timeout_list.end());
        multi_timeouts.erase(t1);
        delete t1->multi_timeout;
        delete t1;
    }

    delete mth;
}

} // namespace

void DefaultTimeout() {
    timeout_list.clear();
    multi_timeouts.clear();
}

void CleanupTimeout() {
    for (auto const& t : timeout_list) {
        if (t->multi_timeout) {
            StopMultiTimeout(t);
        }

        delete t;
    }

    timeout_list.clear();
    multi_timeouts.clear();
}

/** Implementation notes for timeouts: The timeouts are kept in a GSList sorted by their
    * expiration time.
    * That means that update_next_timeout() only have to consider the first timeout in the list,
    * and callback_timeout_expired() only have to consider the timeouts as long as the expiration time
    * is in the past to the current time.
    * As time measurement we use clock_gettime(CLOCK_MONOTONIC) because this refers to a timer, which
    * reference point lies somewhere in the past and cannot be changed, but just queried.
    * If a single shot timer is installed it will be automatically deleted. I.e. the returned value
    * of add_timeout will not be valid anymore. You do not need to call stop_timeout for these timeouts,
    * however it's save to call it.
**/

Timeout* AddTimeout(int value_msec, int interval_msec,
                    void (*_callback)(void*), void* arg) {
    auto t = new Timeout();
    t->multi_timeout = 0;
    AddTimeoutInternal(value_msec, interval_msec, _callback, arg, t);
    return t;
}


void ChangeTimeout(Timeout* t, int value_msec, int interval_msec,
                   void (*_callback)(void*), void* arg) {
    auto timeout_it = std::find(timeout_list.begin(),
                                timeout_list.end(),
                                t);
    bool has_timeout = (timeout_it != timeout_list.end());
    bool has_multi_timeout = (multi_timeouts.find(t) != multi_timeouts.end());

    if (!has_timeout && !has_multi_timeout) {
        printf("programming error: timeout already deleted...");
        return;
    }

    if (t->multi_timeout) {
        RemoveFromMultiTimeout(t);
    } else {
        timeout_list.erase(timeout_it);
    }

    AddTimeoutInternal(value_msec, interval_msec, _callback, arg, t);
}


struct timeval* UpdateNextTimeout() {
    if (!timeout_list.empty()) {
        auto t = timeout_list.front();

        struct timespec next_timeout2 = {
            .tv_sec = next_timeout.tv_sec,
            .tv_nsec = next_timeout.tv_usec * 1000
        };

        struct timespec cur_time;
        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        if (TimespecSubtract(&next_timeout2, &t->timeout_expires, &cur_time)) {
            next_timeout.tv_sec = 0;
            next_timeout.tv_usec = 0;
        } else {
            next_timeout.tv_sec = next_timeout2.tv_sec;
            next_timeout.tv_usec = next_timeout2.tv_nsec / 1000;
        }
    } else {
        next_timeout.tv_sec = -1;
    }

    if (next_timeout.tv_sec >= 0 && next_timeout.tv_usec >= 0) {
        return &next_timeout;
    }

    return nullptr;
}


void CallbackTimeoutExpired() {
    auto it = timeout_list.begin();

    while (it != timeout_list.end()) {
        auto t = (*it);

        struct timespec cur_time;
        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        if (CompareTimespecs(t->timeout_expires, cur_time) > 0) {
            return;
        }

        // it's time for the callback function
        t->_callback(t->arg);

        auto pos = std::find(timeout_list.begin(), timeout_list.end(), t);

        // if _callback() calls stop_timeout(t) the timeout 't' was freed and is not in the timeout_list
        // FIXME: make all of this less ugly
        if (pos == timeout_list.end()) {
            ++it;
        } else {
            it = timeout_list.erase(pos);

            if (t->interval_msec > 0) {
                AddTimeoutInternal(t->interval_msec, t->interval_msec, t->_callback, t->arg, t);
            } else {
                delete t;
            }
        }
    }
}


void StopTimeout(Timeout* t) {
    bool has_timeout = (std::find(timeout_list.begin(),
                                  timeout_list.end(),
                                  t) != timeout_list.end());
    bool has_multi_timeout = (multi_timeouts.find(t) != multi_timeouts.end());

    // if not in the list, it was deleted in callback_timeout_expired
    if (has_timeout || has_multi_timeout) {
        if (t->multi_timeout) {
            RemoveFromMultiTimeout(t);
        }

        timeout_list.erase(std::remove(timeout_list.begin(),
                                       timeout_list.end(),
                                       t), timeout_list.end());
        delete t;
    }
}
