/**
 * @file 02_preemptive_priority_demo.c
 * @brief Preemptive Priority Scheduling & Preemption Latency Demo
 *
 * Demonstrates a high-priority periodic task preempting a low-priority continuous
 * CPU-bound computational task deterministically.
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>

static volatile uint32_t g_compute_counter = 0;
static volatile bool     g_demo_running = true;

/* Low-Priority Task: Continuous CPU-heavy calculation */
static void low_priority_compute_task(void *arg) {
    (void)arg;
    printf("[Compute-Low] Started CPU-bound computation loop at Priority 2...\n");
    
    while (g_demo_running) {
        g_compute_counter++;
        /* Burns CPU cycles without yielding to prove preemptive interruption */
        for (volatile int i = 0; i < 10000; i++);
    }
    
    printf("[Compute-Low] Computation loop finished. Total cycles recorded: %u\n", g_compute_counter);
}

/* High-Priority Task: Periodic real-time control (runs every 20ms) */
static void high_priority_periodic_task(void *arg) {
    (void)arg;
    rtos_tick_t last_wake = rtos_kernel_get_ticks();
    
    for (int i = 1; i <= 6; i++) {
        /* Delay deterministically for 20 ms */
        rtos_task_delay_until(&last_wake, rtos_ms_to_ticks(20));
        
        printf("  >>> [CONTROL-HIGH] (Priority 8) Preempted compute task! Iteration %d/6 (Compute Counter: %u)\n",
               i, g_compute_counter);
    }
    
    g_demo_running = false;
    
    printf("\n--- Preemptive Execution Statistics ---\n");
    rtos_trace_print_task_stats();
    
    printf("[Demo 2] Preemptive Priority Scheduling Demo Complete!\n");
    exit(0);
}

int main(void) {
    printf("============================================================\n");
    printf("   RTOS Demonstration 02: Preemptive Priority Scheduling    \n");
    printf("============================================================\n");
    
    rtos_kernel_init();
    
    rtos_task_config_t low_cfg = {
        .name = "Compute_Task_Low",
        .entry = low_priority_compute_task,
        .arg = NULL,
        .priority = 2,
        .stack_size = 0
    };
    
    rtos_task_config_t high_cfg = {
        .name = "Periodic_Task_High",
        .entry = high_priority_periodic_task,
        .arg = NULL,
        .priority = 8,
        .stack_size = 0
    };
    
    rtos_task_create(&low_cfg, NULL);
    rtos_task_create(&high_cfg, NULL);
    
    rtos_kernel_start();
    return 0;
}
