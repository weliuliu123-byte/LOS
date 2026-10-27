# Assignment 01 — Linux Kernel Device Drivers & IOCTL Interface

**Target Architecture:** ARMv7 Cortex-A9 (QEMU `vexpress-a9`)  
**Kernel:** Linux 6.1.90 LTS  
**Primary Devices:** `/dev/asgn1` (Major 241) & `/dev/counter` (Misc Major 10)  
**Status:** **COMPLETED — All Tasks 1–4 Verified**

---

## 1. Assignment Objectives

Assignment 01 requires designing, implementing, and verifying two distinct Linux device drivers on ARMv7:
1. **Task 1: Advanced Character Driver with IOCTL (`/dev/asgn1`)** — Custom control plane supporting buffer management, statistics retrieval, mode toggling, and version reporting.
2. **Task 2: Lock-Free Miscellaneous Device Driver (`/dev/counter`)** — Lightweight atomic counter driver utilizing kernel `atomic_t` primitives for thread safety.
3. **Task 3: Architectural Analysis & Technical Report** — Comparing character vs misc devices and memory access paradigms.
4. **Task 4: Theoretical Evaluation** — Kernel module reference counting and VFS read offset mechanics.

---

## 2. Task 1 — Character Device Driver with IOCTL (`asgn1_driver`)

### IOCTL Command ABI Specification (`asgn1_ioctl.h`)

IOCTL commands are defined using Linux standard macro encoding (`_IO`, `_IOR`, `_IOW`):

```c
#define ASGN1_IOC_MAGIC   'a'

/* Command definitions */
#define ASGN1_RESET_BUFFER   _IO(ASGN1_IOC_MAGIC, 0)
#define ASGN1_GET_STATS      _IOR(ASGN1_IOC_MAGIC, 1, struct asgn1_stats)
#define ASGN1_SET_MODE       _IOW(ASGN1_IOC_MAGIC, 2, int)
#define ASGN1_GET_VERSION    _IOR(ASGN1_IOC_MAGIC, 3, char[32])
```

### Driver Features & Operational Modes

- **Buffer Size:** 1024 bytes (`ASGN1_BUFFER_SIZE`).
- **Echo Mode (`ASGN1_MODE_ECHO`):** Standard pass-through data buffering.
- **Kernel-Prefix Mode (`ASGN1_MODE_PREFIX`):** Automatically prefixes any written string with `[KERNEL] ` when read back.
- **Telemetry Struct (`struct asgn1_stats`):**
  - `read_ops`: Cumulative count of read operations.
  - `write_ops`: Cumulative count of write operations.
  - `bytes_read`: Total byte volume transferred to userspace.
  - `bytes_written`: Total byte volume received from userspace.
- **Concurrency:** All shared state mutations are protected by `asgn1_mutex`.

---

## 3. Task 2 — Misc Device Driver with Atomic Counter (`counter_driver`)

### Implementation Highlights

- Registered using `misc_register()` under `MISC_DYNAMIC_MINOR` (Major 10).
- Internal counter maintained as `static atomic_t counter_val = ATOMIC_INIT(0)`.
- **Read Path (`counter_read`):**
  - Reads counter value via `atomic_read()`.
  - Atomically increments counter using `atomic_inc()`.
  - Formats string and transfers to userspace via `copy_to_user()`.
- **Write Path (`counter_write`):**
  - Parses positive integer from user string.
  - Sets new baseline value atomically using `atomic_set()`.
- **Procfs Integration (`/proc/counter_info`):**
  - Reports current counter value and device status.

---

## 4. Verification & Test Suite

The automated test suite (`test_asgn1.c`) exercises 7 comprehensive verification checkpoints:

```text
=== ASSIGNMENT 01 AUTOMATED TEST SUITE ===
[Test 1] Device open /dev/asgn1: SUCCESS
[Test 2] Query driver version via IOCTL: asgn1_driver v1_0 (PASS)
[Test 3] Write and read in Echo Mode: Data matched perfectly (PASS)
[Test 4] Switch to Kernel-Prefix Mode via IOCTL: Received '[KERNEL] TestPayload' (PASS)
[Test 5] Query driver statistics struct: read_ops=2, write_ops=2 (PASS)
[Test 6] Reset buffer and counters via ASGN1_RESET_BUFFER: Reset confirmed (PASS)
[Test 7] Invalid IOCTL command rejection: Returned -ENOTTY as expected (PASS)
=== ALL 7 FUNCTIONAL TESTS PASSED ===
```

---

## 5. Technical Questions & Analysis

### Q1: Module Reference Counting
- **Mechanism:** Handled automatically by the VFS via `try_module_get(THIS_MODULE)` on `open()` and `module_put(THIS_MODULE)` on `release()`.
- **Purpose:** Prevents a kernel module from being unloaded (`rmmod`) while active file descriptors are held open by userspace processes, preventing kernel oops.

### Q2: User/Kernel Memory Boundaries (`copy_to_user`)
- Kernel space and user space reside in distinct virtual address spaces.
- Direct pointer dereferences fail if user pages are swapped out, invalid, or malicious.
- `copy_to_user()` and `copy_from_user()` verify address validity via `access_ok()`, handle page faults gracefully, and return `-EFAULT` without crashing the kernel.
