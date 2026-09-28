/**
 * @file rtos_queue.h
 * @brief Inter-Task Message Queue Primitive (Thread-Safe Circular FIFO)
 */

#ifndef RTOS_QUEUE_H_
#define RTOS_QUEUE_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct rtos_queue {
    uint8_t            *buffer;              /**< Storage buffer */
    size_t              item_size;           /**< Size of each item in bytes */
    uint32_t            capacity;            /**< Maximum number of items */
    uint32_t            count;               /**< Current number of stored items */
    uint32_t            head;                /**< Read index */
    uint32_t            tail;                /**< Write index */
    
    rtos_tcb_t         *send_wait_list;      /**< Tasks blocked waiting to send (queue full) */
    rtos_tcb_t         *recv_wait_list;      /**< Tasks blocked waiting to receive (queue empty) */
    const char         *name;
};

/**
 * @brief Create a message queue.
 * @param capacity Maximum number of elements queue can hold.
 * @param item_size Byte size of an individual element.
 * @param name Optional queue identifier name.
 * @return Handle to message queue, or NULL on memory allocation failure.
 */
rtos_queue_handle_t rtos_queue_create(uint32_t capacity, size_t item_size, const char *name);

/**
 * @brief Send an item to the back of the queue.
 * @param queue Queue handle.
 * @param item_ptr Pointer to data item to copy into queue.
 * @param timeout_ticks Ticks to wait if queue is full.
 * @return RTOS_STATUS_OK on success, RTOS_STATUS_QUEUE_FULL / RTOS_STATUS_TIMEOUT on timeout.
 */
rtos_status_t rtos_queue_send(rtos_queue_handle_t queue, const void *item_ptr, rtos_tick_t timeout_ticks);

/**
 * @brief Receive an item from the front of the queue.
 * @param queue Queue handle.
 * @param item_buffer Pointer to buffer receiving the copied item.
 * @param timeout_ticks Ticks to wait if queue is empty.
 * @return RTOS_STATUS_OK on success, RTOS_STATUS_QUEUE_EMPTY / RTOS_STATUS_TIMEOUT on timeout.
 */
rtos_status_t rtos_queue_receive(rtos_queue_handle_t queue, void *item_buffer, rtos_tick_t timeout_ticks);

/**
 * @brief Peek at the front item without removing it.
 * @param queue Queue handle.
 * @param item_buffer Pointer to destination buffer.
 * @return RTOS_STATUS_OK if an item was present, RTOS_STATUS_QUEUE_EMPTY otherwise.
 */
rtos_status_t rtos_queue_peek(rtos_queue_handle_t queue, void *item_buffer);

/**
 * @brief Get current number of items in the queue.
 */
uint32_t rtos_queue_get_count(rtos_queue_handle_t queue);

/**
 * @brief Get available remaining capacity in the queue.
 */
uint32_t rtos_queue_get_spaces_available(rtos_queue_handle_t queue);

/**
 * @brief Reset and empty a queue.
 */
void rtos_queue_reset(rtos_queue_handle_t queue);

/**
 * @brief Delete and free a queue.
 */
void rtos_queue_delete(rtos_queue_handle_t queue);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_QUEUE_H_ */
