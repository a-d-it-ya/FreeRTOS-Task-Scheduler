/**
 * @file rtos_sync.c
 * @brief Synchronization Primitives: Mutex with Priority Inheritance & Semaphores
 */

#include "rtos_sync.h"
#include "rtos_kernel.h"
#include "rtos_port.h"
#include "rtos_trace.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern rtos_tcb_t *g_current_task;

/* Helper to insert task into priority-ordered wait list */
static void wait_list_insert_prio(rtos_tcb_t **head, rtos_tcb_t *task) {
    if (!head || !task) return;
    
    task->next = NULL;
    task->prev = NULL;
    
    if (*head == NULL || task->effective_priority > (*head)->effective_priority) {
        task->next = *head;
        if (*head) (*head)->prev = task;
        *head = task;
    } else {
        rtos_tcb_t *curr = *head;
        while (curr->next != NULL && curr->next->effective_priority >= task->effective_priority) {
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

static void wait_list_remove(rtos_tcb_t **head, rtos_tcb_t *task) {
    if (!head || !task) return;
    
    if (task->prev != NULL) {
        task->prev->next = task->next;
    } else if (*head == task) {
        *head = task->next;
    }
    
    if (task->next != NULL) {
        task->next->prev = task->prev;
    }
    
    task->next = NULL;
    task->prev = NULL;
}

/* ========================================================================= */
/* Semaphore Implementation                                                  */
/* ========================================================================= */

rtos_sem_handle_t rtos_sem_create(uint32_t initial_count, uint32_t max_count, const char *name) {
    if (max_count == 0 || initial_count > max_count) return NULL;
    
    rtos_sem_t *sem = (rtos_sem_t*)calloc(1, sizeof(rtos_sem_t));
    if (!sem) return NULL;
    
    sem->count = initial_count;
    sem->max_count = max_count;
    sem->wait_list_head = NULL;
    sem->name = name ? name : "Sem";
    
    return sem;
}

rtos_sem_handle_t rtos_sem_create_binary(uint32_t initial_count, const char *name) {
    return rtos_sem_create(initial_count > 1 ? 1 : initial_count, 1, name);
}

rtos_status_t rtos_sem_take(rtos_sem_handle_t sem, rtos_tick_t timeout_ticks) {
    if (!sem) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    
    if (sem->count > 0) {
        sem->count--;
        rtos_port_exit_critical();
        return RTOS_STATUS_OK;
    }
    
    if (timeout_ticks == RTOS_NO_WAIT) {
        rtos_port_exit_critical();
        return RTOS_STATUS_TIMEOUT;
    }
    
    rtos_tcb_t *task = g_current_task;
    if (!task) {
        rtos_port_exit_critical();
        return RTOS_STATUS_ERROR;
    }
    
    task->state = RTOS_TASK_STATE_BLOCKED;
    wait_list_insert_prio(&sem->wait_list_head, task);
    
    if (timeout_ticks != RTOS_WAIT_FOREVER) {
        rtos_kernel_delay_list_insert(task, timeout_ticks);
    }
    
    rtos_kernel_schedule();
    
    /* When task resumes execution */
    if (timeout_ticks != RTOS_WAIT_FOREVER && task->wake_tick <= rtos_kernel_get_ticks() && sem->count == 0) {
        wait_list_remove(&sem->wait_list_head, task);
        rtos_port_exit_critical();
        return RTOS_STATUS_TIMEOUT;
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_sem_give(rtos_sem_handle_t sem) {
    if (!sem) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    
    if (sem->wait_list_head != NULL) {
        rtos_tcb_t *waking_task = sem->wait_list_head;
        wait_list_remove(&sem->wait_list_head, waking_task);
        
        if (waking_task->state == RTOS_TASK_STATE_BLOCKED) {
            rtos_kernel_delay_list_remove(waking_task);
        }
        
        rtos_kernel_ready_queue_insert(waking_task);
        
        if (g_current_task && waking_task->effective_priority > g_current_task->effective_priority) {
            rtos_kernel_schedule();
        }
    } else {
        if (sem->count < sem->max_count) {
            sem->count++;
        }
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

void rtos_sem_delete(rtos_sem_handle_t sem) {
    if (sem) {
        rtos_port_enter_critical();
        free(sem);
        rtos_port_exit_critical();
    }
}

/* ========================================================================= */
/* Mutex Implementation with Priority Inheritance Protocol (PIP)           */
/* ========================================================================= */

rtos_mutex_handle_t rtos_mutex_create(const char *name) {
    rtos_mutex_t *mutex = (rtos_mutex_t*)calloc(1, sizeof(rtos_mutex_t));
    if (!mutex) return NULL;
    
    mutex->owner = NULL;
    mutex->lock_count = 0;
    mutex->original_owner_prio = 0;
    mutex->wait_list_head = NULL;
    mutex->next_held = NULL;
    mutex->name = name ? name : "Mutex";
    
    return mutex;
}

static void recalculate_owner_priority(rtos_tcb_t *owner) {
    if (!owner) return;
    
    rtos_priority_t max_prio = owner->base_priority;
    
    /* Iterate over all mutexes held by this owner */
    struct rtos_mutex *m = owner->held_mutexes;
    while (m != NULL) {
        if (m->wait_list_head != NULL) {
            if (m->wait_list_head->effective_priority > max_prio) {
                max_prio = m->wait_list_head->effective_priority;
            }
        }
        m = m->next_held;
    }
    
    if (owner->effective_priority != max_prio) {
        rtos_priority_t old_p = owner->effective_priority;
        owner->effective_priority = max_prio;
        
        if (old_p < max_prio) {
            rtos_trace_record(RTOS_TRACE_EVENT_PIP_ELEVATION, owner->tid, owner->name,
                              old_p, max_prio, "PIP Elevate %s: %u -> %u", owner->name, old_p, max_prio);
        } else {
            rtos_trace_record(RTOS_TRACE_EVENT_PIP_RESTORE, owner->tid, owner->name,
                              old_p, max_prio, "PIP Restore %s: %u -> %u", owner->name, old_p, max_prio);
        }
        
        if (owner->state == RTOS_TASK_STATE_READY) {
            rtos_kernel_ready_queue_remove(owner);
            rtos_kernel_ready_queue_insert(owner);
        }
    }
}

rtos_status_t rtos_mutex_lock(rtos_mutex_handle_t mutex, rtos_tick_t timeout_ticks) {
    if (!mutex) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    rtos_tcb_t *task = g_current_task;
    if (!task) {
        rtos_port_exit_critical();
        return RTOS_STATUS_ERROR;
    }
    
    /* Recursive acquisition */
    if (mutex->owner == task) {
        mutex->lock_count++;
        rtos_port_exit_critical();
        return RTOS_STATUS_OK;
    }
    
    /* Free mutex */
    if (mutex->owner == NULL) {
        mutex->owner = task;
        mutex->lock_count = 1;
        mutex->original_owner_prio = task->base_priority;
        
        /* Add to task's held_mutexes list */
        mutex->next_held = task->held_mutexes;
        task->held_mutexes = mutex;
        
        rtos_trace_record(RTOS_TRACE_EVENT_MUTEX_LOCK, task->tid, task->name,
                          (uint32_t)(uintptr_t)mutex, 1, "Locked mutex '%s'", mutex->name);
        
        rtos_port_exit_critical();
        return RTOS_STATUS_OK;
    }
    
    if (timeout_ticks == RTOS_NO_WAIT) {
        rtos_port_exit_critical();
        return RTOS_STATUS_RESOURCE_BUSY;
    }
    
    /* Block task and apply Priority Inheritance */
    task->state = RTOS_TASK_STATE_BLOCKED;
    wait_list_insert_prio(&mutex->wait_list_head, task);
    
#if RTOS_CONFIG_PRIORITY_INHERITANCE
    if (mutex->owner->effective_priority < task->effective_priority) {
        recalculate_owner_priority(mutex->owner);
    }
#endif
    
    if (timeout_ticks != RTOS_WAIT_FOREVER) {
        rtos_kernel_delay_list_insert(task, timeout_ticks);
    }
    
    rtos_kernel_schedule();
    
    /* Resumed */
    if (timeout_ticks != RTOS_WAIT_FOREVER && task->wake_tick <= rtos_kernel_get_ticks() && mutex->owner != task) {
        wait_list_remove(&mutex->wait_list_head, task);
#if RTOS_CONFIG_PRIORITY_INHERITANCE
        recalculate_owner_priority(mutex->owner);
#endif
        rtos_port_exit_critical();
        return RTOS_STATUS_TIMEOUT;
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_mutex_unlock(rtos_mutex_handle_t mutex) {
    if (!mutex) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    rtos_tcb_t *task = g_current_task;
    
    if (mutex->owner != task) {
        rtos_port_exit_critical();
        return RTOS_STATUS_ERROR; /* Calling task does not own mutex */
    }
    
    mutex->lock_count--;
    if (mutex->lock_count > 0) {
        rtos_port_exit_critical();
        return RTOS_STATUS_OK; /* Still locked recursively */
    }
    
    /* Remove from held mutex list */
    if (task->held_mutexes == mutex) {
        task->held_mutexes = mutex->next_held;
    } else {
        struct rtos_mutex *curr = task->held_mutexes;
        while (curr != NULL && curr->next_held != mutex) {
            curr = curr->next_held;
        }
        if (curr != NULL) {
            curr->next_held = mutex->next_held;
        }
    }
    mutex->next_held = NULL;
    
    rtos_trace_record(RTOS_TRACE_EVENT_MUTEX_UNLOCK, task->tid, task->name,
                      (uint32_t)(uintptr_t)mutex, 0, "Unlocked mutex '%s'", mutex->name);
    
    /* Restore/recalculate owner priority */
#if RTOS_CONFIG_PRIORITY_INHERITANCE
    recalculate_owner_priority(task);
#endif

    /* Hand off ownership to highest waiting task if any */
    if (mutex->wait_list_head != NULL) {
        rtos_tcb_t *new_owner = mutex->wait_list_head;
        wait_list_remove(&mutex->wait_list_head, new_owner);
        
        if (new_owner->state == RTOS_TASK_STATE_BLOCKED) {
            rtos_kernel_delay_list_remove(new_owner);
        }
        
        mutex->owner = new_owner;
        mutex->lock_count = 1;
        mutex->original_owner_prio = new_owner->base_priority;
        
        mutex->next_held = new_owner->held_mutexes;
        new_owner->held_mutexes = mutex;
        
        rtos_kernel_ready_queue_insert(new_owner);
        
        if (new_owner->effective_priority > task->effective_priority) {
            rtos_kernel_schedule();
        }
    } else {
        mutex->owner = NULL;
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

void rtos_mutex_delete(rtos_mutex_handle_t mutex) {
    if (mutex) {
        rtos_port_enter_critical();
        free(mutex);
        rtos_port_exit_critical();
    }
}
