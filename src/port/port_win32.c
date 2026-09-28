/**
 * @file port_win32.c
 * @brief Win32 Host Platform Port Implementation
 *
 * Emulates hardware context switching using Win32 Fibers and hardware timer
 * interrupts using a high-precision multimedia timer thread.
 */

#if defined(_WIN32) || defined(_WIN64)

#include "rtos_port.h"
#include "rtos_kernel.h"
#include "rtos_trace.h"
#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "winmm.lib")

/* Structure for passing entry point and argument to fiber entry wrapper */
typedef struct {
    rtos_task_func_t entry;
    void            *arg;
} fiber_param_t;

static CRITICAL_SECTION g_kernel_cs;
static bool             g_cs_initialized = false;
static void            *g_main_fiber = NULL;
static HANDLE           g_tick_thread = NULL;
static volatile bool    g_tick_running = false;
static void           (*g_tick_callback)(void) = NULL;
static LARGE_INTEGER    g_perf_freq;

static VOID CALLBACK fiber_entry_wrapper(PVOID lpParameter) {
    fiber_param_t *param = (fiber_param_t*)lpParameter;
    rtos_task_func_t func = param->entry;
    void *arg = param->arg;
    free(param);
    
    /* Call user task entry function */
    func(arg);
    
    /* If task returns, clean up */
    rtos_task_delete(NULL);
}

void rtos_port_init(void) {
    if (!g_cs_initialized) {
        InitializeCriticalSection(&g_kernel_cs);
        QueryPerformanceFrequency(&g_perf_freq);
        g_cs_initialized = true;
    }
    
    if (g_main_fiber == NULL) {
        g_main_fiber = ConvertThreadToFiber(NULL);
        if (g_main_fiber == NULL) {
            g_main_fiber = GetCurrentFiber();
        }
    }
}

void* rtos_port_create_context(void *stack_base, size_t stack_size, rtos_task_func_t entry, void *arg) {
    (void)stack_base; /* Win32 fibers manage virtual stack allocation */
    
    fiber_param_t *param = (fiber_param_t*)malloc(sizeof(fiber_param_t));
    if (!param) return NULL;
    
    param->entry = entry;
    param->arg = arg;
    
    void *fiber = CreateFiberEx(stack_size / 2, stack_size, 0, (LPFIBER_START_ROUTINE)fiber_entry_wrapper, param);
    if (!fiber) {
        free(param);
        return NULL;
    }
    
    return fiber;
}

void rtos_port_destroy_context(void *context) {
    if (context && context != g_main_fiber) {
        DeleteFiber(context);
    }
}

void rtos_port_context_switch(void *next_context) {
    if (next_context != NULL) {
        SwitchToFiber(next_context);
    }
}

void rtos_port_enter_critical(void) {
    if (g_cs_initialized) {
        EnterCriticalSection(&g_kernel_cs);
    }
}

void rtos_port_exit_critical(void) {
    if (g_cs_initialized) {
        LeaveCriticalSection(&g_kernel_cs);
    }
}

static DWORD WINAPI tick_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    /* High resolution sleep loop */
    timeBeginPeriod(1);
    
    while (g_tick_running) {
        Sleep(1); /* 1ms resolution */
        
        if (g_tick_callback && rtos_scheduler_is_running()) {
            rtos_port_enter_critical();
            g_tick_callback();
            rtos_port_exit_critical();
        }
    }
    
    timeEndPeriod(1);
    return 0;
}

void rtos_port_start_tick_timer(void (*tick_handler)(void)) {
    g_tick_callback = tick_handler;
    g_tick_running = true;
    
    g_tick_thread = CreateThread(NULL, 0, tick_thread_proc, NULL, 0, NULL);
    if (g_tick_thread != NULL) {
        SetThreadPriority(g_tick_thread, THREAD_PRIORITY_TIME_CRITICAL);
    }
}

void rtos_port_stop_tick_timer(void) {
    g_tick_running = false;
    if (g_tick_thread != NULL) {
        WaitForSingleObject(g_tick_thread, 100);
        CloseHandle(g_tick_thread);
        g_tick_thread = NULL;
    }
}

void rtos_port_idle_sleep(uint32_t ms) {
    Sleep(ms > 0 ? ms : 1);
}

uint64_t rtos_port_get_time_us(void) {
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    if (g_perf_freq.QuadPart == 0) return 0;
    return (uint64_t)((count.QuadPart * 1000000ULL) / g_perf_freq.QuadPart);
}

#endif /* _WIN32 || _WIN64 */
