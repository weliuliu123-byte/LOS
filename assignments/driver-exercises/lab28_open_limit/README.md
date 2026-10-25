# Lab 28 — Maximum Two Concurrent Opens

## 1. Overview

This lab extends the baseline Linux Character Device Driver (Lab 02) by limiting the number of simultaneously active device opens to two.

The driver uses a mutex-protected active-open counter to enforce the limit and returns `-EBUSY` when a third open is attempted.

**Status:** PASS — 7/7 functional tests on QEMU ARMv7.

## 2. Learning Objectives

After completing this lab, I learned how to:

- Implement resource limits in a Linux Character Device Driver.
- Understand the lifecycle of `open()` and `release()` callbacks.
- Distinguish cumulative successful opens from active opens.
- Track active device usage using a shared counter.
- Prevent race conditions using mutex synchronization.
- Return `-EBUSY` when a resource limit is reached.
- Maintain consistent counters after failed operations.
- Release and reuse available device-open slots.
- Verify driver behavior using QEMU, BusyBox, and procfs.

## 3. Development Environment

| Component | Configuration |
|---|---|
| Host OS | Ubuntu 24.04.5 |
| Target Architecture | ARMv7 |
| Emulated CPU | ARM Cortex-A9 |
| QEMU Machine | `vexpress-a9` |
| Linux Kernel | 6.1.90 |
| BusyBox | 1.35.0 |
| Cross-Compiler | `arm-linux-gnueabihf-gcc` |
| Device Node | `/dev/lab2` |
| Major Number | 240 |
| Procfs | `/proc/lab2_info` |

## 4. Implemented Features

### 4.1 Maximum Open Limit

The maximum number of active opens is defined as:

```c
#define MAX_ACTIVE_OPENS 2
```

The driver enforces:

```text
0 <= active_open_count <= 2
```

A third open request is rejected with `-EBUSY`.

### 4.2 Open Counters

The driver maintains two counters:

```c
static unsigned int open_count;
static unsigned int active_open_count;
```

| Counter | Description |
|---|---|
| `open_count` | Total number of successful opens |
| `active_open_count` | Number of currently active open file instances |

A failed open does not increment either counter.

### 4.3 Modified `lab2_open()`

The open callback checks the current number of active opens before accepting a new request.

Core logic:

```c
if (mutex_lock_interruptible(&lab2_mutex))
    return -ERESTARTSYS;

if (active_open_count >= MAX_ACTIVE_OPENS) {
    mutex_unlock(&lab2_mutex);

    pr_warn("lab2_driver: device busy, maximum opens reached\n");
    return -EBUSY;
}

open_count++;
active_open_count++;

mutex_unlock(&lab2_mutex);
```

The limit check and counter updates occur within the same critical section.

This prevents two concurrent callers from both passing the limit check using an outdated counter value.

### 4.4 Modified `lab2_release()`

When an open file instance is released, the active-open counter decreases.

```c
mutex_lock(&lab2_mutex);

if (active_open_count > 0)
    active_open_count--;

mutex_unlock(&lab2_mutex);
```

The cumulative successful-open counter remains unchanged.

### 4.5 Procfs Statistics

The driver exposes statistics through:

```text
/proc/lab2_info
```

Additional statistics include:

```text
Open count    : 2
Active open   : 2
Max open      : 2
```

These values help verify the open limit and active device usage.

## 5. Implementation Logic

```text
              USERSPACE
                  |
                  v
               open()
                  |
                  v
             lab2_open()
                  |
                  v
             LOCK MUTEX
                  |
                  v
       active_open_count >= 2?
              /       \
            YES        NO
             |          |
             v          v
        UNLOCK       open_count++
             |       active_open_count++
             v          |
          -EBUSY        v
                    UNLOCK MUTEX
                         |
                         v
                      SUCCESS
```

### State Transitions

| Event | open_count | active_open_count | Result |
|---|---:|---:|---|
| Module loaded | 0 | 0 | Ready |
| Open FD 3 | 1 | 1 | Success |
| Open FD 4 | 2 | 2 | Success |
| Open FD 5 | 2 | 2 | `-EBUSY` |
| Close FD 3 | 2 | 1 | Slot released |
| Open FD 5 | 3 | 2 | Success |
| Close FD 4 | 3 | 1 | Slot released |
| Close FD 5 | 3 | 0 | All closed |

## 6. Build Instructions

Navigate to the Lab 28 workspace:

```bash
cd ~/embedded_linux_driver_exercises/exercises/lab28_open_limit
```

Clean previous build outputs:

```bash
make clean
```

Cross-compile the ARM kernel module:

