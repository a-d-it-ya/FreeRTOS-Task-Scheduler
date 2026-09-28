# 🎯 RTOS Task Scheduler — Interview Preparation & Technical Deep-Dive

This guide prepares you to speak confidently about this project during senior firmware, embedded systems, and RTOS technical interviews.

---

## 📌 Executive Summary (30-Second Elevator Pitch)

> *"In this project, I built a deterministic, priority-based preemptive and cooperative RTOS task scheduler and micro-kernel from scratch in C11. It implements multi-level ready queues with $O(1)$ bitmapped priority lookups, Priority Inheritance Protocol (PIP) on mutexes to prevent unbounded priority inversion, thread-safe circular message queues, and a delta-sorted delay list. To validate the scheduler under mission-critical constraints, I created a simulated aerospace flight computer multi-rate sensor pipeline with IMU, Flight Guidance, and Telemetry tasks, coupled with a live CPU profiler and stack high-watermark monitor."*

---

## 💡 Top Interview Questions & Expert Answers

### Q1: How does your scheduler achieve $O(1)$ priority selection?
**Answer**:
> *"Instead of iterating through an array of tasks or scanning a linked list, I maintain an array of 32 ready queues (one for each priority level) and a 32-bit integer bitmap where bit $k$ indicates whether priority level $k$ contains ready tasks. Selecting the highest-priority task is a single hardware instruction: finding the most significant set bit using `__builtin_clz` (`Count Leading Zeros`) or `_BitScanReverse` on x86/ARM Cortex. Once the top priority is known, we dequeue the task at the head of that level's queue in $O(1)$ time."*

---

### Q2: What is Priority Inversion and how does your kernel resolve it?
**Answer**:
> *"Priority inversion occurs when a low-priority task holds a mutex needed by a high-priority task, and a medium-priority task (not needing the mutex) preempts the low-priority task, causing the high-priority task to be delayed indefinitely.
> 
> In my kernel, I implemented the **Priority Inheritance Protocol (PIP)**. When the high-priority task attempts to lock the mutex and blocks, the kernel dynamically elevates the low-priority owner's effective priority to equal the high-priority task's priority. This prevents medium-priority tasks from preempting it. Once the low-priority task finishes its critical section and unlocks the mutex, its priority is restored to its base priority, and the high-priority task immediately acquires ownership."*

---

### Q3: How do you detect and prevent stack overflow in an embedded RTOS?
**Answer**:
> *"I implement stack canary watermarking. During task initialization, the stack memory is pre-filled with a known pattern (`0xDEADBEEF`). The kernel can inspect the stack from the base upward: the number of untouched canary words indicates the minimum unused stack space (the high-watermark). If the canary word at the very base of the stack is corrupted, the kernel detects a stack overflow and triggers a diagnostic alert before data corruption can propagate."*

---

### Q4: How does your delay list avoid $O(N)$ tick processing?
**Answer**:
> *"Instead of decrementing a counter on every sleeping task on every single millisecond tick ($O(N)$ overhead), tasks are stored in a sorted delta list where `wake_tick = current_tick + delay_ticks`. On each tick interrupt, the scheduler only checks the head of the list. If the head's `wake_tick` has not arrived, the tick handler exits in $O(1)$ time. When the tick is reached, only the expired tasks at the front of the list are popped and moved to the ready queue."*

---

### Q5: How does this project compare to FreeRTOS?
**Answer**:
> *"The API and architectural model are directly compatible with FreeRTOS concepts:*
> - `rtos_task_create` corresponds to `xTaskCreate`
> - `rtos_task_delay_until` corresponds to `vTaskDelayUntil`
> - `rtos_queue_send` / `receive` corresponds to `xQueueSend` / `xQueueReceive`
> - `rtos_mutex_lock` / `unlock` corresponds to `xSemaphoreTake` / `xSemaphoreGive`
> 
> *Building it from scratch gave me a complete, white-box understanding of context switching, ready list manipulation, critical sections, and interrupt-driven preemption."*
