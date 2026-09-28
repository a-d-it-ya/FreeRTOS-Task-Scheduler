/**
 * @file rtos_task.c
 * @brief Task Management, TCB Lifecycle, and Priority Management
 */

#include "rtos_task.h"
#include "rtos_kernel.h"
#include "rtos_port.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global Task Registry */
rtos_tcb_t *g_task_registry[RTOS_MAX_TASKS] = {0};
uint32_t   g_task_count = 0;
static rtos_task_id_t g_next_task_id = 1;

extern rtos_tcb_t *g_current_task;

/* Initialize stack canary words across memory */
static void init_stack_canaries(void *stack_base, size_t stack_size) {
    if (!stack_base || stack_size < sizeof(uint32_t)) return;
    
    uint32_t *words = (uint32_t*)stack_base;
    size_t num_words = stack_size / sizeof(uint32_t);
    
    for (size_t i = 0; i < num_words; i++) {
        words[i] = RTOS_STACK_CANARY_WORD;
    }
}

rtos_status_t rtos_task_create(const rtos_task_config_t *config, rtos_task_handle_t *out_handle) {
    if (!config || !config->entry || config->priority >= RTOS_MAX_PRIORITIES) {
        return RTOS_STATUS_INVALID_PARAM;
    }
    
    rtos_port_enter_critical();
    
    if (g_task_count >= RTOS_MAX_TASKS) {
        rtos_port_exit_critical();
        return RTOS_STATUS_OUT_OF_MEMORY;
    }
    
    size_t stack_sz = (config->stack_size < RTOS_MIN_STACK_SIZE) ? RTOS_DEFAULT_STACK_SIZE : config->stack_size;
    
    rtos_tcb_t *tcb = (rtos_tcb_t*)calloc(1, sizeof(rtos_tcb_t));
    if (!tcb) {
        rtos_port_exit_critical();
        return RTOS_STATUS_OUT_OF_MEMORY;
    }
    
    tcb->stack_size = stack_sz;
    tcb->stack_base = malloc(stack_sz);
    if (!tcb->stack_base) {
        free(tcb);
        rtos_port_exit_critical();
        return RTOS_STATUS_OUT_OF_MEMORY;
    }
    
    init_stack_canaries(tcb->stack_base, stack_sz);
    
    tcb->context = rtos_port_create_context(tcb->stack_base, stack_sz, config->entry, config->arg);
    if (!tcb->context) {
        free(tcb->stack_base);
        free(tcb);
        rtos_port_exit_critical();
        return RTOS_STATUS_OUT_OF_MEMORY;
    }
    
    tcb->tid = g_next_task_id++;
    tcb->base_priority = config->priority;
    tcb->effective_priority = config->priority;
    tcb->state = RTOS_TASK_STATE_READY;
    tcb->time_slice_remaining = RTOS_TIME_SLICE_TICKS;
    tcb->total_runtime_us = 0;
    tcb->context_switch_count = 0;
    tcb->held_mutexes = NULL;
    
    if (config->name) {
        strncpy(tcb->name, config->name, RTOS_TASK_NAME_MAX_LEN - 1);
        tcb->name[RTOS_TASK_NAME_MAX_LEN - 1] = '\0';
    } else {
        snprintf(tcb->name, sizeof(tcb->name), "Task_%u", tcb->tid);
    }
    
    /* Register in global table */
    g_task_registry[g_task_count++] = tcb;
    
    /* Add to ready queue */
    rtos_kernel_ready_queue_insert(tcb);
    
    rtos_trace_record(RTOS_TRACE_EVENT_TASK_CREATE, tcb->tid, tcb->name, tcb->base_priority, (uint32_t)stack_sz,
                      "Created with Prio=%u Stack=%u", tcb->base_priority, (uint32_t)stack_sz);
    
    if (out_handle) {
        *out_handle = tcb;
    }
    
    /* If scheduler running and newly created task has higher priority, trigger schedule */
    if (rtos_scheduler_is_running() && g_current_task && tcb->effective_priority > g_current_task->effective_priority) {
        rtos_kernel_schedule();
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

void rtos_task_yield(void) {
    rtos_port_enter_critical();
    if (g_current_task && g_current_task->state == RTOS_TASK_STATE_RUNNING) {
        g_current_task->state = RTOS_TASK_STATE_READY;
        g_current_task->time_slice_remaining = RTOS_TIME_SLICE_TICKS;
        rtos_kernel_ready_queue_insert(g_current_task);
        rtos_kernel_schedule();
    }
    rtos_port_exit_critical();
}

void rtos_task_delay_ticks(rtos_tick_t ticks) {
    if (ticks == 0) {
        rtos_task_yield();
        return;
    }
    
    rtos_port_enter_critical();
    if (g_current_task) {
        g_current_task->state = RTOS_TASK_STATE_BLOCKED;
        rtos_kernel_delay_list_insert(g_current_task, ticks);
        
        rtos_trace_record(RTOS_TRACE_EVENT_TASK_DELAY, g_current_task->tid, g_current_task->name,
                          ticks, rtos_kernel_get_ticks() + ticks, "Delaying for %u ticks", ticks);
        
        rtos_kernel_schedule();
    }
    rtos_port_exit_critical();
}

void rtos_task_delay_ms(uint32_t ms) {
    rtos_task_delay_ticks(rtos_ms_to_ticks(ms));
}

void rtos_task_delay_until(rtos_tick_t *previous_wake_tick, rtos_tick_t increment_ticks) {
    if (!previous_wake_tick) return;
    
    rtos_tick_t current = rtos_kernel_get_ticks();
    rtos_tick_t next_wake = *previous_wake_tick + increment_ticks;
    
    if (next_wake > current) {
        rtos_tick_t wait_ticks = next_wake - current;
        *previous_wake_tick = next_wake;
        rtos_task_delay_ticks(wait_ticks);
    } else {
        *previous_wake_tick = current;
        rtos_task_yield();
    }
}

rtos_status_t rtos_task_suspend(rtos_task_handle_t task) {
    rtos_port_enter_critical();
    rtos_tcb_t *target = (task != NULL) ? task : g_current_task;
    if (!target) {
        rtos_port_exit_critical();
        return RTOS_STATUS_INVALID_PARAM;
    }
    
    if (target->state == RTOS_TASK_STATE_READY) {
        rtos_kernel_ready_queue_remove(target);
    } else if (target->state == RTOS_TASK_STATE_BLOCKED) {
        rtos_kernel_delay_list_remove(target);
    }
    
    target->state = RTOS_TASK_STATE_SUSPENDED;
    
    if (target == g_current_task) {
        rtos_kernel_schedule();
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_task_resume(rtos_task_handle_t task) {
    if (!task) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    if (task->state == RTOS_TASK_STATE_SUSPENDED) {
        task->state = RTOS_TASK_STATE_READY;
        rtos_kernel_ready_queue_insert(task);
        if (g_current_task && task->effective_priority > g_current_task->effective_priority) {
            rtos_kernel_schedule();
        }
    }
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_status_t rtos_task_set_priority(rtos_task_handle_t task, rtos_priority_t new_priority) {
    if (new_priority >= RTOS_MAX_PRIORITIES) return RTOS_STATUS_INVALID_PARAM;
    
    rtos_port_enter_critical();
    rtos_tcb_t *target = (task != NULL) ? task : g_current_task;
    if (!target) {
        rtos_port_exit_critical();
        return RTOS_STATUS_INVALID_PARAM;
    }
    
    target->base_priority = new_priority;
    if (target->held_mutexes == NULL) {
        target->effective_priority = new_priority;
    }
    
    if (target->state == RTOS_TASK_STATE_READY) {
        rtos_kernel_ready_queue_remove(target);
        rtos_kernel_ready_queue_insert(target);
    }
    
    if (rtos_scheduler_is_running()) {
        rtos_kernel_schedule();
    }
    
    rtos_port_exit_critical();
    return RTOS_STATUS_OK;
}

rtos_task_handle_t rtos_task_get_current(void) {
    return g_current_task;
}

size_t rtos_task_get_stack_high_watermark(rtos_tcb_t *task) {
    if (!task || !task->stack_base) return 0;
    
    uint32_t *words = (uint32_t*)task->stack_base;
    size_t num_words = task->stack_size / sizeof(uint32_t);
    size_t free_words = 0;
    
    for (size_t i = 0; i < num_words; i++) {
        if (words[i] == RTOS_STACK_CANARY_WORD) {
            free_words++;
        } else {
            break;
        }
    }
    
    return free_words * sizeof(uint32_t);
}

void rtos_task_delete(rtos_task_handle_t task) {
    rtos_port_enter_critical();
    rtos_tcb_t *target = (task != NULL) ? task : g_current_task;
    if (!target) {
        rtos_port_exit_critical();
        return;
    }
    
    if (target->state == RTOS_TASK_STATE_READY) {
        rtos_kernel_ready_queue_remove(target);
    } else if (target->state == RTOS_TASK_STATE_BLOCKED) {
        rtos_kernel_delay_list_remove(target);
    }
    
    target->state = RTOS_TASK_STATE_TERMINATED;
    
    if (target == g_current_task) {
        rtos_kernel_schedule();
    }
    
    rtos_port_exit_critical();
}
