/**
 * @file 01_cooperative_demo.c
 * @brief Cooperative Multitasking Demonstration
 *
 * Demonstrates voluntary CPU yielding with rtos_task_yield() among equal-priority tasks.
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>

#define NUM_ITERATIONS 5

static void worker_task(void *arg) {
    const char *name = (const char*)arg;
    
    for (int i = 1; i <= NUM_ITERATIONS; i++) {
        printf("[Cooperative] %s -> Step %d/%d (Executing)\n", name, i, NUM_ITERATIONS);
        
        /* Cooperatively yield execution to peer tasks */
        rtos_task_yield();
    }
    
    printf("[Cooperative] %s -> Completed all steps!\n", name);
}

static void monitor_task(void *arg) {
    (void)arg;
    
    /* Delay to allow workers to complete */
    rtos_task_delay_ms(100);
    
    printf("\n--- Final Diagnostic State ---\n");
    rtos_trace_print_task_stats();
    
    printf("[Demo 1] Cooperative Multitasking Demo Complete!\n");
    exit(0);
}

int main(void) {
    printf("============================================================\n");
    printf("   RTOS Demonstration 01: Cooperative Multitasking          \n");
    printf("============================================================\n");
    
    rtos_kernel_init();
    
    rtos_task_config_t cfg_a = { .name = "Task_Alpha", .entry = worker_task, .arg = "Alpha", .priority = 3, .stack_size = 0 };
    rtos_task_config_t cfg_b = { .name = "Task_Beta",  .entry = worker_task, .arg = "Beta",  .priority = 3, .stack_size = 0 };
    rtos_task_config_t cfg_c = { .name = "Task_Gamma", .entry = worker_task, .arg = "Gamma", .priority = 3, .stack_size = 0 };
    rtos_task_config_t cfg_m = { .name = "Monitor",    .entry = monitor_task, .arg = NULL,    .priority = 1, .stack_size = 0 };
    
    rtos_task_create(&cfg_a, NULL);
    rtos_task_create(&cfg_b, NULL);
    rtos_task_create(&cfg_c, NULL);
    rtos_task_create(&cfg_m, NULL);
    
    rtos_kernel_start();
    return 0;
}
