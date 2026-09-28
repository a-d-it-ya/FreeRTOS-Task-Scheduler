# ⚙️ Scheduler Design, Synchronization & PIP Protocol

This document explains the algorithmic details behind the scheduler dispatching engine, preemption mechanics, and the **Priority Inheritance Protocol (PIP)**.

---

## 1. The Priority Inversion Problem

Priority inversion occurs when a low-priority task holds a shared resource (such as a mutex guarding a hardware sensor bus) needed by a high-priority task, and an intermediate medium-priority task preempts the low-priority task because the medium task does not need the resource.

### Failure Timeline (Unbounded Inversion):
1. **$T_{Low}$ (Priority 2)** acquires `Mutex_Bus`.
2. **$T_{High}$ (Priority 10)** wakes up, requests `Mutex_Bus`, and enters `BLOCKED` state.
3. **$T_{Med}$ (Priority 5)** wakes up. Since Priority 5 > Priority 2, $T_{Med}$ preempts $T_{Low}$.
4. $T_{Med}$ runs indefinitely, starving $T_{Low}$.
5. Consequently, $T_{High}$ is indirectly blocked by $T_{Med}$ (which has lower priority than $T_{High}$!).

---

## 2. Priority Inheritance Protocol (PIP) Algorithm

The micro-kernel implements dynamic Priority Inheritance:

```
                          +-------------------+
                          | High Prio Task    |
                          | (Priority 10)     |
                          +-------------------+
                                    |
                          Requests Mutex (Blocked)
                                    |
                                    v
                          +-------------------+
                          | Low Prio Task     |
                          | Base: 2 -> Eff: 10| <--- [PIP Elevation]
                          +-------------------+
                                    |
                     Now executes at Priority 10!
               (Cannot be preempted by Medium Priority 5)
                                    |
                             Unlocks Mutex
                                    |
                                    v
                     +-----------------------------+
                     | Base: 2 <- Eff: 2 (Restore) |
                     | High Prio acquires mutex    |
                     +-----------------------------+
```

### Mutex Lock Operation:
```c
if (mutex->owner != NULL) {
    task->state = RTOS_TASK_STATE_BLOCKED;
    wait_list_insert_prio(&mutex->wait_list_head, task);
    
    // Elevate owner priority to match highest waiter
    if (mutex->owner->effective_priority < task->effective_priority) {
        recalculate_owner_priority(mutex->owner);
    }
}
```

### Mutex Unlock Operation:
1. When $T_{Low}$ releases the mutex, it is removed from $T_{Low}$'s `held_mutexes` list.
2. If $T_{Low}$ holds other mutexes, its effective priority becomes $\max(\text{base\_priority}, \max(\text{waiters on remaining mutexes}))$.
3. If no other mutexes are held, effective priority reverts to `base_priority`.
4. Ownership is transferred directly to the highest-priority task on the mutex's wait list.

---

## 3. Delta-Queue Delay List

Rather than iterating through all tasks on every timer tick, sleeping tasks are stored in an ascending sorted list based on absolute tick wake time (`wake_tick`).

- **Tick Inspection**: The kernel examines only `delay_list_head`. If `delay_list_head->wake_tick > current_tick`, it immediately returns ($O(1)$ fast path).
- **Wakeup**: Expired tasks are popped and moved to the ready queue in $O(1)$ operations.
