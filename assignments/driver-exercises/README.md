# Character Device Driver Extension Exercises (Labs 01–20)

**Target Hardware:** ARMv7 Cortex-A9 (QEMU `vexpress-a9`)  
**Target Operating System:** Linux Kernel 6.1.90 LTS SMP PREEMPT  
**Base Character Driver:** Course LAB-02 Baseline (`/dev/lab2`, Major 240, Minor 0)  
**Suite Status:** **20/20 PASS — All Functional Suites Verified**

---

## 1. Scope & Architectural Purpose

This directory contains **20 independent hands-on driver modules** developed to systematically explore the Linux Device Model, Virtual File System (VFS) integration, kernel-to-userspace memory safety, lock-free concurrency, and hardware control planes.

Each exercise starts from the clean **LAB-02 character driver baseline** and implements a focused architectural enhancement.

```text
+-------------------------------------------------------------------------+
|                  20 DRIVER EXTENSION EXERCISES TAXONOMY                 |
+-------------------------------------------------------------------------+
|  GROUP A: SYSFS & PROCFS TELEMETRY (Labs 01 - 10)                       |
|  - Lab 01: Read counter sysfs attribute (DEVICE_ATTR_RO)                |
|  - Lab 02: Write counter in procfs (seq_file interface)                 |
|  - Lab 03: In-band string command parsing (clear command)               |
|  - Lab 04: Write payload boundary clamping (128 bytes limit)            |
|  - Lab 05: Buffer capacity exposure via sysfs                           |
|  - Lab 06: Categorical device state tracking (EMPTY / DATA)             |
|  - Lab 07: Cumulative read byte accounting                              |
|  - Lab 08: Cumulative write byte accounting                             |
|  - Lab 09: System uptime timestamping via kernel jiffies                |
|  - Lab 10: Static driver release versioning in sysfs                    |
+-------------------------------------------------------------------------+
|  GROUP B: CONCURRENCY, ATOMICITY & BUFFER CONTROL (Labs 11 - 19)        |
|  - Lab 11: Write-only sysfs store callback for statistics reset         |
|  - Lab 12: Lock-free multi-thread safety using atomic_t                 |
|  - Lab 13: Read callback mutual exclusion under lab2_mutex              |
|  - Lab 14: Dynamic buffer append and tail concatenation                 |
|  - Lab 15: Delimiter-normalized command parser (clear\n support)        |
|  - Lab 16: Read-write tunable write threshold (DEVICE_ATTR_RW)          |
|  - Lab 17: Last write operation byte volume tracking                    |
|  - Lab 18: Multi-metric consolidated procfs diagnostic dashboard        |
|  - Lab 19: One-shot read semantics with replay protection               |
+-------------------------------------------------------------------------+
|  GROUP C: HARDWARE CONTROL PLANE VIA IOCTL (Lab 20)                     |
|  - Lab 20: Unlocked IOCTL dispatch (GET_BUFFER_LEN & CLEAR_BUFFER)      |
+-------------------------------------------------------------------------+
```

---

## 2. Complete Exercise Master Catalog

| Lab | Module Directory | Core Technical Feature | Interface Modified | Difficulty | Status |
|:---:|---|---|---|:---:|:---:|
| **01** | [`lab01_read_count/`](lab01_read_count/) | Read Operation Counter | Sysfs (`read_count`) | ★★ | **PASS** |
| **02** | [`lab02_write_count/`](lab02_write_count/) | Write Operation Counter | Procfs (`/proc/lab2_info`) | ★★ | **PASS** |
| **03** | [`lab03_clear_command/`](lab03_clear_command/) | Clear Command via `write()` | Write callback command parser | ★★ | **PASS** |
| **04** | [`lab04_max_write_limit/`](lab04_max_write_limit/) | Write Size Limit (128 Bytes) | Write bounds clamping | ★★ | **PASS** |
| **05** | [`lab05_sysfs_buffer_size/`](lab05_sysfs_buffer_size/) | Static Buffer Capacity | Sysfs (`buffer_size`) | ★★ | **PASS** |
| **06** | [`lab06_sysfs_status/`](lab06_sysfs_status/) | Device Status (EMPTY / DATA) | Sysfs (`status`) | ★★ | **PASS** |
| **07** | [`lab07_total_bytes_read/`](lab07_total_bytes_read/) | Cumulative Read Bytes | Sysfs (`total_bytes_read`) | ★★ | **PASS** |
| **08** | [`lab08_total_bytes_written/`](lab08_total_bytes_written/) | Cumulative Written Bytes | Sysfs (`total_bytes_written`) | ★★ | **PASS** |
| **09** | [`lab09_write_timestamp/`](lab09_write_timestamp/) | Last Write Jiffies Timestamp | Procfs (`/proc/lab2_info`) | ★★ | **PASS** |
| **10** | [`lab10_sysfs_version/`](lab10_sysfs_version/) | Driver Version String | Sysfs (`version`) | ★★ | **PASS** |
| **11** | [`lab11_reset_stats/`](lab11_reset_stats/) | Statistics Reset via Sysfs | Sysfs store (`reset_stats`) | ★★★ | **PASS** |
| **12** | [`lab12_atomic_open_count/`](lab12_atomic_open_count/) | Lock-Free Concurrency Control | Atomic variable (`atomic_t`) | ★★★ | **PASS** |
| **13** | [`lab13_mutex_read_protection/`](lab13_mutex_read_protection/) | Read Mutual Exclusion | Mutex lock in `read()` | ★★★ | **PASS** |
| **14** | [`lab14_buffer_append/`](lab14_buffer_append/) | Dynamic Buffer Concatenation | Write offset concatenation | ★★★ | **PASS** |
| **15** | [`lab15_clear_newline/`](lab15_clear_newline/) | Newline-Tolerant Clear Command | Command parser normalization | ★★★ | **PASS** |
| **16** | [`lab16_sysfs_max_size/`](lab16_sysfs_max_size/) | Configurable Write Max Size | Sysfs RW (`max_size`) | ★★★ | **PASS** |
| **17** | [`lab17_sysfs_last_write_size/`](lab17_sysfs_last_write_size/) | Last Write Byte Volume | Sysfs (`last_write_size`) | ★★ | **PASS** |
| **18** | [`lab18_expanded_procfs/`](lab18_expanded_procfs/) | Diagnostic Dashboard | Procfs multi-metric table | ★★★ | **PASS** |
| **19** | [`lab19_one_shot_read/`](lab19_one_shot_read/) | One-Shot Buffer Read | State flag consume-once | ★★★ | **PASS** |
| **20** | [`lab20_ioctl_control/`](lab20_ioctl_control/) | Unlocked IOCTL Interface | `GET_BUFFER_LEN`, `CLEAR_BUFFER` | ★★★+ | **PASS** |

---

## 3. Standard Verification & Compilation Guide

Each exercise directory contains a standalone `lab2_driver.c` and a Kbuild `Makefile`.

### Compilation

```bash
cd assignments/driver-exercises/<exercise_folder>
make clean
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
```

### Module Verification

```bash
file lab2_driver.ko
# Verify vermagic matches kernel target:
modinfo lab2_driver.ko | grep vermagic
# Output: 6.1.90 SMP mod_unload ARMv7 p2v8
```

### Testing on Target

```sh
# Inside QEMU ARM target:
insmod lab2_driver.ko
mknod /dev/lab2 c 240 0 2>/dev/null || true
chmod 666 /dev/lab2
```
