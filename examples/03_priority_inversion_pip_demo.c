/**
 * @file 03_priority_inversion_pip_demo.c
 * @brief Priority Inversion & Priority Inheritance Protocol (PIP) Demonstration
 *
 * Recreates the classic Mars Pathfinder priority inversion challenge:
 * 1. Low Priority (Prio 2) acquires shared hardware bus mutex.
 * 2. High Priority (Prio 10) wakes up and requests the mutex, blocking.
 * 3. Medium Priority (Prio 5) wakes up and tries to preempt Low Priority.
 * 4. With PIP enabled, Low Priority inherits Priority 10, preventing Medium
 *    from causing unbounded priority inversion!
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_sync.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>

static rtos_mutex_handle_t g_shared_bus_mutex = NULL;
static volatile bool       g_med_finished = false;

/* Low-Priority Task (Prio 2): Reads sensor bus */
static void low_prio_task(void *arg) {
    (void)arg;
    printf("[LOW (Prio 2)] Starting. Attempting to lock Shared Bus Mutex...\n");
    
    rtos_mutex_lock(g_shared_bus_mutex, RTOS_WAIT_FOREVER);
    printf("[LOW (Prio 2)] Acquired mutex! Simulating critical bus transfer...\n");
    
    /* Delay to allow High and Medium priority tasks to arrive */
    rtos_task_delay_ms(30);
    
    printf("[LOW (Prio 2)] Finished critical section. Releasing mutex...\n");
    rtos_mutex_unlock(g_shared_bus_mutex);
    
    printf("[LOW (Prio 2)] Task complete.\n");
}

/* Medium-Priority Task (Prio 5): Long computational task */
static void med_prio_task(void *arg) {
    (void)arg;
    /* Delay 10ms so Low acquires mutex first */
    rtos_task_delay_ms(10);
    
    printf("  [MED (Prio 5)] Woke up! Attempting CPU compute without mutex...\n");
    
    /* Simulate CPU work */
    for (int i = 0; i < 3; i++) {
        rtos_task_delay_ms(10);
        printf("  [MED (Prio 5)] Executing compute batch %d/3\n", i + 1);
    }
    
    printf("  [MED (Prio 5)] Compute complete.\n");
    g_med_finished = true;
}

/* High-Priority Task (Prio 10): Urgent flight attitude control */
static void high_prio_task(void *arg) {
    (void)arg;
    /* Delay 15ms so Low acquires mutex first and Medium is active */
    rtos_task_delay_ms(15);
    
    printf("    >>> [HIGH (Prio 10)] Woke up! Requesting Shared Bus Mutex...\n");
    
    uint64_t request_time = rtos_port_get_time_us();
    rtos_mutex_lock(g_shared_bus_mutex, RTOS_WAIT_FOREVER);
    uint64_t acquire_time = rtos_port_get_time_us();
    
    printf("    >>> [HIGH (Prio 10)] Mutex acquired successfully! Blocked time: %llu us\n",
           (unsigned long long)(acquire_time - request_time));
    
    rtos_task_delay_ms(10);
    rtos_mutex_unlock(g_shared_bus_mutex);
    
    printf("    >>> [HIGH (Prio 10)] Critical flight command executed!\n");
    
    /* Wait for rest to settle and display stats */
    rtos_task_delay_ms(50);
    
    printf("\n--- Trace Event Log of Priority Inheritance ---\n");
    rtos_trace_dump_log();
    
    printf("\n--- Task State Metrics ---\n");
    rtos_trace_print_task_stats();
    
    printf("[Demo 3] Priority Inversion & PIP Validation Complete!\n");
    exit(0);
}

int main(void) {
    printf("============================================================\n");
    printf("   RTOS Demonstration 03: Priority Inheritance Protocol     \n");
    printf("============================================================\n");
    
    rtos_kernel_init();
    
    g_shared_bus_mutex = rtos_mutex_create("I2C_Sensor_Bus_Mutex");
    
    rtos_task_config_t low_cfg  = { .name = "Task_Low_Prio",  .entry = low_prio_task,  .arg = NULL, .priority = 2,  .stack_size = 0 };
    rtos_task_config_t med_cfg  = { .name = "Task_Med_Prio",  .entry = med_prio_task,  .arg = NULL, .priority = 5,  .stack_size = 0 };
    rtos_task_config_t high_cfg = { .name = "Task_High_Prio", .entry = high_prio_task, .arg = NULL, .priority = 10, .stack_size = 0 };
    
    rtos_task_create(&low_cfg, NULL);
    rtos_task_create(&med_cfg, NULL);
    rtos_task_create(&high_cfg, NULL);
    
    rtos_kernel_start();
    return 0;
}
