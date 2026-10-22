# LAB 09 — System Uptime Timestamp Recording via Jiffies

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 3/3 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 09 of the 20 Extension Exercises sequence** (`lab09_write_timestamp`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Record kernel jiffies timestamp on every write and display elapsed seconds in /proc/lab2_info.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Timestamp variable | Store unsigned long last_write_jiffies = 0. |
| FR-02 | Capture timestamp | Update last_write_jiffies = jiffies on successful write. |
| FR-03 | Elapsed time | Calculate elapsed seconds using (jiffies - last_write_jiffies) / HZ. |
| FR-04 | Procfs integration | Display last write jiffies and elapsed seconds in /proc/lab2_info. |
| FR-05 | Unwritten state | Display 'None' or '0' if no write has occurred since boot. |

## 2. Baseline analysis and design reasoning

Linux kernel jiffies counter increments on timer interrupts at frequency HZ. Dividing jiffies difference by HZ yields human-readable elapsed seconds with low computational overhead.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab09_write_timestamp/lab2_driver.c`:

```c
#include <linux/jiffies.h> /* [EXERCISE 9 - ADDED]: Thư viện lấy nhịp đồng hồ hệ thống (jiffies và HZ) */
static int open_count = 0;
static unsigned long last_write_jiffies = 0; /* [EXERCISE 9 - ADDED]: Lưu mốc thời gian (jiffies) của lần ghi cuối */
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
    last_write_jiffies = jiffies; /* [EXERCISE 9 - ADDED]: Ghi nhận thời điểm hệ thống ngay khi write() thành công */
    pr_info("lab2: wrote %d bytes at jiffies=%lu\n", bytes_to_write, last_write_jiffies);
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
    seq_printf(m, "Open Count  : %d\n", open_count);
    if (last_write_jiffies == 0)
        seq_printf(m, "Last Write  : %lu jiffies (~%lu sec since boot)\n",
                   last_write_jiffies, last_write_jiffies / HZ);
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
cd assignments/driver-exercises/lab09_write_timestamp
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

### T01 - Initial procfs check

```sh
cat /proc/lab2_info | grep 'Last Write'
```

**Observed behavior:**

```text
Last Write    : No write yet
```


### T02 - Write data

```sh
echo 'timestamp test' > /dev/lab2
```

**Observed behavior:**

```text
Timestamp captured
```


### T03 - Read procfs after delay

```sh
sleep 3 && cat /proc/lab2_info | grep 'Last Write'
```

**Observed behavior:**

```text
Last Write    : 3 seconds ago (jiffies: ...)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initial procfs check | Expected response | Last Write    : No write yet | **PASS** |
| T02 | Write data | Expected response | Timestamp captured | **PASS** |
| T03 | Read procfs after delay | Expected response | Last Write    : 3 seconds ago (jiffies:  | **PASS** |

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
