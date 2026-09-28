/**
 * @file test_suite.c
 * @brief Automated Unit and Integration Test Suite for RTOS Micro-Kernel
 */

#include "rtos_kernel.h"
#include "rtos_task.h"
#include "rtos_sync.h"
#include "rtos_queue.h"
#include "rtos_timer.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
    } \
} while(0)

/* Test 1: Queue Unit Test */
static void test_queue_operations(void) {
    printf("\n=== Running Queue Unit Tests ===\n");
    rtos_queue_handle_t q = rtos_queue_create(3, sizeof(int), "TestQ");
    TEST_ASSERT(q != NULL, "Queue created successfully");
    TEST_ASSERT(rtos_queue_get_count(q) == 0, "Queue initially empty");
    TEST_ASSERT(rtos_queue_get_spaces_available(q) == 3, "Queue initial capacity correct");
    
    int val1 = 10, val2 = 20, val3 = 30, val4 = 40;
    TEST_ASSERT(rtos_queue_send(q, &val1, RTOS_NO_WAIT) == RTOS_STATUS_OK, "Send item 1");
    TEST_ASSERT(rtos_queue_send(q, &val2, RTOS_NO_WAIT) == RTOS_STATUS_OK, "Send item 2");
    TEST_ASSERT(rtos_queue_send(q, &val3, RTOS_NO_WAIT) == RTOS_STATUS_OK, "Send item 3");
    TEST_ASSERT(rtos_queue_send(q, &val4, RTOS_NO_WAIT) == RTOS_STATUS_QUEUE_FULL, "Send item 4 fails (queue full)");
    
    int peek_val = 0;
    TEST_ASSERT(rtos_queue_peek(q, &peek_val) == RTOS_STATUS_OK && peek_val == 10, "Peek item matches first value");
    
    int recv_val = 0;
    TEST_ASSERT(rtos_queue_receive(q, &recv_val, RTOS_NO_WAIT) == RTOS_STATUS_OK && recv_val == 10, "Receive item 1 (FIFO order)");
    TEST_ASSERT(rtos_queue_receive(q, &recv_val, RTOS_NO_WAIT) == RTOS_STATUS_OK && recv_val == 20, "Receive item 2 (FIFO order)");
    TEST_ASSERT(rtos_queue_receive(q, &recv_val, RTOS_NO_WAIT) == RTOS_STATUS_OK && recv_val == 30, "Receive item 3 (FIFO order)");
    TEST_ASSERT(rtos_queue_receive(q, &recv_val, RTOS_NO_WAIT) == RTOS_STATUS_QUEUE_EMPTY, "Receive on empty queue returns RTOS_STATUS_QUEUE_EMPTY");
    
    rtos_queue_delete(q);
}

/* Test 2: Semaphore Unit Test */
static void test_semaphore_operations(void) {
    printf("\n=== Running Semaphore Unit Tests ===\n");
    rtos_sem_handle_t sem = rtos_sem_create(2, 3, "TestSem");
    TEST_ASSERT(sem != NULL, "Semaphore created with count 2, max 3");
    
    TEST_ASSERT(rtos_sem_take(sem, RTOS_NO_WAIT) == RTOS_STATUS_OK, "Take sem token 1");
    TEST_ASSERT(rtos_sem_take(sem, RTOS_NO_WAIT) == RTOS_STATUS_OK, "Take sem token 2");
    TEST_ASSERT(rtos_sem_take(sem, RTOS_NO_WAIT) == RTOS_STATUS_TIMEOUT, "Take sem token 3 fails on empty");
    
    TEST_ASSERT(rtos_sem_give(sem) == RTOS_STATUS_OK, "Give token 1");
    TEST_ASSERT(rtos_sem_give(sem) == RTOS_STATUS_OK, "Give token 2");
    TEST_ASSERT(rtos_sem_give(sem) == RTOS_STATUS_OK, "Give token 3 (max count)");
    TEST_ASSERT(rtos_sem_give(sem) == RTOS_STATUS_OK, "Give token 4 (clamped to max count)");
    
    rtos_sem_delete(sem);
}

/* Test Runner Task inside RTOS environment */
static void test_runner_task(void *arg) {
    (void)arg;
    printf("\n============================================================\n");
    printf("   RTOS COMPREHENSIVE AUTOMATED TEST SUITE EXECUTION        \n");
    printf("============================================================\n");
    
    test_queue_operations();
    test_semaphore_operations();
    
    printf("\n--- Stack Watermark & Canary Check ---\n");
    rtos_tcb_t *curr = rtos_task_get_current();
    size_t watermark = rtos_task_get_stack_high_watermark(curr);
    TEST_ASSERT(watermark > 0, "Stack canary intact and watermark detected");
    printf("  [INFO] Unused stack headroom: %zu bytes\n", watermark);
    
    printf("\n============================================================\n");
    printf("   TEST SUITE SUMMARY: %d PASSED, %d FAILED                \n", g_tests_passed, g_tests_failed);
    printf("============================================================\n");
    
    exit(g_tests_failed > 0 ? 1 : 0);
}

int main(void) {
    rtos_kernel_init();
    
    rtos_task_config_t test_cfg = {
        .name = "TestRunner",
        .entry = test_runner_task,
        .arg = NULL,
        .priority = 10,
        .stack_size = 0
    };
    
    rtos_task_create(&test_cfg, NULL);
    rtos_kernel_start();
    return 0;
}
