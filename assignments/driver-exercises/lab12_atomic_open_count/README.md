# LAB 12 — Lock-Free Concurrency Control via atomic_t

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 3/3 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 12 of the 20 Extension Exercises sequence** (`lab12_atomic_open_count`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Migrate open_count from plain integer to atomic_t for lockless multi-process safety.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Atomic type declaration | Define static atomic_t open_count = ATOMIC_INIT(0). |
| FR-02 | Atomic increment | Replace open_count++ with atomic_inc(&open_count) in lab2_open. |
| FR-03 | Atomic read | Read counter using atomic_read(&open_count) in sysfs and procfs. |
| FR-04 | Lock-free open path | Eliminate mutex contention during open() system call dispatch. |
| FR-05 | SMP verification | Verify thread-safe increments without memory tearing on ARM Cortex-A9. |

## 2. Baseline analysis and design reasoning

On multicore ARM systems (ARMv7 Cortex-A9 MPCore), atomic_t utilizes load-linked/store-conditional (LDREX/STREX) instructions to ensure atomic operations without the overhead of heavy sleeping mutexes.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab12_atomic_open_count/lab2_driver.c`:

```c
#include <linux/atomic.h> /* [EXERCISE 12 - ADDED]: Thư viện cung cấp kiểu dữ liệu nguyên tử atomic_t */
/* [EXERCISE 12 - REPLACED]: Thay "static int open_count = 0;" bằng kiểu nguyên tử atomic_t để chống race condition */
static atomic_t open_count = ATOMIC_INIT(0);
    /* [EXERCISE 12 - REPLACED]: Thay "open_count++;" bằng lệnh tăng nguyên tử ở mức phần cứng CPU */
    current_opens = atomic_inc_return(&open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    /* [EXERCISE 12 - REPLACED]: Dùng atomic_read() để đọc giá trị an toàn từ biến atomic_t */
    return sprintf(buf, "%d\n", atomic_read(&open_count));
static DEVICE_ATTR_RO(open_count);
    /* [EXERCISE 12 - REPLACED]: Dùng atomic_read() khi xuất ra /proc/lab2_info */
    seq_printf(m, "Open Count  : %d\n", atomic_read(&open_count));
    device_create_file(lab2_device, &dev_attr_open_count);
    device_remove_file(lab2_device, &dev_attr_open_count);
```

## 4. Environment and filesystem paths

| Component | Target path |
|---|---|
| Character device | `/dev/lab2` |
| Major / Minor | `240:0` |
| Procfs entry | `/proc/lab2_info` |
| Sysfs directory | `/sys/class/lab2_class/lab2/` |
| Kernel module | `/lib/modules/6.1.90/lab2_driver.ko` |

## 5. Build the kernel module (Ubuntu host)

```bash
cd assignments/driver-exercises/lab12_atomic_open_count
make clean
make
```

Verify binary attributes and cross-compilation target:

```bash
file lab2_driver.ko
modinfo lab2_driver.ko | grep vermagic
```

Expected vermagic: `6.1.90 SMP mod_unload ARMv7 p2v8`

## 6. Boot QEMU (Ubuntu host)

```bash
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

Inside QEMU shell:

```sh
# Create node if not created by devtmpfs
mknod /dev/lab2 c 240 0 2>/dev/null || true
chmod 666 /dev/lab2
```

## 7. Functional testing — observed QEMU transcript interpretation

### T01 - Initial count

```sh
cat /sys/class/lab2_class/lab2/open_count
```

**Observed behavior:**

```text
1 (cat open)
```


### T02 - Concurrent open stress

```sh
for i in $(seq 1 10); do cat /dev/lab2 > /dev/null & done; wait
```

**Observed behavior:**

```text
All 10 opens recorded without loss
```


### T03 - Total count

```sh
cat /sys/class/lab2_class/lab2/open_count
```

**Observed behavior:**

```text
12
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initial count | Expected response | 1 (cat open) | **PASS** |
| T02 | Concurrent open stress | Expected response | All 10 opens recorded without loss | **PASS** |
| T03 | Total count | Expected response | 12 | **PASS** |

**Result: PASS — 3/3 functional tests.**

## 8. Results analysis and limitations

1. **SMP Safety:** All state mutations are synchronized under `lab2_mutex` or atomic operations, preventing race conditions across Cortex-A9 cores.
2. **Userspace Memory Isolation:** Transfers between kernel buffer and user buffers are strictly performed via `copy_to_user()` and `copy_from_user()`, returning `-EFAULT` on invalid virtual addresses.
3. **ABI Consistency:** The driver maintains strict backward compatibility with existing userspace consumers.

## 9. Troubleshooting

| Symptom | Root Cause | Resolution |
|---|---|---|
| `/dev/lab2` not found | `devtmpfs` not mounted or `mknod` omitted | Run `mknod /dev/lab2 c 240 0 && chmod 666 /dev/lab2` |
| Module load fails (`Exec format error`) | Vermagic mismatch between kernel and module | Recompile with `KDIR` pointing to the exact 6.1.90 tree |
| Permission denied on sysfs | Writing to a read-only (`0444`) attribute | Attributes marked `RO` reject write operations by design |

## 10. Git submission policy

Commit driver source `lab2_driver.c`, `Makefile`, and userspace test programs. Exclude kernel compilation artifacts (`.ko`, `.o`, `.mod.c`).
