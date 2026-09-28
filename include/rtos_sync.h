/**
 * @file rtos_sync.h
 * @brief Synchronization Primitives: Mutex with Priority Inheritance & Semaphores
 */

#ifndef RTOS_SYNC_H_
#define RTOS_SYNC_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Semaphore Interface                                                       */
/* ========================================================================= */

struct rtos_sem {
    uint32_t            count;
    uint32_t            max_count;
    rtos_tcb_t         *wait_list_head;      /**< Blocked tasks waiting on semaphore */
    const char         *name;
};

/**
 * @brief Create a counting or binary semaphore.
 * @param initial_count Initial token count.
 * @param max_count Maximum allowable tokens (1 for binary semaphore).
 * @param name Optional identifier name.
 * @return Handle to newly created semaphore, or NULL on failure.
 */
rtos_sem_handle_t rtos_sem_create(uint32_t initial_count, uint32_t max_count, const char *name);

/**
 * @brief Create a binary semaphore (0 or 1).
 */
rtos_sem_handle_t rtos_sem_create_binary(uint32_t initial_count, const char *name);

/**
 * @brief Wait / Take a semaphore with timeout.
 * @param sem Semaphore handle.
 * @param timeout_ticks Ticks to wait (RTOS_WAIT_FOREVER or RTOS_NO_WAIT).
 * @return RTOS_STATUS_OK if acquired, RTOS_STATUS_TIMEOUT if elapsed.
 */
rtos_status_t rtos_sem_take(rtos_sem_handle_t sem, rtos_tick_t timeout_ticks);

/**
 * @brief Give / Release a semaphore token.
 * @param sem Semaphore handle.
 * @return RTOS_STATUS_OK on success.
 */
rtos_status_t rtos_sem_give(rtos_sem_handle_t sem);

/**
 * @brief Delete and free a semaphore.
 */
void rtos_sem_delete(rtos_sem_handle_t sem);

/* ========================================================================= */
/* Mutex Interface (with Priority Inheritance)                              */
/* ========================================================================= */

struct rtos_mutex {
    rtos_tcb_t         *owner;               /**< Task currently holding the mutex */
    uint32_t            lock_count;          /**< Recursion lock count */
    rtos_priority_t     original_owner_prio; /**< Base priority of owner prior to elevation */
    rtos_tcb_t         *wait_list_head;      /**< High-to-low priority ordered wait list */
    struct rtos_mutex  *next_held;           /**< Next mutex held by same task */
    const char         *name;
};

/**
 * @brief Create a recursive mutex supporting Priority Inheritance Protocol (PIP).
 * @param name Optional identifier name.
 * @return Handle to mutex, or NULL on failure.
 */
rtos_mutex_handle_t rtos_mutex_create(const char *name);

/**
 * @brief Lock / Acquire a mutex with timeout and automatic priority inheritance.
 * @param mutex Mutex handle.
 * @param timeout_ticks Maximum ticks to wait.
 * @return RTOS_STATUS_OK if locked, RTOS_STATUS_TIMEOUT if timed out.
 */
rtos_status_t rtos_mutex_lock(rtos_mutex_handle_t mutex, rtos_tick_t timeout_ticks);

/**
 * @brief Unlock / Release a mutex and restore original priority if no other mutexes held.
 * @param mutex Mutex handle.
 * @return RTOS_STATUS_OK on success.
 */
rtos_status_t rtos_mutex_unlock(rtos_mutex_handle_t mutex);

/**
 * @brief Delete and free a mutex.
 */
void rtos_mutex_delete(rtos_mutex_handle_t mutex);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_SYNC_H_ */
