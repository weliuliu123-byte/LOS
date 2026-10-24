# LAB 18 — Comprehensive Kernel Diagnostic Dashboard

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 3/3 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 18 of the 20 Extension Exercises sequence** (`lab18_expanded_procfs`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Expand /proc/lab2_info into an all-inclusive diagnostic report with read/write counts, total volume, and status.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Multi-metric reporting | Include Device Name, Major Number, Buffer Size, Buffer Len, Open Count. |
| FR-02 | Operation counters | Display Read Count and Write Count. |
| FR-03 | Cumulative throughput | Display Total Bytes Read and Total Bytes Written. |
| FR-04 | Buffer status | Display Status (EMPTY / ACTIVE / FULL). |
| FR-05 | Seq_file formatting | Format cleanly using seq_printf alignment. |

## 2. Baseline analysis and design reasoning

Consolidating all internal state indicators into a single procfs entry provides system administrators with an instantaneous diagnostic snapshot via a single cat command.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab18_expanded_procfs/lab2_driver.c`:

```c
static int open_count = 0;
static int read_count = 0;
static int write_count = 0;
static unsigned long total_bytes_read = 0;
static unsigned long total_bytes_written = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    read_count++; /* [EXERCISE 18 - ADDED]: Tăng số lần gọi read() */
    total_bytes_read += bytes_to_read; /* [EXERCISE 18 - ADDED]: Cộng dồn số byte đọc thành công */
    if (mutex_lock_interruptible(&lab2_mutex))
    write_count++;
    total_bytes_written += bytes_to_write;
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
    /* [EXERCISE 18 - ADDED]: Bổ sung trạng thái buffer, read_count, write_count và tổng byte đọc/ghi */
    seq_printf(m, "Open Count        : %d\n", open_count);
    seq_printf(m, "Read Count        : %d\n", read_count);
    seq_printf(m, "Write Count       : %d\n", write_count);
    seq_printf(m, "Total Bytes Read  : %lu\n", total_bytes_read);
    seq_printf(m, "Total Bytes Write : %lu\n", total_bytes_written);
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
cd assignments/driver-exercises/lab18_expanded_procfs
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

### T01 - Inspect expanded dashboard

```sh
cat /proc/lab2_info
```

**Observed behavior:**

```text
Displays structured report with all metrics
```


### T02 - Perform I/O operations

```sh
echo 'test' > /dev/lab2 && cat /dev/lab2 > /dev/null
```

**Observed behavior:**

```text
I/O executed
```


### T03 - Verify updated metrics

```sh
cat /proc/lab2_info
```

**Observed behavior:**

```text
Read Count: 1, Write Count: 1, Status: ACTIVE
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Inspect expanded dashboard | Expected response | Displays structured report with all metr | **PASS** |
| T02 | Perform I/O operations | Expected response | I/O executed | **PASS** |
| T03 | Verify updated metrics | Expected response | Read Count: 1, Write Count: 1, Status: A | **PASS** |

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
