/**
 * @file rtos_timer.h
 * @brief Software Timers Subsystem (One-Shot and Periodic Auto-Reload)
 */

#ifndef RTOS_TIMER_H_
#define RTOS_TIMER_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RTOS_TIMER_MODE_ONE_SHOT    = 0,
    RTOS_TIMER_MODE_AUTO_RELOAD = 1
} rtos_timer_mode_t;

struct rtos_timer {
    const char             *name;
    rtos_timer_callback_t   callback;
    void                   *arg;
    rtos_tick_t             period_ticks;
    rtos_tick_t             expiry_tick;
    rtos_timer_mode_t       mode;
    bool                    is_active;
    struct rtos_timer      *next;
};

/**
 * @brief Create a software timer.
 * @param name Friendly timer name.
 * @param period_ticks Period duration in kernel ticks.
 * @param mode ONE_SHOT or AUTO_RELOAD.
 * @param callback Function invoked when timer expires.
 * @param arg Argument passed to callback.
 * @return Handle to newly created timer.
 */
rtos_timer_handle_t rtos_timer_create(const char *name,
                                      rtos_tick_t period_ticks,
                                      rtos_timer_mode_t mode,
                                      rtos_timer_callback_t callback,
                                      void *arg);

/**
 * @brief Start or restart an existing timer.
 */
rtos_status_t rtos_timer_start(rtos_timer_handle_t timer);

/**
 * @brief Stop an active timer.
 */
rtos_status_t rtos_timer_stop(rtos_timer_handle_t timer);

/**
 * @brief Change a timer's period and restart it.
 */
rtos_status_t rtos_timer_change_period(rtos_timer_handle_t timer, rtos_tick_t new_period_ticks);

/**
 * @brief Check if a timer is currently running.
 */
bool rtos_timer_is_active(rtos_timer_handle_t timer);

/**
 * @brief Delete and free a software timer.
 */
void rtos_timer_delete(rtos_timer_handle_t timer);

/**
 * @brief Internal tick check function called by kernel tick handler.
 */
void rtos_timer_tick_check(rtos_tick_t current_tick);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_TIMER_H_ */
