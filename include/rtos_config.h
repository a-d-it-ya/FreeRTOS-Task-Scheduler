/**
 * @file rtos_config.h
 * @brief Kernel Configuration & Compile-Time Tuning Parameters
 *
 * Configures scheduling characteristics, priority levels, stack limits,
 * and enabled kernel subsystems.
 */

#ifndef RTOS_CONFIG_H_
#define RTOS_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Scheduler Configuration                                                   */
/* ========================================================================= */

/** Maximum number of priority levels (0 = lowest/idle, RTOS_MAX_PRIORITIES-1 = highest) */
#define RTOS_MAX_PRIORITIES             (32U)

/** Maximum number of concurrent tasks allowed in the system */
#define RTOS_MAX_TASKS                  (32U)

/** Default task stack size in bytes */
#define RTOS_DEFAULT_STACK_SIZE         (64U * 1024U)

/** Minimum allowable task stack size in bytes */
#define RTOS_MIN_STACK_SIZE             (16U * 1024U)

/** Idle task stack size in bytes */
#define RTOS_IDLE_STACK_SIZE            (32U * 1024U)

/** Stack canary word used for stack overflow detection */
#define RTOS_STACK_CANARY_WORD          (0xDEADBEEFU)

/** Kernel tick frequency in Hertz (e.g. 1000 Hz = 1ms tick period) */
#define RTOS_TICK_RATE_HZ               (1000U)

/** Maximum length of a task's human-readable name */
#define RTOS_TASK_NAME_MAX_LEN          (32U)

/** Enable or disable preemptive scheduling (1 = Preemptive, 0 = Pure Cooperative) */
#define RTOS_CONFIG_PREEMPTION          (1)

/** Enable time slicing among equal-priority tasks (Round-Robin within priority level) */
#define RTOS_CONFIG_TIME_SLICING        (1)

/** Time slice quantum in kernel ticks for equal-priority tasks */
#define RTOS_TIME_SLICE_TICKS           (5U)

/* ========================================================================= */
/* Synchronization & IPC Configuration                                       */
/* ========================================================================= */

/** Enable Priority Inheritance Protocol on Mutexes */
#define RTOS_CONFIG_PRIORITY_INHERITANCE (1)

/** Maximum number of software timers */
#define RTOS_MAX_TIMERS                 (16U)

/** Maximum number of message queues */
#define RTOS_MAX_QUEUES                 (16U)

/** Maximum number of semaphores / mutexes */
#define RTOS_MAX_SEMAPHORES             (32U)

/* ========================================================================= */
/* Trace & Diagnostics Configuration                                         */
/* ========================================================================= */

/** Enable kernel event tracing and CPU runtime statistics */
#define RTOS_CONFIG_ENABLE_TRACE        (1)

/** Maximum trace log ring buffer entries */
#define RTOS_TRACE_BUFFER_SIZE          (256U)

/** Enable ANSI color output for terminal observability */
#define RTOS_CONFIG_TERMINAL_COLORS     (1)

#ifdef __cplusplus
}
#endif

#endif /* RTOS_CONFIG_H_ */
