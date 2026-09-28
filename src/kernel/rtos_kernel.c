/**
 * @file rtos_kernel.c
 * @brief Multi-Priority Ready Queue, Scheduler Dispatcher, and Kernel Engine
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_port.h"
#include "rtos_trace.h"
#include "rtos_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Ready queue: Circular doubly linked list per priority level */
typedef struct {
    rtos_tcb_t *head;
    rtos_tcb_t *tail;
    uint32_t    count;
} rtos_ready_list_t;

static rtos_ready_list_t g_ready_queues[RTOS_MAX_PRIORITIES];
static uint32_t          g_ready_priority_bitmap = 0;

/* Delay list (sorted ascending by wake_tick) */
static rtos_tcb_t       *g_delay_list_head = NULL;

/* Scheduler state variables */
static volatile rtos_sched_state_t g_sched_state = RTOS_SCHED_STOPPED;
static volatile rtos_tick_t        g_kernel_tick_count = 0;
static uint32_t                    g_scheduler_lock_nesting = 0;

rtos_tcb_t                        *g_current_task = NULL;
static rtos_task_handle_t          g_idle_task_handle = NULL;
static void                       *g_kernel_main_context = NULL;

/* Idle task function */
static void rtos_idle_task_entry(void *arg) {
    (void)arg;
    while (g_sched_state == RTOS_SCHED_RUNNING) {
        rtos_port_idle_sleep(1);
        rtos_task_yield();
    }
}

static uint32_t get_highest_priority(void) {
    if (g_ready_priority_bitmap == 0) return 0;
    
#if defined(__GNUC__) || defined(__clang__)
    return 31U - (uint32_t)__builtin_clz(g_ready_priority_bitmap);
#else
    /* Fast bit-scan reverse fallback */
    uint32_t p = 31;
    while (p > 0 && !(g_ready_priority_bitmap & (1U << p))) {
        p--;
    }
    return p;
#endif
}

void rtos_kernel_ready_queue_insert(rtos_tcb_t *task) {
    if (!task) return;
    
    rtos_priority_t prio = task->effective_priority;
    if (prio >= RTOS_MAX_PRIORITIES) prio = RTOS_MAX_PRIORITIES - 1;
    
    rtos_ready_list_t *q = &g_ready_queues[prio];
    task->next = NULL;
    task->prev = q->tail;
    
    if (q->tail != NULL) {
        q->tail->next = task;
    } else {
        q->head = task;
    }
    q->tail = task;
    q->count++;
    
    g_ready_priority_bitmap |= (1U << prio);
    task->state = RTOS_TASK_STATE_READY;
}

