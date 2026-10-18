# LAB-02 — Linux Character Device Driver & MTD Storage Management

**Target Architecture:** ARMv7 Cortex-A9 (QEMU `vexpress-a9`)  
**Kernel:** Linux 6.1.90 LTS SMP PREEMPT  
**Driver Target:** `/dev/lab2` (Major 240, Minor 0)  
**Storage Target:** Simulated NAND Flash (`nandsim`) with JFFS2  
**Status:** **COMPLETED — Parts A, B, C, D Verified**

---

## 1. Laboratory Objectives

LAB-02 implements a production-style Linux character device driver, establishes observability channels via procfs and sysfs, exercises wear-leveling flash filesystem management with MTD/JFFS2, and integrates automated driver loading into BusyBox startup.

```text
+------------------------------------------------------------------------+
|                          LAB-02 SYSTEM STACK                           |
+------------------------------------------------------------------------+
|  USER SPACE ACCESS                                                     |
|  - Applications: cat, echo, test_driver.sh, test_procfs.sh             |
|  - Nodes: /dev/lab2, /proc/lab2_info, /sys/class/lab2_class/lab2/...   |
+------------------------------------------------------------------------+
|  VFS (VIRTUAL FILE SYSTEM) & CHARACTER DEVICE DISPATCH                 |
|  - sys_open -> lab2_open()       sys_release -> lab2_release()         |
|  - sys_read -> lab2_read()       sys_write   -> lab2_write()           |
+------------------------------------------------------------------------+
|  KERNEL DRIVER CORE (lab2_driver.ko)                                   |
|  - Shared Buffer: device_buffer[1024]                                  |
|  - Mutual Exclusion: lab2_mutex locking on state changes               |
|  - Safe Memory Exchange: copy_to_user() / copy_from_user()             |
+------------------------------------------------------------------------+
|  STORAGE SUBSYSTEM (MTD & JFFS2)                                       |
|  - Simulated NAND (nandsim 32MB) -> /dev/mtd2                          |
|  - Filesystem: JFFS2 with unmount/remount persistence validation       |
+------------------------------------------------------------------------+
```

---

## 2. Part A — Character Device Driver Architecture

### Device Specifications

| Parameter | Configuration Value |
|---|---|
| **Driver Module** | `lab2_driver.ko` |
| **Device File** | `/dev/lab2` |
| **Device Major / Minor** | `240` / `0` |
| **Driver Class** | `lab2_class` (registered under `/sys/class/lab2_class/`) |
| **Internal Data Buffer** | `1024` bytes static char array |
| **Concurrency Primitive** | `struct mutex lab2_mutex` |

### Core File Operations Workflow

1. **`lab2_open(struct inode *, struct file *)`**:
   Increments `open_count`, logs opening event to `dmesg`, returns 0.
2. **`lab2_write(struct file *, const char __user *, size_t, loff_t *)`**:
   Acquires `lab2_mutex`. Clamps incoming byte count to `BUFFER_SIZE - 1`. Safely copies payload from userspace using `copy_from_user()`. Null-terminates buffer, updates `buffer_len`, releases mutex, and returns bytes written.
3. **`lab2_read(struct file *, char __user *, size_t, loff_t *)`**:
   Checks file offset `*offset`. If `*offset >= buffer_len`, returns 0 (EOF). Calculates available bytes, safely transfers data to userspace using `copy_to_user()`, increments `*offset`, and returns bytes transferred.
4. **`lab2_release(struct inode *, struct file *)`**:
   Logs device closure to kernel ring buffer.

---

## 3. Part B — Kernel Telemetry (Procfs & Sysfs)

### Procfs Diagnostic Interface (`/proc/lab2_info`)

The driver implements `seq_file` operations (`single_open`, `seq_read`, `seq_printf`) to format dynamic driver statistics without buffer overflow hazards:

```text
Driver name   : lab2
Major number  : 240
Buffer size   : 1024 bytes
Data length   : 18 bytes
Open count    : 6
Last data     : [Hello from User]
```

### Sysfs Device Attributes (`/sys/class/lab2_class/lab2/`)

Attributes exposed using `DEVICE_ATTR_RO()`:
- `buffer_len`: Current length of valid string data inside `device_buffer`.
- `open_count`: Cumulative successful device opening count.
- `last_data`: The raw textual content of the latest write operation.

---

## 4. Part C — MTD Subsystem & JFFS2 Storage Persistence

Flash devices cannot use traditional block filesystems (like ext4) directly due to erase-before-write constraints and finite write endurance.

### Simulated NAND Configuration (Host Environment)

The `nandsim` kernel driver emulates a 32MB NAND flash chip:

```bash
# Load simulator with 32MB NAND profile
sudo modprobe nandsim first_id_byte=0x20 second_id_byte=0x75 third_id_byte=0x00 fourth_id_byte=0x00
```

Properties observed via `cat /proc/mtd`:
- Device Node: `/dev/mtd2` (NAND simulator partition)
- Total Size: `33,554,432` bytes (32 MiB)
- Erase Block Size: `16,384` bytes (16 KiB)
- Page Size: 512 bytes

### Formatting and Persistence Verification

