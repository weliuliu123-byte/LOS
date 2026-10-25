# Lab 27 — Sysfs `is_open` Attribute

## 1. Overview

This lab extends the baseline Linux character device driver (Lab 02) by introducing a read-only sysfs attribute that reports whether the device is currently open.

The implementation uses an active-open counter rather than a simple boolean, allowing the driver to correctly track multiple open file instances.

**Lab status:** PASS — Functional tests completed on QEMU ARMv7.

## 2. Learning Objectives

After completing this lab, I learned how to:

- Understand the relationship between `open()` and `release()` callbacks in Linux character device drivers.
- Distinguish cumulative open counts from currently active opens.
- Track device usage using an active-open counter.
- Protect shared kernel variables using a mutex.
- Expose device state through a read-only sysfs attribute.
- Handle multiple simultaneously opened file descriptors.
- Test device state transitions using BusyBox shell commands.
- Verify driver behavior using QEMU and kernel logs.

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
| Sysfs Attribute | `/sys/class/lab2_class/lab2/is_open` |

## 4. Implemented Features

### 4.1 Active Open Counter

Added a new kernel variable:

```c
static unsigned int active_open_count;
```

Two counters are maintained:

| Variable | Description |
|---|---|
| `open_count` | Total number of successful device opens |
| `active_open_count` | Number of currently active open file instances |

The counters are protected by `lab2_mutex`.

### 4.2 Modified `lab2_open()`

Each successful device open increments both counters:

```c
count = ++open_count;
active = ++active_open_count;
```

The updates occur while holding the mutex.

Example kernel log:

```text
lab2_driver: device opened (count=1)
lab2_driver: device opened (count=2)
```

### 4.3 Modified `lab2_release()`

When an open file instance is released, the active-open counter decreases:

```c
mutex_lock(&lab2_mutex);

if (active_open_count > 0)
    active_open_count--;

mutex_unlock(&lab2_mutex);
```

The cumulative `open_count` remains unchanged.

Example kernel logs:

```text
lab2_driver: device closed (Active=1)
lab2_driver: device closed (Active=0)
```

### 4.4 Read-Only Sysfs Attribute

Created the following attribute:

```text
/sys/class/lab2_class/lab2/is_open
```

The attribute returns:

- `0`: No active open file instances.
- `1`: At least one active open file instance.

The state is calculated using:

```c
status = (active_open_count > 0) ? 1 : 0;
```

The attribute is registered using:

```c
static DEVICE_ATTR_RO(is_open);
```

And added to the existing sysfs attribute group:

```c
&dev_attr_is_open.attr,
```

## 5. Implementation Logic

The device state is determined by:

```text
is_open = (active_open_count > 0)
```

### State Transitions

| Event | open_count | active_open_count | is_open |
|---|---:|---:|---:|
| Module loaded | 0 | 0 | 0 |
| Open FD 3 | 1 | 1 | 1 |
| Open FD 4 | 2 | 2 | 1 |
| Close FD 3 | 2 | 1 | 1 |
| Close FD 4 | 2 | 0 | 0 |

A counter is necessary because multiple processes or file descriptors may keep the device open simultaneously.

A simple boolean updated to `false` on every release would not correctly represent this situation.

## 6. Build Instructions

Navigate to the Lab 27 workspace:

```bash
cd ~/embedded_linux_driver_exercises/exercises/lab27_sysfs_is_open
```

Clean previous build outputs:

```bash
make clean
```

Cross-compile the kernel module:

```bash
make
```

Verify the resulting module:

```bash
file lab2_driver.ko
```

The module is built for the ARM architecture and targets Linux kernel 6.1.90.

## 7. QEMU Testing

The driver was tested using:

- QEMU ARM Cortex-A9 (`vexpress-a9`)
- Linux kernel 6.1.90
- BusyBox shell

### Test 1 — Initial Device State

Command:

```bash
cat /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
0
```

**Result: PASS**

The device initially has no active open file instances.

### Test 2 — Open the Device

