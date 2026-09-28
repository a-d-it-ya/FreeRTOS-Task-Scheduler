/**
 * @file rtos_trace.c
 * @brief Runtime Event Tracing, CPU Profiling, and Telemetry Inspector
 */

#include "rtos_trace.h"
#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_port.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if RTOS_CONFIG_TERMINAL_COLORS
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#else
#define COLOR_RESET   ""
#define COLOR_BOLD    ""
#define COLOR_RED     ""
#define COLOR_GREEN   ""
#define COLOR_YELLOW  ""
#define COLOR_BLUE    ""
#define COLOR_MAGENTA ""
#define COLOR_CYAN    ""
#endif

/* Global trace buffer */
static rtos_trace_entry_t g_trace_buffer[RTOS_TRACE_BUFFER_SIZE];
static uint32_t           g_trace_head = 0;
static uint32_t           g_trace_count = 0;

/* External task registry reference from rtos_task.c */
extern rtos_tcb_t* g_task_registry[RTOS_MAX_TASKS];
extern uint32_t   g_task_count;
extern rtos_tcb_t *g_current_task;

void rtos_trace_init(void) {
    g_trace_head = 0;
    g_trace_count = 0;
    memset(g_trace_buffer, 0, sizeof(g_trace_buffer));
}

void rtos_trace_record(rtos_trace_event_type_t event_type,
                       rtos_task_id_t task_id,
                       const char *task_name,
                       uint32_t data_1,
                       uint32_t data_2,
                       const char *fmt, ...) {
#if RTOS_CONFIG_ENABLE_TRACE
    rtos_port_enter_critical();
    
    rtos_trace_entry_t *entry = &g_trace_buffer[g_trace_head];
    entry->timestamp_us = rtos_port_get_time_us();
    entry->tick = rtos_kernel_get_ticks();
    entry->event_type = event_type;
    entry->task_id = task_id;
    entry->data_1 = data_1;
    entry->data_2 = data_2;
    
    if (task_name) {
        strncpy(entry->task_name, task_name, RTOS_TASK_NAME_MAX_LEN - 1);
        entry->task_name[RTOS_TASK_NAME_MAX_LEN - 1] = '\0';
    } else {
        strcpy(entry->task_name, "Kernel");
    }
    
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(entry->message, sizeof(entry->message), fmt, args);
        va_end(args);
    } else {
        entry->message[0] = '\0';
    }
    
    g_trace_head = (g_trace_head + 1) % RTOS_TRACE_BUFFER_SIZE;
    if (g_trace_count < RTOS_TRACE_BUFFER_SIZE) {
        g_trace_count++;
    }
    
    rtos_port_exit_critical();
#else
    (void)event_type; (void)task_id; (void)task_name; (void)data_1; (void)data_2; (void)fmt;
#endif
}

static const char* event_to_str(rtos_trace_event_type_t type) {
    switch (type) {
        case RTOS_TRACE_EVENT_TASK_CREATE:   return "TASK_CREATE";
        case RTOS_TRACE_EVENT_TASK_SWITCH:   return "CTX_SWITCH";
        case RTOS_TRACE_EVENT_TASK_DELAY:    return "TASK_DELAY";
        case RTOS_TRACE_EVENT_TASK_WAKEUP:   return "TASK_WAKEUP";
        case RTOS_TRACE_EVENT_MUTEX_LOCK:    return "MUTEX_LOCK";
        case RTOS_TRACE_EVENT_MUTEX_UNLOCK:  return "MUTEX_UNLOCK";
        case RTOS_TRACE_EVENT_PIP_ELEVATION: return "PIP_ELEVATE";
        case RTOS_TRACE_EVENT_PIP_RESTORE:   return "PIP_RESTORE";
        case RTOS_TRACE_EVENT_QUEUE_SEND:    return "QUEUE_SEND";
        case RTOS_TRACE_EVENT_QUEUE_RECV:    return "QUEUE_RECV";
        case RTOS_TRACE_EVENT_STACK_WARNING: return "STACK_WARN";
        default:                             return "UNKNOWN";
    }
}

