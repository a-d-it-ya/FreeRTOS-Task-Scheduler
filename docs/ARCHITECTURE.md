# 🏗️ Micro-RTOS Kernel Architecture Specification

This document provides a technical specification of the micro-kernel architecture, data structures, scheduling invariants, and memory layout.

---

## 1. System Block Diagram

```
+-------------------------------------------------------------------------------+
|                             Application Tasks                                 |
|  (Flight Control, Telemetry, Sensor Acquisition, Diagnostic Supervisors)      |
+-------------------------------------------------------------------------------+
                                      |
                                      v
+-------------------------------------------------------------------------------+
|                       Kernel API & Synchronization                            |
|  +---------------------+  +---------------------+  +------------------------+ |
|  | Task Management API |  | Mutex with PIP      |  | Message Queues (FIFO)  | |
|  | (Create/Yield/Delay)|  | Counting Semaphores |  | Software Timers        | |
|  +---------------------+  +---------------------+  +------------------------+ |
+-------------------------------------------------------------------------------+
                                      |
                                      v
+-------------------------------------------------------------------------------+
|                            Kernel Core Scheduler                              |
|  - 32-Level Ready Queue with Bitmapped Indexing (O(1) highest-priority lookup)|
|  - Delta-Sorted Delay List for Deterministic Timer Wakeups                    |
|  - Context Switch Dispatcher & CPU Profiler                                   |
|  - Stack Canary Integrity Verifier (0xDEADBEEF)                               |
+-------------------------------------------------------------------------------+
                                      |
                                      v
+-------------------------------------------------------------------------------+
|                         Platform Porting Layer (HAL)                          |
|  - Win32 Fibers / POSIX ucontext Stack Frame Switcher                         |
|  - Periodic High-Resolution Timer Interrupt Emulator                          |
|  - Critical Section / Interrupt Mask Emulation                                |
+-------------------------------------------------------------------------------+
```

---

## 2. Task Control Block (TCB) Layout

The TCB encapsulates all state required to preserve, dispatch, and monitor a task:

| Member | Type | Purpose |
| :--- | :--- | :--- |
| `context` | `void*` | Machine stack context (Fiber pointer or ucontext pointer) |
| `stack_base` | `void*` | Base address of the allocated task stack |
| `stack_size` | `size_t` | Total configured stack memory in bytes |
| `tid` | `rtos_task_id_t` | Unique sequential Task ID |
| `name` | `char[32]` | Human-readable task identifier for telemetry |
| `base_priority` | `rtos_priority_t` | Statically assigned nominal priority (0–31) |
| `effective_priority` | `rtos_priority_t` | Dynamically adjusted priority (elevated via PIP) |
| `state` | `rtos_task_state_t` | `READY`, `RUNNING`, `BLOCKED`, `SUSPENDED`, `TERMINATED` |
| `wake_tick` | `rtos_tick_t` | Absolute tick at which delay or timeout expires |
| `time_slice_remaining`| `rtos_tick_t` | Remaining ticks in round-robin quantum |
| `total_runtime_us` | `uint64_t` | Cumulative CPU time consumed in microseconds |
| `last_switched_in_us` | `uint64_t` | Monotonic timestamp when task gained CPU |
| `context_switch_count`| `uint32_t` | Total times this task was scheduled |
| `held_mutexes` | `rtos_mutex_t*` | Singly-linked list of currently held mutexes (for PIP) |
| `next` / `prev` | `rtos_tcb_t*` | Doubly-linked list pointers for queue placement |

---

## 3. Ready Queue Design & $O(1)$ Selection

The ready queue consists of an array of 32 doubly-linked circular task lists:
$$\text{ready\_queues}[0 \dots 31]$$

A 32-bit integer (`g_ready_priority_bitmap`) tracks which priority levels contain active tasks.

### Priority Selection:
```c
uint32_t top_prio = 31U - __builtin_clz(g_ready_priority_bitmap);
```
- **Time Complexity**: Exactly $O(1)$ operations regardless of the number of registered tasks.
- **Round-Robin**: When multiple tasks exist at `top_prio`, the dispatcher schedules `ready_queues[top_prio].head`, and shifts expired tasks to `ready_queues[top_prio].tail`.

---

## 4. Stack Canary Protection

During task creation, the entire allocated stack is filled with the sentinel pattern `0xDEADBEEF`.
When `rtos_task_get_stack_high_watermark()` is queried, the stack is scanned from the base upwards:
1. It counts intact canary words until the first overwritten word.
2. The remaining size represents the maximum stack headroom (minimum unused stack space observed).
3. If the first word at the base is corrupted, a stack overflow alert is raised immediately.
