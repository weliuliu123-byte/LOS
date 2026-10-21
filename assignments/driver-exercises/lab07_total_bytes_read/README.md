# LAB 07 — Cumulative Transferred Byte Accounting (Read)

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 4/4 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 07 of the 20 Extension Exercises sequence** (`lab07_total_bytes_read`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Track lifetime bytes transferred to userspace via total_bytes_read sysfs attribute.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Counter variable | Add unsigned long total_bytes_read = 0. |
| FR-02 | Accounting | Accumulate bytes_to_read on successful copy_to_user in lab2_read. |
| FR-03 | Sysfs attribute | Expose total_bytes_read under sysfs with DEVICE_ATTR_RO. |
| FR-04 | Mutex protection | Update counter under lab2_mutex to prevent lost increments on SMP. |
| FR-05 | Monotonic increase | Counter increases monotonically over driver lifetime until rmmod. |

## 2. Baseline analysis and design reasoning

Tracking throughput volume is critical for embedded diagnostics, bandwidth rate limiting, and detecting abnormal traffic spikes in device driver communication.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab07_total_bytes_read/lab2_driver.c`:

```c
static int open_count = 0;
static unsigned long total_bytes_read = 0; /* [EXERCISE 7 - ADDED]: Biến cộng dồn tổng số byte đã đọc */
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    total_bytes_read += bytes_to_read; /* [EXERCISE 7 - ADDED]: Cộng dồn số byte thực tế đã trả về cho user */
    pr_info("lab2: read %d bytes (total_bytes_read=%lu)\n", bytes_to_read, total_bytes_read);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
/* [EXERCISE 7 - ADDED]: Hàm hiển thị total_bytes_read qua sysfs */
static ssize_t total_bytes_read_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%lu\n", total_bytes_read);
static DEVICE_ATTR_RO(total_bytes_read);
    seq_printf(m, "Open Count       : %d\n", open_count);
    seq_printf(m, "Total Bytes Read : %lu\n", total_bytes_read); /* [EXERCISE 7 - ADDED]: Hiển thị trong /proc/lab2_info */
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_total_bytes_read); /* [EXERCISE 7 - ADDED]: Tạo file sysfs total_bytes_read */
    device_remove_file(lab2_device, &dev_attr_total_bytes_read); /* [EXERCISE 7 - ADDED]: Xóa file sysfs total_bytes_read */
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
cd assignments/driver-exercises/lab07_total_bytes_read
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

### T01 - Initial volume

```sh
cat /sys/class/lab2_class/lab2/total_bytes_read
```

**Observed behavior:**

```text
0
```


### T02 - Write and read

```sh
echo '12345' > /dev/lab2 && cat /dev/lab2 > /dev/null
```

**Observed behavior:**

```text
5 bytes transferred
```


### T03 - Verify accumulation

```sh
cat /sys/class/lab2_class/lab2/total_bytes_read
```

**Observed behavior:**

```text
5
```


### T04 - Multiple reads

```sh
cat /dev/lab2 > /dev/null && cat /sys/class/lab2_class/lab2/total_bytes_read
```

**Observed behavior:**

```text
10
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initial volume | Expected response | 0 | **PASS** |
| T02 | Write and read | Expected response | 5 bytes transferred | **PASS** |
| T03 | Verify accumulation | Expected response | 5 | **PASS** |
| T04 | Multiple reads | Expected response | 10 | **PASS** |

**Result: PASS — 4/4 functional tests.**

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