void rtos_kernel_ready_queue_remove(rtos_tcb_t *task) {
    if (!task) return;
    
    rtos_priority_t prio = task->effective_priority;
    if (prio >= RTOS_MAX_PRIORITIES) prio = RTOS_MAX_PRIORITIES - 1;
    
    rtos_ready_list_t *q = &g_ready_queues[prio];
    
    if (task->prev != NULL) {
        task->prev->next = task->next;
    } else {
        q->head = task->next;
    }
    
    if (task->next != NULL) {
        task->next->prev = task->prev;
    } else {
        q->tail = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
    if (q->count > 0) q->count--;
    
    if (q->count == 0) {
        g_ready_priority_bitmap &= ~(1U << prio);
    }
}

void rtos_kernel_delay_list_insert(rtos_tcb_t *task, rtos_tick_t ticks_to_delay) {
    if (!task) return;
    
    task->wake_tick = g_kernel_tick_count + ticks_to_delay;
    task->state = RTOS_TASK_STATE_BLOCKED;
    task->next = NULL;
    task->prev = NULL;
    
    /* Insert into sorted linked list */
    if (g_delay_list_head == NULL || task->wake_tick < g_delay_list_head->wake_tick) {
        task->next = g_delay_list_head;
        if (g_delay_list_head) {
            g_delay_list_head->prev = task;
        }
        g_delay_list_head = task;
    } else {
        rtos_tcb_t *curr = g_delay_list_head;
        while (curr->next != NULL && curr->next->wake_tick <= task->wake_tick) {
            curr = curr->next;
        }
        task->next = curr->next;
        if (curr->next != NULL) {
            curr->next->prev = task;
        }
        curr->next = task;
        task->prev = curr;
    }
}

void rtos_kernel_delay_list_remove(rtos_tcb_t *task) {
    if (!task) return;
    
    if (task->prev != NULL) {
        task->prev->next = task->next;
    } else if (g_delay_list_head == task) {
        g_delay_list_head = task->next;
    }
    
    if (task->next != NULL) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
}

static void rtos_check_delayed_tasks(void) {
    while (g_delay_list_head != NULL && g_delay_list_head->wake_tick <= g_kernel_tick_count) {
        rtos_tcb_t *waking_task = g_delay_list_head;
        g_delay_list_head = waking_task->next;
        if (g_delay_list_head) {
            g_delay_list_head->prev = NULL;
        }
        waking_task->next = NULL;
        waking_task->prev = NULL;
        
        rtos_trace_record(RTOS_TRACE_EVENT_TASK_WAKEUP, waking_task->tid, waking_task->name,
                          g_kernel_tick_count, waking_task->effective_priority, "Woke up from delay");
        
        rtos_kernel_ready_queue_insert(waking_task);
    }
}

void rtos_kernel_schedule(void) {
    if (g_sched_state != RTOS_SCHED_RUNNING || g_scheduler_lock_nesting > 0) {
        return;
    }
    
    uint32_t top_prio = get_highest_priority();
    rtos_ready_list_t *top_queue = &g_ready_queues[top_prio];
    rtos_tcb_t *next_task = top_queue->head;
    
    if (next_task == NULL) {
        return;
    }
    
    rtos_tcb_t *prev_task = g_current_task;
    
    /* If same task, continue running */
    if (prev_task == next_task && prev_task->state == RTOS_TASK_STATE_RUNNING) {
        return;
    }
    
    uint64_t now_us = rtos_port_get_time_us();
    if (prev_task != NULL && prev_task->last_switched_in_us > 0) {
        prev_task->total_runtime_us += (now_us - prev_task->last_switched_in_us);
    }
    
    /* Pop next_task from ready queue */
    rtos_kernel_ready_queue_remove(next_task);
    next_task->state = RTOS_TASK_STATE_RUNNING;
    next_task->last_switched_in_us = now_us;
    next_task->context_switch_count++;
    
    g_current_task = next_task;
    
    rtos_trace_record(RTOS_TRACE_EVENT_TASK_SWITCH, next_task->tid, next_task->name,
                      prev_task ? prev_task->tid : 0, next_task->effective_priority,
                      "Switched from %s (Prio %u) to %s (Prio %u)",
                      prev_task ? prev_task->name : "None", prev_task ? prev_task->effective_priority : 0,
                      next_task->name, next_task->effective_priority);
    
    rtos_port_context_switch(next_task->context);
}

void rtos_kernel_init(void) {
    rtos_port_init();
    rtos_trace_init();
    
    memset(g_ready_queues, 0, sizeof(g_ready_queues));
    g_ready_priority_bitmap = 0;
    g_delay_list_head = NULL;
    g_kernel_tick_count = 0;
    g_sched_state = RTOS_SCHED_STOPPED;
    g_scheduler_lock_nesting = 0;
    g_current_task = NULL;
    
    /* Create Idle Task at Priority 0 */
    rtos_task_config_t idle_cfg = {
        .name = "IDLE_TASK",
        .entry = rtos_idle_task_entry,
        .arg = NULL,
        .priority = 0,
        .stack_size = RTOS_IDLE_STACK_SIZE
    };
    rtos_task_create(&idle_cfg, &g_idle_task_handle);
}

void rtos_kernel_start(void) {
    g_sched_state = RTOS_SCHED_RUNNING;
    
#if RTOS_CONFIG_PREEMPTION
    rtos_port_start_tick_timer(rtos_kernel_tick_handler);
#endif
    
    /* Schedule first highest-priority task */
    rtos_kernel_schedule();
}

void rtos_kernel_stop(void) {
    g_sched_state = RTOS_SCHED_STOPPED;
    rtos_port_stop_tick_timer();
}

void rtos_kernel_tick_handler(void) {
    if (g_sched_state != RTOS_SCHED_RUNNING) return;
    
    g_kernel_tick_count++;
    
    /* Check delayed sleeping tasks */
    rtos_check_delayed_tasks();
    
    /* Check software timers */
    rtos_timer_tick_check(g_kernel_tick_count);
    
    /* Time-slicing / Preemption check */
    if (g_current_task != NULL) {
#if RTOS_CONFIG_TIME_SLICING
        if (g_current_task->time_slice_remaining > 0) {
            g_current_task->time_slice_remaining--;
        }
#endif
        uint32_t top_prio = get_highest_priority();
        
        /* Preempt if higher priority task is ready, or time slice expired for equal priority */
        if (top_prio > g_current_task->effective_priority ||
            (top_prio == g_current_task->effective_priority &&
             g_ready_queues[top_prio].count > 0 &&
             g_current_task->time_slice_remaining == 0)) {
            
            g_current_task->state = RTOS_TASK_STATE_READY;
            g_current_task->time_slice_remaining = RTOS_TIME_SLICE_TICKS;
            rtos_kernel_ready_queue_insert(g_current_task);
            rtos_kernel_schedule();
        }
    }
}

rtos_tick_t rtos_kernel_get_ticks(void) {
    return g_kernel_tick_count;
}

rtos_tick_t rtos_ms_to_ticks(uint32_t ms) {
    return (rtos_tick_t)((ms * RTOS_TICK_RATE_HZ) / 1000U);
}

uint32_t rtos_ticks_to_ms(rtos_tick_t ticks) {
    return (uint32_t)((ticks * 1000U) / RTOS_TICK_RATE_HZ);
}

void rtos_scheduler_lock(void) {
    rtos_port_enter_critical();
    g_scheduler_lock_nesting++;
    rtos_port_exit_critical();
}

void rtos_scheduler_unlock(void) {
    rtos_port_enter_critical();
    if (g_scheduler_lock_nesting > 0) {
        g_scheduler_lock_nesting--;
        if (g_scheduler_lock_nesting == 0 && rtos_scheduler_is_running()) {
            rtos_kernel_schedule();
        }
    }
    rtos_port_exit_critical();
}

bool rtos_scheduler_is_running(void) {
    return (g_sched_state == RTOS_SCHED_RUNNING);
}
