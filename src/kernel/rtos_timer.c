/**
 * @file rtos_timer.c
 * @brief Software Timers Engine (One-Shot & Auto-Reload)
 */

#include "rtos_timer.h"
#include "rtos_kernel.h"
#include "rtos_port.h"
#include <stdlib.h>
#include <string.h>

static rtos_timer_t *g_active_timers_head = NULL;

rtos_timer_handle_t rtos_timer_create(const char *name,
                                      rtos_tick_t period_ticks,
                                      rtos_timer_mode_t mode,
                                      rtos_timer_callback_t callback,
                                      void *arg) {
    if (!callback || period_ticks == 0) return NULL;
    
    rtos_timer_t *tmr = (rtos_timer_t*)calloc(1, sizeof(rtos_timer_t));
    if (!tmr) return NULL;
    
    tmr->name = name ? name : "Timer";
    tmr->period_ticks = period_ticks;
    tmr->mode = mode;
    tmr->callback = callback;
    tmr->arg = arg;
    tmr->is_active = false;
    tmr->next = NULL;
    
    return tmr;
}

rtos_status_t rtos_timer_start(rtos_timer_handle_t timer) {
    if (!timer) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    
    timer->expiry_tick = rtos_kernel_get_ticks() + timer->period_ticks;
    
    if (!timer->is_active) {
        timer->is_active = true;
        timer->next = g_active_timers_head;
        g_active_timers_head = timer;
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_timer_stop(rtos_timer_handle_t timer) {
    if (!timer) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    if (timer->is_active) {
        timer->is_active = false;
        
        if (g_active_timers_head == timer) {
            g_active_timers_head = timer->next;
        } else {
            rtos_timer_t *curr = g_active_timers_head;
            while (curr && curr->next != timer) {
                curr = curr->next;
            }
            if (curr) {
                curr->next = timer->next;
            }
        }
        timer->next = NULL;
    }
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_timer_change_period(rtos_timer_handle_t timer, rtos_tick_t new_period_ticks) {
    if (!timer || new_period_ticks == 0) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    timer->period_ticks = new_period_ticks;
    timer->expiry_tick = rtos_kernel_get_ticks() + new_period_ticks;
    
    if (!timer->is_active) {
        timer->is_active = true;
        timer->next = g_active_timers_head;
        g_active_timers_head = timer;
    }
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

bool rtos_timer_is_active(rtos_timer_handle_t timer) {
    return timer ? timer->is_active : false;
}

void rtos_timer_delete(rtos_timer_handle_t timer) {
    if (timer) {
        rtos_timer_stop(timer);
        rtos_port_enter_critical();
        free(timer);
        rtos_port_exit_critical();
    }
}

void rtos_timer_tick_check(rtos_tick_t current_tick) {
    rtos_timer_t *curr = g_active_timers_head;
    rtos_timer_t *prev = NULL;
    
    while (curr != NULL) {
        rtos_timer_t *next = curr->next;
        
        if (curr->is_active && current_tick >= curr->expiry_tick) {
            /* Execute callback */
            if (curr->callback) {
                curr->callback(curr->arg);
            }
            
            if (curr->mode == RTOS_TIMER_MODE_AUTO_RELOAD) {
                curr->expiry_tick = current_tick + curr->period_ticks;
                prev = curr;
            } else {
                /* Deactivate one-shot */
                curr->is_active = false;
                if (prev == NULL) {
                    g_active_timers_head = next;
                } else {
                    prev->next = next;
                }
                curr->next = NULL;
            }
        } else {
            prev = curr;
        }
        
        curr = next;
    }
}