Command:

```bash
exec 3<> /dev/lab2
```

Kernel log:

```text
lab2_driver: device opened (count=1)
```

Check device state:

```bash
cat /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
1
```

**Result: PASS**

The device is reported as open.

### Test 3 — Open a Second File Descriptor

Command:

```bash
exec 4<> /dev/lab2
```

Kernel log:

```text
lab2_driver: device opened (count=2)
```

Check the device state:

```bash
cat /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
1
```

Check cumulative statistics:

```bash
cat /proc/lab2_info
```

Relevant output:

```text
Open count    : 2
```

**Result: PASS**

The device remains open while two file descriptors are active.

### Test 4 — Close the First File Descriptor

Command:

```bash
exec 3>&-
```

Kernel log:

```text
lab2_driver: device closed (Active=1)
```

Check device state:

```bash
cat /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
1
```

**Result: PASS**

Closing one file descriptor does not incorrectly mark the device as closed while another remains active.

### Test 5 — Close the Remaining File Descriptor

Command:

```bash
exec 4>&-
```

Kernel log:

```text
lab2_driver: device closed (Active=0)
```

Check device state:

```bash
cat /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
0
```

**Result: PASS**

The device returns to the closed state after all open file instances are released.

### Test 6 — Read-Only Sysfs Protection

Command:

```bash
echo 1 > /sys/class/lab2_class/lab2/is_open
```

Actual output:

```text
-/bin/sh: can't create /sys/class/lab2_class/lab2/is_open: Permission denied
```

**Result: PASS**

The sysfs attribute cannot be modified through userspace writes.

## 8. Test Summary

| Test ID | Description | Result |
|---|---|---|
| T01 | Initial `is_open` equals 0 | PASS |
| T02 | First open changes `is_open` to 1 | PASS |
| T03 | Second open keeps `is_open` equal to 1 | PASS |
| T04 | Cumulative `open_count` reaches 2 | PASS |
| T05 | Closing one FD keeps `is_open` equal to 1 | PASS |
| T06 | Closing the final FD changes `is_open` to 0 | PASS |
| T07 | Writing to read-only sysfs is rejected | PASS |

**Overall result: PASS (7/7 functional tests).**

## 9. Key Takeaways

### 9.1 Cumulative vs Active Counters

`open_count` counts successful opens over the lifetime of the loaded driver.

`active_open_count` tracks how many open file instances remain active.

These counters serve different purposes and should not be used interchangeably.

### 9.2 Why a Boolean Is Not Enough

A boolean updated on each `open()` and `release()` is insufficient when multiple file instances can be opened simultaneously.

Using an active-open counter ensures the device remains logically open until the final file instance is released.

### 9.3 Mutex Synchronization

Shared counters are protected by a mutex to prevent lost updates and inconsistent state caused by concurrent access.

### 9.4 Sysfs for Device State

Sysfs provides a simple interface for exposing kernel device properties to userspace.

The `DEVICE_ATTR_RO()` macro creates a read-only device attribute with an associated show callback.

### 9.5 File Descriptor Lifetime

Linux calls the driver's `release()` callback when the final reference to an open file description is released.

Therefore, a reference-counting approach is more reliable than simply tracking individual `close()` calls.

## 10. Limitations

- The driver tracks active opens globally rather than maintaining separate state for each process.
- The tests verify two file descriptors opened sequentially from one BusyBox shell.
- Concurrent multi-process stress testing has not yet been performed.
- The lab reports device state but does not restrict the maximum number of simultaneous opens.

The maximum-open restriction will be introduced in Lab 28.

## 11. Conclusion

Lab 27 successfully implemented a read-only sysfs attribute for monitoring the current open state of a Linux character device.

The lab demonstrates:

- Linux character device lifecycle management.
- Active-open reference counting.
- Mutex-protected shared state.
- Sysfs attribute implementation.
- Verification using QEMU ARMv7.

All seven functional tests passed.

**Next Lab:** Lab 28 — Maximum Two Concurrent Opens.