```bash
make
```

Verify the resulting module:

```bash
file lab2_driver.ko
```

Check kernel compatibility:

```bash
modinfo lab2_driver.ko | grep vermagic
```

The module targets Linux kernel 6.1.90 on ARMv7.

## 7. QEMU Testing

The driver was tested using:

- QEMU `vexpress-a9`
- ARM Cortex-A9
- Linux kernel 6.1.90
- BusyBox shell
- Character device major number 240

### Test 1 — First Open

Command:

```bash
exec 3<> /dev/lab2
```

Actual output:

```text
lab2_driver: device opened (total=1, active=1)
```

**Result: PASS**

The first open request was accepted.

### Test 2 — Second Open

Command:

```bash
exec 4<> /dev/lab2
```

Actual output:

```text
lab2_driver: device opened (total=2, active=2)
```

**Result: PASS**

The second open request was accepted.

### Test 3 — Third Open Rejected

Command:

```bash
exec 5<> /dev/lab2
```

Actual output:

```text
lab2_driver: device busy, maximum opens reached
-/bin/sh: can't create /dev/lab2: Device or resource busy
```

**Result: PASS**

The driver rejected the third open request with `-EBUSY`.

### Test 4 — Counters After Rejected Open

Command:

```bash
cat /proc/lab2_info
```

Actual output:

```text
LAB-02 Driver Statistics
======================
Driver name   : lab2
Major number  : 240
Buffer size   : 1024 bytes
Data length   : 0 bytes
Open count    : 2
Active open   : 2
Max open      : 2
```

**Result: PASS**

The failed open did not increment the counters.

### Test 5 — Release One Open Slot

Command:

```bash
exec 3>&-
```

Actual output:

```text
lab2_driver: device closed (active=1)
```

**Result: PASS**

The active-open counter decreased from two to one.

### Test 6 — Reopen After Releasing a Slot

Command:

```bash
exec 5<> /dev/lab2
```

Actual output:

```text
lab2_driver: device opened (total=3, active=2)
```

**Result: PASS**

The device accepted another open after a slot became available.

### Test 7 — Close All Remaining File Descriptors

Commands:

```bash
exec 4>&-
exec 5>&-
```

Actual output:

```text
lab2_driver: device closed (active=1)
lab2_driver: device closed (active=0)
```

Verify statistics:

```bash
cat /proc/lab2_info
```

Actual output:

```text
Open count    : 3
Active open   : 0
Max open      : 2
```

**Result: PASS**

All active opens were released.

## 8. Test Summary

| Test ID | Description | Result |
|---|---|---|
| T01 | Accept first open | PASS |
| T02 | Accept second open | PASS |
| T03 | Reject third open with `-EBUSY` | PASS |
| T04 | Preserve counters after rejected open | PASS |
| T05 | Decrement active counter after release | PASS |
| T06 | Allow reopening after a slot becomes free | PASS |
| T07 | Return active counter to zero | PASS |

**Overall Result: PASS (7/7 functional tests).**

## 9. Key Takeaways

### 9.1 Resource Limiting

The driver limits the number of simultaneous open file instances to prevent additional clients from acquiring access once the configured capacity is reached.

### 9.2 Mutex Synchronization

The limit check and counter increments must occur inside the same mutex-protected critical section.

Otherwise, concurrent processes could bypass the maximum-open limit.

### 9.3 Error Handling

The driver returns `-EBUSY` when no open slots remain.

Failed open requests do not affect successful-open statistics.

### 9.4 Active vs Cumulative Counters

`open_count` increases only after successful opens.

`active_open_count` increases on successful opens and decreases when open file instances are released.

### 9.5 Resource Reuse

Once a file instance is released, its slot becomes available for another open request.

This allows the driver to enforce a fixed concurrent-open limit without permanently blocking future clients.

## 10. Limitations

- The maximum-open limit is fixed at compile time.
- The driver tracks open file instances globally rather than imposing separate limits per process.
- The tests were performed sequentially using file descriptors in one BusyBox shell.
- Simultaneous multi-process stress testing has not yet been performed.
- The driver rejects excess opens immediately rather than waiting for an available slot.

## 11. Conclusion

Lab 28 successfully implemented a maximum-two-open restriction in a Linux Character Device Driver.

The lab demonstrates:

- Resource limiting.
- Active-open accounting.
- Mutex-protected critical sections.
- Proper handling of `-EBUSY`.
- Reuse of resources after release.
- Kernel driver verification using QEMU ARMv7.

All seven recorded functional tests passed.

**Next Lab:** Lab 29 — Line-by-Line Reading.