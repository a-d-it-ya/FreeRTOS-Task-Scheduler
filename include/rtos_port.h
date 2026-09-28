/**
 * @file rtos_port.h
 * @brief Platform & Architecture Porting Interface
 *
 * Defines machine-dependent abstractions: context switching, stack frame initialization,
 * critical sections (interrupt mask emulation), and timer tick triggers.
 */

#ifndef RTOS_PORT_H_
#define RTOS_PORT_H_

#include "rtos_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Platform initialization (converts main thread to primary context/fiber) */
void rtos_port_init(void);

/** Allocate and initialize machine context/stack for a new task */
void* rtos_port_create_context(void *stack_base, size_t stack_size, rtos_task_func_t entry, void *arg);

/** Destroy context resource if host-allocated */
void rtos_port_destroy_context(void *context);

/** Trigger a synchronous context switch from old task to new task */
void rtos_port_context_switch(void *next_context);

/** Enter critical section (disable tick / task switching) */
void rtos_port_enter_critical(void);

/** Exit critical section */
void rtos_port_exit_critical(void);

/** Start the high-resolution hardware/host timer tick generator */
void rtos_port_start_tick_timer(void (*tick_handler)(void));

/** Stop the timer tick generator */
void rtos_port_stop_tick_timer(void);

/** Sleep the host CPU for a short duration (idle task) */
void rtos_port_idle_sleep(uint32_t ms);

/** Get high-resolution monotonic timestamp in microseconds */
uint64_t rtos_port_get_time_us(void);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_PORT_H_ */
