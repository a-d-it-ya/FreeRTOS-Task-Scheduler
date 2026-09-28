/**
 * @file rtos_queue.c
 * @brief Thread-Safe Circular FIFO Message Queue Implementation
 */

#include "rtos_queue.h"
#include "rtos_kernel.h"
#include "rtos_port.h"
#include "rtos_trace.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern rtos_tcb_t *g_current_task;

static void queue_wait_insert(rtos_tcb_t **list_head, rtos_tcb_t *task) {
    if (!list_head || !task) return;
    
    task->next = NULL;
    task->prev = NULL;
    
    if (*list_head == NULL) {
        *list_head = task;
    } else {
        rtos_tcb_t *curr = *list_head;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = task;
        task->prev = curr;
    }
}

static void queue_wait_remove(rtos_tcb_t **list_head, rtos_tcb_t *task) {
    if (!list_head || !task) return;
    
    if (task->prev != NULL) {
        task->prev->next = task->next;
    } else if (*list_head == task) {
        *list_head = task->next;
    }
    
    if (task->next != NULL) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
}

rtos_queue_handle_t rtos_queue_create(uint32_t capacity, size_t item_size, const char *name) {
    if (capacity == 0 || item_size == 0) return NULL;
    
    rtos_queue_t *q = (rtos_queue_t*)calloc(1, sizeof(rtos_queue_t));
    if (!q) return NULL;
    
    q->buffer = (uint8_t*)malloc(capacity * item_size);
    if (!q->buffer) {
        free(q);
        return NULL;
    }
    
    q->capacity = capacity;
    q->item_size = item_size;
    q->count = 0;
    q->head = 0;
    q->tail = 0;
    q->send_wait_list = NULL;
    q->recv_wait_list = NULL;
    q->name = name ? name : "Queue";
    
    return q;
}

rtos_status_t rtos_queue_send(rtos_queue_handle_t queue, const void *item_ptr, rtos_tick_t timeout_ticks) {
    if (!queue || !item_ptr) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    
    while (queue->count == queue->capacity) {
        if (timeout_ticks == RTOS_NO_WAIT) {
            rtos_port_exit_critical();
            return RTOS_STATUS_QUEUE_FULL;
        }
        
        rtos_tcb_t *task = g_current_task;
        if (!task) {
            rtos_port_exit_critical();
            return RTOS_STATUS_ERROR;
        }
        
        task->state = RTOS_TASK_STATE_BLOCKED;
        queue_wait_insert(&queue->send_wait_list, task);
        
        if (timeout_ticks != RTOS_WAIT_FOREVER) {
            rtos_kernel_delay_list_insert(task, timeout_ticks);
        }
        
        rtos_kernel_schedule();
        
        if (timeout_ticks != RTOS_WAIT_FOREVER && task->wake_tick <= rtos_kernel_get_ticks() && queue->count == queue->capacity) {
            queue_wait_remove(&queue->send_wait_list, task);
            rtos_port_exit_critical();
            return RTOS_STATUS_TIMEOUT;
        }
    }
    
    /* Copy item to circular buffer */
    memcpy(queue->buffer + (queue->tail * queue->item_size), item_ptr, queue->item_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    
    rtos_trace_record(RTOS_TRACE_EVENT_QUEUE_SEND, g_current_task ? g_current_task->tid : 0,
                      g_current_task ? g_current_task->name : "Kernel", queue->count, queue->capacity,
                      "Sent to '%s' (Count=%u/%u)", queue->name, queue->count, queue->capacity);
    
    /* Wake up waiting receiver if any */
    if (queue->recv_wait_list != NULL) {
        rtos_tcb_t *waking_task = queue->recv_wait_list;
        queue_wait_remove(&queue->recv_wait_list, waking_task);
        
        if (waking_task->state == RTOS_TASK_STATE_BLOCKED) {
            rtos_kernel_delay_list_remove(waking_task);
        }
        
        rtos_kernel_ready_queue_insert(waking_task);
        
        if (g_current_task && waking_task->effective_priority > g_current_task->effective_priority) {
            rtos_kernel_schedule();
        }
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_queue_receive(rtos_queue_handle_t queue, void *item_buffer, rtos_tick_t timeout_ticks) {
    if (!queue || !item_buffer) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    
    while (queue->count == 0) {
        if (timeout_ticks == RTOS_NO_WAIT) {
            rtos_port_exit_critical();
            return RTOS_STATUS_QUEUE_EMPTY;
        }
        
        rtos_tcb_t *task = g_current_task;
        if (!task) {
            rtos_port_exit_critical();
            return RTOS_STATUS_ERROR;
        }
        
        task->state = RTOS_TASK_STATE_BLOCKED;
        queue_wait_insert(&queue->recv_wait_list, task);
        
        if (timeout_ticks != RTOS_WAIT_FOREVER) {
            rtos_kernel_delay_list_insert(task, timeout_ticks);
        }
        
        rtos_kernel_schedule();
        
        if (timeout_ticks != RTOS_WAIT_FOREVER && task->wake_tick <= rtos_kernel_get_ticks() && queue->count == 0) {
            queue_wait_remove(&queue->recv_wait_list, task);
            rtos_port_exit_critical();
            return RTOS_STATUS_TIMEOUT;
        }
    }
    
    /* Copy item from circular buffer */
    memcpy(item_buffer, queue->buffer + (queue->head * queue->item_size), queue->item_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    
    rtos_trace_record(RTOS_TRACE_EVENT_QUEUE_RECV, g_current_task ? g_current_task->tid : 0,
                      g_current_task ? g_current_task->name : "Kernel", queue->count, queue->capacity,
                      "Received from '%s' (Count=%u/%u)", queue->name, queue->count, queue->capacity);
    
    /* Wake up waiting sender if any */
    if (queue->send_wait_list != NULL) {
        rtos_tcb_t *waking_task = queue->send_wait_list;
        queue_wait_remove(&queue->send_wait_list, waking_task);
        
        if (waking_task->state == RTOS_TASK_STATE_BLOCKED) {
            rtos_kernel_delay_list_remove(waking_task);
        }
        
        rtos_kernel_ready_queue_insert(waking_task);
        
        if (g_current_task && waking_task->effective_priority > g_current_task->effective_priority) {
            rtos_kernel_schedule();
        }
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_queue_peek(rtos_queue_handle_t queue, void *item_buffer) {
    if (!queue || !item_buffer) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    if (queue->count == 0) {
        rtos_port_exit_critical();
        return RTOS_STATUS_QUEUE_EMPTY;
    }
    
    memcpy(item_buffer, queue->buffer + (queue->head * queue->item_size), queue->item_size);
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

uint32_t rtos_queue_get_count(rtos_queue_handle_t queue) {
    if (!queue) return 0;
    return queue->count;
}

uint32_t rtos_queue_get_spaces_available(rtos_queue_handle_t queue) {
    if (!queue) return 0;
    return (queue->capacity - queue->count);
}

void rtos_queue_reset(rtos_queue_handle_t queue) {
    if (!queue) return;
    rtos_port_enter_critical();
    queue->count = 0;
    queue->head = 0;
    queue->tail = 0;
    rtos_port_exit_critical();
}

void rtos_queue_delete(rtos_queue_handle_t queue) {
    if (queue) {
        rtos_port_enter_critical();
        if (queue->buffer) {
            free(queue->buffer);
        }
        free(queue);
        rtos_port_exit_critical();
    }
}
