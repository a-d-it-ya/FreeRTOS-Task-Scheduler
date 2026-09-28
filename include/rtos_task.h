/**
 * @file rtos_task.h
 * @brief Task Management, TCB Structure, and Scheduling Control
 */

#ifndef RTOS_TASK_H_
#define RTOS_TASK_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Task Control Block (TCB) */
struct rtos_tcb {
    void               *context;             /**< Platform context / Fiber / Stack ptr */
    void               *stack_base;          /**< Allocated stack buffer pointer */
    size_t              stack_size;          /**< Size of stack in bytes */
    
    rtos_task_id_t      tid;                 /**< Unique Task ID */
    char                name[RTOS_TASK_NAME_MAX_LEN]; /**< Human-readable name */
    
    rtos_priority_t     base_priority;       /**< Assigned static priority */
    rtos_priority_t     effective_priority;  /**< Dynamic priority (PIP elevated) */
    
    rtos_task_state_t   state;               /**< READY, RUNNING, BLOCKED, etc. */
    
    rtos_tick_t         wake_tick;           /**< Tick at which a sleeping/blocked task awakens */
    rtos_tick_t         time_slice_remaining;/**< Ticks remaining in current quantum */
    
    /* CPU & Performance Metrics */
    uint64_t            total_runtime_us;    /**< Total accumulated CPU runtime in microseconds */
    uint64_t            last_switched_in_us; /**< Timestamp when task was switched in */
    uint32_t            context_switch_count;/**< Number of times scheduled */
    size_t              stack_high_watermark;/**< Minimum unused stack observed (bytes) */
    
    /* Linked list pointers for queues (ready list, delay list, wait list) */
    struct rtos_tcb    *next;
    struct rtos_tcb    *prev;
    
    /* Mutex holding list for Priority Inheritance Protocol */
    struct rtos_mutex  *held_mutexes;
};

/** Task Creation Configuration Parameter Struct */
typedef struct {
    const char         *name;
    rtos_task_func_t    entry;
    void               *arg;
    rtos_priority_t     priority;
    size_t              stack_size;
} rtos_task_config_t;

/**
 * @brief Create and register a new task with the scheduler.
 * @param config Task creation parameter structure.
 * @param[out] out_handle Returned task handle.
 * @return RTOS_STATUS_OK on success, error code otherwise.
 */
rtos_status_t rtos_task_create(const rtos_task_config_t *config, rtos_task_handle_t *out_handle);

/**
 * @brief Voluntarily yield the CPU to another task of equal or higher priority.
 */
void rtos_task_yield(void);

/**
 * @brief Delay the current task for a specified duration in milliseconds.
 * @param ms Duration to sleep in milliseconds.
 */
void rtos_task_delay_ms(uint32_t ms);

/**
 * @brief Delay the current task for a specified number of kernel ticks.
 * @param ticks Number of ticks to delay.
 */
void rtos_task_delay_ticks(rtos_tick_t ticks);

/**
 * @brief Delay until an absolute tick count is reached (deterministic periodic task execution).
 * @param previous_wake_tick Pointer to variable storing previous wake tick; updated automatically.
 * @param increment_ticks Periodicity in ticks.
 */
void rtos_task_delay_until(rtos_tick_t *previous_wake_tick, rtos_tick_t increment_ticks);

/**
 * @brief Suspend a specific task.
 * @param task Handle to task (or NULL for current task).
 * @return RTOS_STATUS_OK on success.
 */
rtos_status_t rtos_task_suspend(rtos_task_handle_t task);

/**
 * @brief Resume a suspended task.
 * @param task Handle to suspended task.
 * @return RTOS_STATUS_OK on success.
 */
rtos_status_t rtos_task_resume(rtos_task_handle_t task);

/**
 * @brief Change a task's base priority dynamically.
 * @param task Task handle (or NULL for current task).
 * @param new_priority New priority level.
 * @return RTOS_STATUS_OK on success.
 */
rtos_status_t rtos_task_set_priority(rtos_task_handle_t task, rtos_priority_t new_priority);

/**
 * @brief Get the current running task's handle.
 */
rtos_task_handle_t rtos_task_get_current(void);

/**
 * @brief Check stack health and calculate stack high-watermark for a task.
 * @param task Task handle.
 * @return Remaining unused stack bytes, or 0 if corrupted.
 */
size_t rtos_task_get_stack_high_watermark(rtos_task_handle_t task);

/**
 * @brief Terminate and delete the current task or a specified task.
 */
void rtos_task_delete(rtos_task_handle_t task);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_TASK_H_ */
