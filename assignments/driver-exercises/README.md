# Character Device Driver Extension Exercises (Labs 01–20)

**Target Architecture:** ARMv7 Cortex-A9 (QEMU `vexpress-a9`)  
**Operating System Kernel:** Linux 6.1.90 LTS SMP PREEMPT  
**Verification Status:** **20/20 PASS — All 20 Functional Suites Verified**

This directory contains 20 independent character device driver modules extending the course's **LAB-02 baseline** (`/dev/lab2`). Each exercise focuses on a specific kernel subsystem capability: sysfs integration, procfs reporting, command parsing, bounds enforcement, concurrency primitives (mutex, atomic_t), dynamic buffering, and ioctl dispatch.

---

## Exercise Catalog

| Lab | Directory | Focus Area | Interface Modified | Difficulty | Status |
|---|---|---|---|:---:|:---:|
| **Lab 01** | [`lab01_read_count/`](lab01_read_count/) | Read Operation Counter | Sysfs (`read_count`) | ★★ | **PASS** |
| **Lab 02** | [`lab02_write_count/`](lab02_write_count/) | Write Operation Counter | Procfs (`/proc/lab2_info`) | ★★ | **PASS** |
| **Lab 03** | [`lab03_clear_command/`](lab03_clear_command/) | Device Buffer Clear Command | Write callback command parser | ★★ | **PASS** |
| **Lab 04** | [`lab04_max_write_limit/`](lab04_max_write_limit/) | Write Size Limit (128 Bytes) | Write bounds clamping | ★★ | **PASS** |
| **Lab 05** | [`lab05_sysfs_buffer_size/`](lab05_sysfs_buffer_size/) | Static Buffer Capacity | Sysfs (`buffer_size`) | ★★ | **PASS** |
| **Lab 06** | [`lab06_sysfs_status/`](lab06_sysfs_status/) | Device Status (EMPTY / DATA) | Sysfs (`status`) | ★★ | **PASS** |
| **Lab 07** | [`lab07_total_bytes_read/`](lab07_total_bytes_read/) | Cumulative Read Bytes | Sysfs (`total_bytes_read`) | ★★ | **PASS** |
| **Lab 08** | [`lab08_total_bytes_written/`](lab08_total_bytes_written/) | Cumulative Written Bytes | Sysfs (`total_bytes_written`) | ★★ | **PASS** |
| **Lab 09** | [`lab09_write_timestamp/`](lab09_write_timestamp/) | Last Write Jiffies Timestamp | Procfs (`/proc/lab2_info`) | ★★ | **PASS** |
| **Lab 10** | [`lab10_sysfs_version/`](lab10_sysfs_version/) | Driver Version String | Sysfs (`version`) | ★★ | **PASS** |
| **Lab 11** | [`lab11_reset_stats/`](lab11_reset_stats/) | Statistics Reset via Sysfs | Sysfs store (`reset_stats`) | ★★★ | **PASS** |
| **Lab 12** | [`lab12_atomic_open_count/`](lab12_atomic_open_count/) | Lock-Free Concurrency Control | Atomic variable (`atomic_t`) | ★★★ | **PASS** |
| **Lab 13** | [`lab13_mutex_read_protection/`](lab13_mutex_read_protection/) | Read Mutual Exclusion | Mutex lock in `read()` | ★★★ | **PASS** |
| **Lab 14** | [`lab14_buffer_append/`](lab14_buffer_append/) | Dynamic Buffer Concatenation | Write offset concatenation | ★★★ | **PASS** |
| **Lab 15** | [`lab15_clear_newline/`](lab15_clear_newline/) | Newline-Tolerant Clear Command | Command parser normalization | ★★★ | **PASS** |
| **Lab 16** | [`lab16_sysfs_max_size/`](lab16_sysfs_max_size/) | Configurable Write Max Size | Sysfs RW (`max_size`) | ★★★ | **PASS** |
| **Lab 17** | [`lab17_sysfs_last_write_size/`](lab17_sysfs_last_write_size/) | Last Write Byte Volume | Sysfs (`last_write_size`) | ★★ | **PASS** |
| **Lab 18** | [`lab18_expanded_procfs/`](lab18_expanded_procfs/) | Diagnostic Dashboard | Procfs multi-metric table | ★★★ | **PASS** |
| **Lab 19** | [`lab19_one_shot_read/`](lab19_one_shot_read/) | One-Shot Buffer Read | State flag consume-once | ★★★ | **PASS** |
| **Lab 20** | [`lab20_ioctl_control/`](lab20_ioctl_control/) | Unlocked IOCTL Interface | `GET_BUFFER_LEN`, `CLEAR_BUFFER` | ★★★+ | **PASS** |

---

## Build and Testing Workflow

### 1. Cross-Compilation

```bash
cd assignments/driver-exercises/<exercise_directory>
make clean
make
```

### 2. Deployment to QEMU Rootfs

```bash
cp lab2_driver.ko ~/embedded_lab2/rootfs/initramfs/lib/modules/6.1.90/
cd ~/embedded_lab2/rootfs/initramfs
find . | cpio -H newc -o | gzip -9 > ~/embedded_lab2/output/initramfs_lab2.cpio.gz
```

### 3. Execution Inside QEMU Target

```bash
# Insert module and instantiate device node
insmod /lib/modules/6.1.90/lab2_driver.ko
mknod /dev/lab2 c 240 0
chmod 666 /dev/lab2
```
