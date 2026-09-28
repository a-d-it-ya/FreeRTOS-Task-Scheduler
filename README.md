# 🚀 FreeRTOS Task Scheduler & Micro-Kernel

<div align="center">

[![C11](https://img.shields.io/badge/Language-C11-00599C.svg?style=for-the-badge&logo=c)](https://en.cppreference.com/w/c/11)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20ARM%20Cortex-brightgreen.svg?style=for-the-badge)](https://github.com/a-d-it-ya/FreeRTOS-Task-Scheduler)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)](LICENSE)
[![Build Status](https://img.shields.io/badge/Build-Passing-success.svg?style=for-the-badge)]()

**A deterministic, production-grade Real-Time Operating System (RTOS) Task Scheduler and Micro-Kernel in C11.**  
*Featuring Cooperative & Preemptive Scheduling, $O(1)$ Multi-Priority Ready Queues, Priority Inheritance Protocol (PIP), Thread-Safe Queues, Delta-Queue Delays, and Live Telemetry Observability.*

[Architecture](#-architectural-overview) • [Key Features](#-key-features) • [FreeRTOS Comparison](#-freertos-api-parity) • [Flight Simulator Demo](#-aerospace-flight-computer-simulation) • [Getting Started](#-getting-started) • [Documentation](#-documentation)

</div>

---

## 📌 Overview

**FreeRTOS Task Scheduler** is an embedded real-time micro-kernel and priority-based task dispatcher engineered from the ground up to model mission-critical RTOS kernel primitives. Designed with aerospace flight computer deterministic timing constraints in mind, it provides robust multitasking, synchronization primitives, and observability tooling.

```
+---------------------------------------------------------------------------------+
|                              Application Layer                                  |
|  - Avionics Flight Simulator   - Multi-Rate Sensors   - Producer/Consumer IPC   |
+---------------------------------------------------------------------------------+
                                      │
+---------------------------------------------------------------------------------+
|                       RTOS API & Synchronization Layer                          |
|  - Task Control (Create, Delay, Suspend, Resume, PrioritySet)                  |
|  - Mutex with Priority Inheritance Protocol (PIP)                               |
|  - Counting / Binary Semaphores & Circular Message Queues                       |
|  - Software Timers (One-Shot & Periodic Auto-Reload)                            |
+---------------------------------------------------------------------------------+
                                      │
+---------------------------------------------------------------------------------+
|                             Kernel Core Engine                                  |
|  - 32-Level Ready Queue (Bitmapped Priority Array, O(1) Scheduling)             |
|  - Preemptive / Cooperative Dispatcher & Delta-Sorted Delay List                |
|  - TCB Management, Stack Canary Overflow Checks (0xDEADBEEF), CPU Profiler     |
+---------------------------------------------------------------------------------+
                                      │
+---------------------------------------------------------------------------------+
|                         Platform Porting Layer (HAL)                            |
|  - Win32 Fibers / POSIX ucontext Stack Frame Switcher                           |
|  - High-Resolution Periodic Hardware Timer Interrupt Emulator                  |
+---------------------------------------------------------------------------------+
```

---

## ✨ Key Features

- ⚡ **$O(1)$ Multi-Priority Ready Queue**:
  - Supports 32 priority levels (`0` lowest/idle, `31` highest).
  - Bitmapped lookup using hardware fast bit-scan (`__builtin_clz` / `_BitScanReverse`) for deterministic constant-time task selection.
  - Round-robin time-slicing among equal-priority tasks.

- 🔄 **Dual Scheduling Modes (Cooperative & Preemptive)**:
  - **Cooperative**: Voluntary yielding via `rtos_task_yield()`.
  - **Preemptive**: Hardware timer tick interrupt drives immediate preemption whenever a higher-priority task wakes up.
  - Deterministic periodic execution using `rtos_task_delay_until()`.

- 🛡️ **Priority Inheritance Protocol (PIP)**:
  - Eliminates the classic unbounded priority inversion problem (Mars Pathfinder anomaly).
  - Temporarily boosts the priority of a low-priority mutex owner to match that of the highest-priority blocked waiting task, preventing intermediate-priority tasks from starving critical operations.

- 📬 **Thread-Safe Circular FIFO Message Queues**:
  - Blocking push/pop operations with configurable timeout semantics (`RTOS_WAIT_FOREVER`, `RTOS_NO_WAIT`, or tick timeouts).
  - Zero-copy and value-copy memory transfers.

- 🔍 **Real-Time Diagnostics & Observability**:
  - Live ASCII telemetry dashboard displaying task states, switch counts, execution times, and CPU utilization percentage.
  - Stack watermark monitor verifying `0xDEADBEEF` canaries for stack overflow protection.

---

## 🔄 FreeRTOS API Parity

This kernel's interface is designed to map directly to standard FreeRTOS APIs:

| FreeRTOS Standard API | Micro-RTOS Equivalent | Description |
| :--- | :--- | :--- |
| `xTaskCreate()` | `rtos_task_create()` | Creates and registers a new task |
| `vTaskDelay()` | `rtos_task_delay_ms()` | Blocks task for relative time duration |
| `vTaskDelayUntil()` | `rtos_task_delay_until()` | Periodic deterministic task delays |
| `taskYIELD()` | `rtos_task_yield()` | Voluntarily yields CPU to ready tasks |
| `vTaskSuspend()` / `vTaskResume()` | `rtos_task_suspend()` / `rtos_task_resume()` | Suspends or resumes task execution |
| `vTaskPrioritySet()` | `rtos_task_set_priority()` | Dynamically alters task nominal priority |
| `xSemaphoreCreateMutex()` | `rtos_mutex_create()` | Creates recursive mutex with PIP |
| `xSemaphoreTake()` / `xSemaphoreGive()` | `rtos_mutex_lock()` / `rtos_mutex_unlock()` | Mutex lock/unlock with PIP elevation |
| `xQueueCreate()` | `rtos_queue_create()` | Allocates circular message queue |
| `xQueueSend()` / `xQueueReceive()` | `rtos_queue_send()` / `rtos_queue_receive()` | Blocking FIFO queue enqueue/dequeue |

---

## 🛩️ Aerospace Flight Computer Simulation

Included in `examples/05_flight_telemetry_sim.c` is a multi-rate avionics task pipeline directly modeled after aerospace flight computers:

```
+---------------------+   100 Hz (10ms)   +----------------------+
|  IMU Fast Sensor    | ────────────────> |                      |
+---------------------+                   |                      |
                                          |                      |
+---------------------+   50 Hz (20ms)    |                      |   2 Hz (500ms)   +-------------------+
|  Flight Guidance    | ────────────────> | Flight Telemetry Bus | ---------------> | Radio Downlink TX |
+---------------------+   (State Mutex)   |                      |                  +-------------------+
                                          |                      |
+---------------------+   10 Hz (100ms)   |                      |
|  GPS Navigation     | ────────────────> |                      |
+---------------------+                   +----------------------+
```

### Live Diagnostic Telemetry Output:
```
+---------------------------------------------------------------------------------------------------------+
|                                  RTOS TASK SCHEDULER DIAGNOSTIC MONITOR                                 |
+----+----------------------+----------+------+------+------------+------------------+----------+---------+
| ID | Task Name            | State    | Base | Eff  | Sw Counter | Runtime (us)     | CPU (%)  | FreeStk |
+----+----------------------+----------+------+------+------------+------------------+----------+---------+
| 1  | IDLE_TASK            | READY    | 0    | 0    | 124        | 1240000          |  49.60%  | 31800   |
| 2  | IMU_Acq_100Hz        | BLOCKED  | 12   | 12   | 250        | 312500           |  12.50%  | 63248   |
| 3  | Control_50Hz         | BLOCKED  | 10   | 10   | 125        | 450000           |  18.00%  | 63120   |
| 4  | GPS_Nav_10Hz         | BLOCKED  | 6    | 6    | 25         | 187500           |   7.50%  | 63380   |
| 5  | Telemetry_Radio_2Hz  | BLOCKED  | 3    | 3    | 5          | 125000           |   5.00%  | 63400   |
| 6  | FlightSupervisor     | RUNNING  | 1    | 1    | 18         | 185000           |   7.40%  | 63012   |
+----+----------------------+----------+------+------+------------+------------------+----------+---------+
 Kernel Ticks: 2500     | Total CPU Measured: 2500000 us | System Tasks: 6
```

---

## 🚀 Getting Started

### Prerequisites
- **Compiler**: GCC, Clang, or MSVC supporting C11.
- **Build Tools**: CMake (>= 3.15) or GNU Make.
- **Platform**: Windows (Win32 Fibers) or Linux/macOS (POSIX `ucontext`).

### Cloning the Repository
```bash
git clone https://github.com/a-d-it-ya/FreeRTOS-Task-Scheduler.git
cd FreeRTOS-Task-Scheduler
```

### Building with CMake
```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Building with Make
```bash
make all
```

### Running Demonstrations & Tests
```bash
# 1. Cooperative Multitasking Demo
./bin/01_cooperative

# 2. Preemptive Priority Preemption Demo
./bin/02_preemptive

# 3. Priority Inheritance Protocol (PIP) Demo
./bin/03_pip

# 4. Producer-Consumer Message Queue Demo
./bin/04_queue

# 5. Aerospace Flight Computer Telemetry Simulator
./bin/05_flight_sim

# 6. Automated Invariant & Unit Test Suite
./bin/test_suite
```

---

## 📂 Repository Structure

```
FreeRTOS-Task-Scheduler/
├── CMakeLists.txt              # CMake build configuration
├── Makefile                    # Make build configuration
├── README.md                   # Main project documentation
├── LICENSE                     # MIT License
├── .gitignore                  # Git ignore rules
├── docs/
│   ├── ARCHITECTURE.md         # Micro-kernel architecture and TCB layouts
│   ├── SCHEDULER_DESIGN.md     # Priority Inheritance, delta-queues, O(1) bitmap
│   └── INTERVIEW_GUIDE.md      # Resume pitch & technical interview Q&A
├── include/
│   ├── rtos_config.h           # Compile-time kernel tuning
│   ├── rtos_types.h            # Status codes, handles, task states
│   ├── rtos_kernel.h           # Scheduler engine, tick ISR, lifecycle
│   ├── rtos_task.h             # TCB management, stack canaries, yielding, delays
│   ├── rtos_sync.h             # Mutex with Priority Inheritance & Semaphores
│   ├── rtos_queue.h            # Thread-safe circular FIFO message queues
│   ├── rtos_timer.h            # One-shot and auto-reload software timers
│   ├── rtos_trace.h            # Real-time event tracing & CPU runtime profiler
│   └── rtos_port.h             # Machine-dependent context & tick abstraction
├── src/
│   ├── kernel/
│   │   ├── rtos_kernel.c       # Ready queue dispatcher & scheduler engine
│   │   ├── rtos_task.c         # TCB lifecycle, priority, stack watermarks
│   │   ├── rtos_sync.c         # Mutex (PIP) & Counting Semaphores
│   │   ├── rtos_queue.c        # Blocking circular message queue
│   │   ├── rtos_timer.c        # Software timer delta-wheel
│   │   └── rtos_trace.c        # ASCII visualizer & telemetry logger
│   └── port/
│       ├── port_win32.c        # Win32 Fibers & multimedia timer port
│       └── port_posix.c        # POSIX ucontext & pthread timer port
├── examples/
│   ├── 01_cooperative_demo.c   # Voluntary round-robin yield demo
│   ├── 02_preemptive_priority_demo.c # Preemptive priority interruption
│   ├── 03_priority_inversion_pip_demo.c # Mars Pathfinder PIP scenario
│   ├── 04_producer_consumer_queue_demo.c # Multi-sensor queue pipeline
│   └── 05_flight_telemetry_sim.c # Aerospace Flight Computer Multi-Rate Simulation
└── tests/
    └── test_suite.c            # Automated unit test runner
```

---

## 📖 Documentation

- **[Architecture Specification](docs/ARCHITECTURE.md)**: Deep dive into kernel internals, TCB data structures, and memory layouts.
- **[Scheduler Design](docs/SCHEDULER_DESIGN.md)**: Algorithmic analysis of $O(1)$ ready queue bitmapping, delta-delay lists, and the Priority Inheritance Protocol.
- **[Interview Preparation Guide](docs/INTERVIEW_GUIDE.md)**: Senior embedded systems interview questions, FreeRTOS architectural comparisons, and resume talking points.

---

## 👤 Author

**Aditya Singh**
- GitHub: [@a-d-it-ya](https://github.com/a-d-it-ya)
- Email: aditya.singh.bu1902@gmail.com

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
