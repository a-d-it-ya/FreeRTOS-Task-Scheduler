/**
 * @file rtos_types.h
 * @brief Core Data Types, Status Codes, and Forward Declarations
 */

#ifndef RTOS_TYPES_H_
#define RTOS_TYPES_H_

#include "rtos_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Status Codes & Return Types                                               */
/* ========================================================================= */

typedef enum {
    RTOS_STATUS_OK                  =  0,
    RTOS_STATUS_ERROR               = -1,
    RTOS_STATUS_TIMEOUT             = -2,
    RTOS_STATUS_RESOURCE_BUSY       = -3,
    RTOS_STATUS_OUT_OF_MEMORY       = -4,
    RTOS_STATUS_INVALID_PARAM       = -5,
    RTOS_STATUS_STACK_OVERFLOW      = -6,
    RTOS_STATUS_QUEUE_FULL          = -7,
    RTOS_STATUS_QUEUE_EMPTY         = -8,
    RTOS_STATUS_TASK_NOT_FOUND      = -9,
    RTOS_STATUS_NOT_INITIALIZED     = -10
} rtos_status_t;

/* ========================================================================= */
/* Task States                                                               */
/* ========================================================================= */

typedef enum {
    RTOS_TASK_STATE_READY       = 0,  /**< In ready queue, eligible to execute */
    RTOS_TASK_STATE_RUNNING     = 1,  /**< Currently executing on CPU */
    RTOS_TASK_STATE_BLOCKED     = 2,  /**< Waiting on sync primitive, queue, or timer delay */
    RTOS_TASK_STATE_SUSPENDED   = 3,  /**< Explicitly suspended */
    RTOS_TASK_STATE_TERMINATED  = 4   /**< Execution finished, awaiting cleanup */
} rtos_task_state_t;

/* ========================================================================= */
/* Primitive Types                                                           */
/* ========================================================================= */

typedef uint32_t rtos_tick_t;
typedef uint32_t rtos_priority_t;
typedef uint32_t rtos_task_id_t;

#define RTOS_WAIT_FOREVER           ((rtos_tick_t)0xFFFFFFFFU)
#define RTOS_NO_WAIT                ((rtos_tick_t)0U)

/** Task Entry Function Signature */
typedef void (*rtos_task_func_t)(void *arg);

/** Software Timer Callback Signature */
typedef void (*rtos_timer_callback_t)(void *arg);

/* Forward declarations */
typedef struct rtos_tcb rtos_tcb_t;
typedef struct rtos_queue rtos_queue_t;
typedef struct rtos_sem rtos_sem_t;
typedef struct rtos_mutex rtos_mutex_t;
typedef struct rtos_timer rtos_timer_t;

/* Task Handle */
typedef rtos_tcb_t* rtos_task_handle_t;
typedef rtos_queue_t* rtos_queue_handle_t;
typedef rtos_sem_t* rtos_sem_handle_t;
typedef rtos_mutex_t* rtos_mutex_handle_t;
typedef rtos_timer_t* rtos_timer_handle_t;

#ifdef __cplusplus
}
#endif

#endif /* RTOS_TYPES_H_ */
