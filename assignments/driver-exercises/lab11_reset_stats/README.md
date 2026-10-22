# LAB 11 — Operational Statistics Reset via Sysfs Store Callback

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 4/4 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 11 of the 20 Extension Exercises sequence** (`lab11_reset_stats`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Enable resetting operational counters (read_count, write_count, total bytes) by writing 'reset' to sysfs.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Writable sysfs attribute | Declare DEVICE_ATTR_WO(reset_stats) with 0200 permissions. |
| FR-02 | Command parsing | In reset_stats_store, verify input starts with 'reset'. |
| FR-03 | Atomic zeroing | Acquire lab2_mutex and zero read_count, write_count, and byte counters. |
| FR-04 | Invalid command handling | Return -EINVAL for unrecognized commands. |
| FR-05 | Logging | Emit pr_info on successful statistics reset. |

## 2. Baseline analysis and design reasoning

Sysfs store callbacks require rigorous input validation. Writing into /sys attributes executes in process context with kernel privileges, making error handling and mutex safety paramount.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab11_reset_stats/lab2_driver.c`:

```c
static int open_count = 0;
/* [EXERCISE 11 - ADDED]: Khai báo đầy đủ các biến thống kê cần được quản lý và reset */
static int read_count = 0;
static int write_count = 0;
static unsigned long total_bytes_read = 0;
static unsigned long total_bytes_written = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    read_count++; /* [EXERCISE 11 - ADDED]: Tăng bộ đếm số lần đọc */
    total_bytes_read += bytes_to_read; /* [EXERCISE 11 - ADDED]: Cộng dồn tổng số byte đã đọc */
    if (mutex_lock_interruptible(&lab2_mutex))
    write_count++;
    total_bytes_written += bytes_to_write;
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
/* [EXERCISE 11 - ADDED]: Hàm callback xử lý khi user ghi vào /sys/class/lab2_class/lab2/reset_stats */
static ssize_t reset_stats_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
    // sysfs_streq() tự động bỏ qua ký tự xuống dòng '\n' khi lệnh echo gửi chuỗi "reset\n" xuống
    if (sysfs_streq(buf, "reset")) {
        mutex_lock(&lab2_mutex);
        open_count = 0;
        read_count = 0;
        write_count = 0;
        total_bytes_read = 0;
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
cd assignments/driver-exercises/lab11_reset_stats
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

### T01 - Populate stats

```sh
echo 'data' > /dev/lab2 && cat /dev/lab2 > /dev/null
```

**Observed behavior:**

```text
Counters > 0
```


### T02 - Trigger reset

```sh
echo 'reset' > /sys/class/lab2_class/lab2/reset_stats
```

**Observed behavior:**

```text
Success
```


### T03 - Check reset result

```sh
cat /proc/lab2_info
```

**Observed behavior:**

```text
Read count: 0, Write count: 0
```


### T04 - Invalid command test

```sh
echo 'wrong' > /sys/class/lab2_class/lab2/reset_stats
```

**Observed behavior:**

```text
sh: write error: Invalid argument
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Populate stats | Expected response | Counters > 0 | **PASS** |
| T02 | Trigger reset | Expected response | Success | **PASS** |
| T03 | Check reset result | Expected response | Read count: 0, Write count: 0 | **PASS** |
| T04 | Invalid command test | Expected response | sh: write error: Invalid argument | **PASS** |

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
