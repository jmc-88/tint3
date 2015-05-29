#ifndef TINT3_UTIL_TIMER_H
#define TINT3_UTIL_TIMER_H

#include <functional>

extern struct timeval next_timeout;

struct _multi_timeout;
typedef struct _multi_timeout MultiTimeout;

typedef struct {
  int interval_msec;
  struct timespec timeout_expires;
  std::function<void()> callback;
  MultiTimeout* multi_timeout;
} Timeout;

// timer functions
/**
  * Single shot timer (i.e. timer with interval_msec == 0) are deleted
*automatically as soon as they expire
  * i.e. you do not need to stop them, however it is safe to call stop_timeout
*for these timers.
  * Periodic timeouts are aligned to each other whenever possible, i.e. one
*interval_msec is an
  * integral multiple of the other.
**/

/** default global data **/
void DefaultTimeout();

/** freed memory : stops all timeouts **/
void CleanupTimeout();

/** installs a timeout with the first timeout of 'value_msec' and then a
*periodic timeout with
  * 'interval_msec'. 'callback' is the callback function when the timer reaches
*the timeout.
  * returns a pointer to the timeout, which is needed for stopping it again
**/
Timeout* AddTimeout(int value_msec, int interval_msec,
                    std::function<void()> callback);

/** changes timeout 't'. If timeout 't' does not exist, nothing happens **/
void ChangeTimeout(Timeout* t, int value_msec, int interval_msec,
                   std::function<void()> callback);

/** stops the timeout 't' **/
void StopTimeout(Timeout* t);

/** update_next_timeout updates next_timeout to the value, when the next
 * installed timeout will expire **/
struct timeval* UpdateNextTimeout();

/** Callback of all expired timeouts **/
void CallbackTimeoutExpired();

#endif  // TINT3_UTIL_TIMER_H
