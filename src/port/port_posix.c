/**
 * @file port_posix.c
 * @brief POSIX Host Platform Port Implementation
 *
 * Implements context switching using ucontext / signal timers for Linux and macOS.
 */

#if !defined(_WIN32) && !defined(_WIN64)

#define _GNU_SOURCE
#include "rtos_port.h"
#include "rtos_kernel.h"
#include "rtos_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>

typedef struct {
    ucontext_t ctx;
    void *stack;
    size_t stack_size;
} posix_context_t;

static pthread_mutex_t  g_kernel_mutex = PTHREAD_MUTEX_INITIALIZER;
static posix_context_t *g_current_ctx = NULL;
static pthread_t        g_tick_thread;
static volatile bool    g_tick_running = false;
static void           (*g_tick_callback)(void) = NULL;

void rtos_port_init(void) {
    /* Initialize main context container */
}

static void posix_task_trampoline(uint32_t entry_high, uint32_t entry_low, uint32_t arg_high, uint32_t arg_low) {
    uintptr_t entry_ptr = ((uintptr_t)entry_high << 32) | (uintptr_t)entry_low;
    uintptr_t arg_ptr   = ((uintptr_t)arg_high << 32) | (uintptr_t)arg_low;
    
    rtos_task_func_t entry = (rtos_task_func_t)entry_ptr;
    void *arg = (void*)arg_ptr;
    
    entry(arg);
    rtos_task_delete(NULL);
}

void* rtos_port_create_context(void *stack_base, size_t stack_size, rtos_task_func_t entry, void *arg) {
    posix_context_t *ctx = (posix_context_t*)malloc(sizeof(posix_context_t));
    if (!ctx) return NULL;
    
    if (getcontext(&ctx->ctx) != 0) {
        free(ctx);
        return NULL;
    }
    
    ctx->stack = stack_base ? stack_base : malloc(stack_size);
    ctx->stack_size = stack_size;
    ctx->ctx.uc_stack.ss_sp = ctx->stack;
    ctx->ctx.uc_stack.ss_size = stack_size;
    ctx->ctx.uc_link = NULL;
    
    uintptr_t ep = (uintptr_t)entry;
    uintptr_t ap = (uintptr_t)arg;
    uint32_t eh = (uint32_t)(ep >> 32);
    uint32_t el = (uint32_t)(ep & 0xFFFFFFFF);
    uint32_t ah = (uint32_t)(ap >> 32);
    uint32_t al = (uint32_t)(ap & 0xFFFFFFFF);
    
    makecontext(&ctx->ctx, (void(*)(void))posix_task_trampoline, 4, eh, el, ah, al);
    return ctx;
}

void rtos_port_destroy_context(void *context) {
    if (context) {
        posix_context_t *ctx = (posix_context_t*)context;
        free(ctx);
    }
}

void rtos_port_context_switch(void *next_context) {
    posix_context_t *next = (posix_context_t*)next_context;
    posix_context_t *prev = g_current_ctx;
    g_current_ctx = next;
    
    if (prev != NULL && prev != next) {
        swapcontext(&prev->ctx, &next->ctx);
    } else if (next != NULL) {
        setcontext(&next->ctx);
    }
}

void rtos_port_enter_critical(void) {
    pthread_mutex_lock(&g_kernel_mutex);
}

void rtos_port_exit_critical(void) {
    pthread_mutex_unlock(&g_kernel_mutex);
}

static void* posix_tick_worker(void *arg) {
    (void)arg;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 1000000; /* 1 ms */
    
    while (g_tick_running) {
        nanosleep(&ts, NULL);
        if (g_tick_callback && rtos_scheduler_is_running()) {
            rtos_port_enter_critical();
            g_tick_callback();
            rtos_port_exit_critical();
        }
    }
    return NULL;
}

void rtos_port_start_tick_timer(void (*tick_handler)(void)) {
    g_tick_callback = tick_handler;
    g_tick_running = true;
    pthread_create(&g_tick_thread, NULL, posix_tick_worker, NULL);
}

void rtos_port_stop_tick_timer(void) {
    g_tick_running = false;
    pthread_join(g_tick_thread, NULL);
}

void rtos_port_idle_sleep(uint32_t ms) {
    usleep(ms * 1000);
}

uint64_t rtos_port_get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

#endif /* !_WIN32 && !_WIN64 */
