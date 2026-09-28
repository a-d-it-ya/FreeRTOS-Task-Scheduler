/**
 * @file rtos_trace.h
 * @brief Real-Time Event Tracing, CPU Profiling, and Telemetry Inspector
 */

#ifndef RTOS_TRACE_H_
#define RTOS_TRACE_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RTOS_TRACE_EVENT_TASK_CREATE    = 0,
    RTOS_TRACE_EVENT_TASK_SWITCH    = 1,
    RTOS_TRACE_EVENT_TASK_DELAY     = 2,
    RTOS_TRACE_EVENT_TASK_WAKEUP    = 3,
    RTOS_TRACE_EVENT_MUTEX_LOCK     = 4,
    RTOS_TRACE_EVENT_MUTEX_UNLOCK   = 5,
    RTOS_TRACE_EVENT_PIP_ELEVATION  = 6,
    RTOS_TRACE_EVENT_PIP_RESTORE    = 7,
    RTOS_TRACE_EVENT_QUEUE_SEND     = 8,
    RTOS_TRACE_EVENT_QUEUE_RECV     = 9,
    RTOS_TRACE_EVENT_STACK_WARNING  = 10
} rtos_trace_event_type_t;

typedef struct {
    uint64_t                timestamp_us;
    rtos_tick_t             tick;
    rtos_trace_event_type_t event_type;
    rtos_task_id_t          task_id;
    char                    task_name[RTOS_TASK_NAME_MAX_LEN];
    uint32_t                data_1;
    uint32_t                data_2;
    char                    message[48];
} rtos_trace_entry_t;

/**
 * @brief Initialize trace ring buffer and statistics tracking.
 */
void rtos_trace_init(void);

/**
 * @brief Record a kernel event into the high-speed circular trace log.
 */
void rtos_trace_record(rtos_trace_event_type_t event_type,
                       rtos_task_id_t task_id,
                       const char *task_name,
                       uint32_t data_1,
                       uint32_t data_2,
                       const char *fmt, ...);

/**
 * @brief Dump all recorded trace log events to stdout.
 */
void rtos_trace_dump_log(void);

/**
 * @brief Print a rich ASCII runtime dashboard with CPU utilization, stack usage, and task states.
 */
void rtos_trace_print_task_stats(void);

/**
 * @brief Print a single-line summary of context switches and tick elapsed.
 */
void rtos_trace_print_summary(void);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_TRACE_H_ */