```bash
# Format MTD partition with clean markers
flash_erase -j /dev/mtd2 0 0

# Mount JFFS2 partition
sudo mkdir -p /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock2 /mnt/jffs2

# Write test files
echo "Persistent Kernel Storage" > /mnt/jffs2/test.txt
sync

# Unmount and remount to verify non-volatility
sudo umount /mnt/jffs2
sudo mount -t jffs2 /dev/mtdblock2 /mnt/jffs2
cat /mnt/jffs2/test.txt
# Output: Persistent Kernel Storage
```

**Result: PASS.** Data remained fully intact across unmount/remount cycles.

---

## 5. Part D — BusyBox Init & Auto-Load Integration

In `/etc/init.d/rcS`:

```sh
# Automatic driver insertion at system boot
if [ -f /lib/modules/6.1.90/lab2_driver.ko ]; then
    insmod /lib/modules/6.1.90/lab2_driver.ko
    mknod /dev/lab2 c 240 0
    chmod 666 /dev/lab2
fi
```

Upon booting the QEMU target, `/dev/lab2` is automatically created, `lab2_driver.ko` is listed in `lsmod`, and the automated test script `/usr/bin/test_driver.sh` runs successfully.

---

## 6. Functional Verification Matrix

| Section | Scope | Verification Evidence | Status |
|---|---|---|:---:|
| **Part A** | Character device driver | Read/write strings `Message_1`..`Message_3` verified | **PASS** |
| **Part B** | Procfs & Sysfs telemetry | `/proc/lab2_info` and sysfs attributes return valid data | **PASS** |
| **Part C** | MTD & JFFS2 flash storage | Files survived unmount/remount on simulated NAND | **PASS** |
| **Part D** | Automated boot loading | Driver auto-loaded by `/etc/init.d/rcS` in QEMU | **PASS** |

---

## 7. Character Driver Extension Exercises (Labs 01–20)

In addition to the baseline driver above, this repository implements **20 independent driver extensions** located in [`assignments/driver-exercises/`](../../assignments/driver-exercises/):

| Lab | Directory | Feature Implemented | Verification |
|:---:|---|---|:---:|
| **01** | [`lab01_read_count`](../../assignments/driver-exercises/lab01_read_count/) | Read operation counter via sysfs | **PASS** |
| **02** | [`lab02_write_count`](../../assignments/driver-exercises/lab02_write_count/) | Write operation counter via procfs | **PASS** |
| **03** | [`lab03_clear_command`](../../assignments/driver-exercises/lab03_clear_command/) | Clear command via write callback | **PASS** |
| **04** | [`lab04_max_write_limit`](../../assignments/driver-exercises/lab04_max_write_limit/) | Maximum write size limit (128 bytes) | **PASS** |
| **05** | [`lab05_sysfs_buffer_size`](../../assignments/driver-exercises/lab05_sysfs_buffer_size/) | Static buffer capacity in sysfs | **PASS** |
| **06** | [`lab06_sysfs_status`](../../assignments/driver-exercises/lab06_sysfs_status/) | Dynamic status attribute (EMPTY / DATA) | **PASS** |
| **07** | [`lab07_total_bytes_read`](../../assignments/driver-exercises/lab07_total_bytes_read/) | Cumulative transferred read bytes | **PASS** |
| **08** | [`lab08_total_bytes_written`](../../assignments/driver-exercises/lab08_total_bytes_written/) | Cumulative transferred write bytes | **PASS** |
| **09** | [`lab09_write_timestamp`](../../assignments/driver-exercises/lab09_write_timestamp/) | Last write timestamp (jiffies) | **PASS** |
| **10** | [`lab10_sysfs_version`](../../assignments/driver-exercises/lab10_sysfs_version/) | Driver version attribute in sysfs | **PASS** |
| **11** | [`lab11_reset_stats`](../../assignments/driver-exercises/lab11_reset_stats/) | Operational statistics reset handler | **PASS** |
| **12** | [`lab12_atomic_open_count`](../../assignments/driver-exercises/lab12_atomic_open_count/) | Lock-free concurrency with `atomic_t` | **PASS** |
| **13** | [`lab13_mutex_read_protection`](../../assignments/driver-exercises/lab13_mutex_read_protection/) | Read mutual exclusion under `lab2_mutex` | **PASS** |
| **14** | [`lab14_buffer_append`](../../assignments/driver-exercises/lab14_buffer_append/) | Dynamic buffer append mode | **PASS** |
| **15** | [`lab15_clear_newline`](../../assignments/driver-exercises/lab15_clear_newline/) | Newline-tolerant command parsing | **PASS** |
| **16** | [`lab16_sysfs_max_size`](../../assignments/driver-exercises/lab16_sysfs_max_size/) | Configurable write threshold via sysfs | **PASS** |
| **17** | [`lab17_sysfs_last_write_size`](../../assignments/driver-exercises/lab17_sysfs_last_write_size/) | Last write transfer volume tracking | **PASS** |
| **18** | [`lab18_expanded_procfs`](../../assignments/driver-exercises/lab18_expanded_procfs/) | Multi-metric diagnostic dashboard | **PASS** |
| **19** | [`lab19_one_shot_read`](../../assignments/driver-exercises/lab19_one_shot_read/) | One-shot read with replay prevention | **PASS** |
| **20** | [`lab20_ioctl_control`](../../assignments/driver-exercises/lab20_ioctl_control/) | Unlocked IOCTL hardware control plane | **PASS** |