static const char* state_to_str(rtos_task_state_t state) {
    switch (state) {
        case RTOS_TASK_STATE_READY:      return COLOR_GREEN "READY" COLOR_RESET;
        case RTOS_TASK_STATE_RUNNING:    return COLOR_CYAN "RUNNING" COLOR_RESET;
        case RTOS_TASK_STATE_BLOCKED:    return COLOR_YELLOW "BLOCKED" COLOR_RESET;
        case RTOS_TASK_STATE_SUSPENDED:  return COLOR_MAGENTA "SUSPEND" COLOR_RESET;
        case RTOS_TASK_STATE_TERMINATED: return COLOR_RED "DEAD" COLOR_RESET;
        default:                         return "UNKNOWN";
    }
}

void rtos_trace_dump_log(void) {
    rtos_port_enter_critical();
    printf("\n" COLOR_BOLD "======================== RTOS Trace Event Log ========================" COLOR_RESET "\n");
    printf("%-10s | %-8s | %-12s | %-16s | %s\n", "Time (us)", "Tick", "Event", "Task", "Details");
    printf("--------------------------------------------------------------------------------\n");
    
    uint32_t start_idx = (g_trace_count < RTOS_TRACE_BUFFER_SIZE) ? 0 : g_trace_head;
    for (uint32_t i = 0; i < g_trace_count; i++) {
        uint32_t idx = (start_idx + i) % RTOS_TRACE_BUFFER_SIZE;
        rtos_trace_entry_t *e = &g_trace_buffer[idx];
        printf("%-10llu | %-8u | %-12s | %-16s | %s\n",
               (unsigned long long)e->timestamp_us,
               e->tick,
               event_to_str(e->event_type),
               e->task_name,
               e->message);
    }
    printf("================================================================================\n\n");
    rtos_port_exit_critical();
}

void rtos_trace_print_task_stats(void) {
    rtos_port_enter_critical();
    
    uint64_t total_system_runtime_us = 0;
    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_task_registry[i]) {
            total_system_runtime_us += g_task_registry[i]->total_runtime_us;
        }
    }
    
    printf("\n" COLOR_BOLD COLOR_CYAN);
    printf("+---------------------------------------------------------------------------------------------------------+\n");
    printf("|                                  RTOS TASK SCHEDULER DIAGNOSTIC MONITOR                                 |\n");
    printf("+----+----------------------+----------+------+------+------------+------------------+----------+---------+\n");
    printf("| ID | Task Name            | State    | Base | Eff  | Sw Counter | Runtime (us)     | CPU (%%)  | FreeStk |\n");
    printf("+----+----------------------+----------+------+------+------------+------------------+----------+---------+\n");
    printf(COLOR_RESET);

    for (uint32_t i = 0; i < g_task_count; i++) {
        rtos_tcb_t *t = g_task_registry[i];
        if (!t) continue;
        
        double cpu_pct = 0.0;
        if (total_system_runtime_us > 0) {
            cpu_pct = ((double)t->total_runtime_us / (double)total_system_runtime_us) * 100.0;
        }
        
        size_t watermark = rtos_task_get_stack_high_watermark(t);
        
        printf("| %-2u | %-20s | %-16s | %-4u | %-4u | %-10u | %-16llu | %6.2f%%  | %-7zu |\n",
               t->tid,
               t->name,
               state_to_str(t->state),
               t->base_priority,
               t->effective_priority,
               t->context_switch_count,
               (unsigned long long)t->total_runtime_us,
               cpu_pct,
               watermark);
    }
    
    printf("+----+----------------------+----------+------+------+------------+------------------+----------+---------+\n");
    printf(" Kernel Ticks: %-8u | Total CPU Measured: %-12llu us | System Tasks: %u\n\n",
           rtos_kernel_get_ticks(),
           (unsigned long long)total_system_runtime_us,
           g_task_count);
    
    rtos_port_exit_critical();
}

void rtos_trace_print_summary(void) {
    rtos_port_enter_critical();
    uint32_t total_switches = 0;
    for (uint32_t i = 0; i < g_task_count; i++) {
        if (g_task_registry[i]) {
            total_switches += g_task_registry[i]->context_switch_count;
        }
    }
    printf("[RTOS] Ticks: %u | Context Switches: %u | Active Tasks: %u\n",
           rtos_kernel_get_ticks(), total_switches, g_task_count);
    rtos_port_exit_critical();
}
