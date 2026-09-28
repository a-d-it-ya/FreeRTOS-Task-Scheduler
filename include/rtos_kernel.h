/**
 * @file rtos_kernel.h
 * @brief Kernel Core Lifecycle, Ready Queue Operations, and Scheduler Engine
 */

#ifndef RTOS_KERNEL_H_
#define RTOS_KERNEL_H_

#include "rtos_types.h"
#include "rtos_task.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Scheduler Running State */
typedef enum {
    RTOS_SCHED_STOPPED = 0,
    RTOS_SCHED_RUNNING = 1,
    RTOS_SCHED_LOCKED  = 2
} rtos_sched_state_t;

/**
 * @brief Initialize all kernel subsystems, ready queues, delay lists, and the idle task.
 */
void rtos_kernel_init(void);

/**
 * @brief Start the RTOS multitasking scheduler. Never returns under normal operation.
 */
void rtos_kernel_start(void);

/**
 * @brief Stop the RTOS multitasking scheduler and return to host environment.
 */
void rtos_kernel_stop(void);

/**
 * @brief Periodic kernel tick handler invoked on each hardware/timer interrupt.
 */
void rtos_kernel_tick_handler(void);

/**
 * @brief Query current kernel tick count.
 * @return Monotonically increasing tick value since scheduler start.
 */
rtos_tick_t rtos_kernel_get_ticks(void);

/**
 * @brief Convert milliseconds to kernel ticks.
 */
rtos_tick_t rtos_ms_to_ticks(uint32_t ms);

/**
 * @brief Convert kernel ticks to milliseconds.
 */
uint32_t rtos_ticks_to_ms(rtos_tick_t ticks);

/**
 * @brief Lock scheduler preemption (allows current task to run without preemption).
 */
void rtos_scheduler_lock(void);

/**
 * @brief Unlock scheduler preemption.
 */
void rtos_scheduler_unlock(void);

/**
 * @brief Check if scheduler is actively running.
 */
bool rtos_scheduler_is_running(void);

/* Kernel Internal Ready Queue / Delay List Helpers (used across kernel modules) */
void rtos_kernel_ready_queue_insert(rtos_tcb_t *task);
void rtos_kernel_ready_queue_remove(rtos_tcb_t *task);
void rtos_kernel_delay_list_insert(rtos_tcb_t *task, rtos_tick_t ticks_to_delay);
void rtos_kernel_delay_list_remove(rtos_tcb_t *task);
void rtos_kernel_schedule(void);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_KERNEL_H_ */
