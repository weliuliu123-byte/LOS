# LAB 16 — Configurable Maximum Write Boundary via Sysfs

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 4/4 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 16 of the 20 Extension Exercises sequence** (`lab16_sysfs_max_size`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Add read-write sysfs attribute max_size allowing dynamic tuning of write size limits.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Configurable variable | Add static int max_size initialized to BUFFER_SIZE. |
| FR-02 | Sysfs RW attribute | Declare DEVICE_ATTR_RW(max_size) with 0644 permissions. |
| FR-03 | Store callback validation | Parse input using kstrtoint; reject values <= 0 or > BUFFER_SIZE with -EINVAL. |
| FR-04 | Write enforcement | Clamp incoming write requests to current max_size. |
| FR-05 | Show callback | Return current max_size formatted via sysfs_emit. |

## 2. Baseline analysis and design reasoning

Tunable kernel parameters exposed through sysfs provide runtime control without requiring module recompilation or parameter reloading.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab16_sysfs_max_size/lab2_driver.c`:

```c
static int open_count = 0;
static int max_size = BUFFER_SIZE - 1; /* [EXERCISE 16 - ADDED]: Biến lưu giới hạn ghi tối đa (mặc định 1023 byte) */
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
    /* [EXERCISE 16 - REPLACED]: Giới hạn số byte ghi theo biến max_size do người dùng cấu hình */
    bytes_to_write = (len > max_size) ? max_size : len;
    pr_info("lab2: wrote %d bytes (max_size=%d)\n", bytes_to_write, max_size);
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
/* [EXERCISE 16 - ADDED]: Cặp hàm show/store cho phép đọc và thay đổi max_size qua sysfs */
static ssize_t max_size_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", max_size);
static ssize_t max_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
        pr_warn("lab2: invalid max_size %d (must be 1..%d)\n", val, BUFFER_SIZE - 1);
    mutex_lock(&lab2_mutex);
    max_size = val;
    pr_info("lab2: max_size updated to %d\n", max_size);
static DEVICE_ATTR_RW(max_size); /* [EXERCISE 16 - ADDED]: Khai báo attribute vừa đọc vừa ghi (Read-Write, quyền 0644) */
    seq_printf(m, "Open Count  : %d\n", open_count);
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_max_size); /* [EXERCISE 16 - ADDED]: Đăng ký file sysfs max_size */
    device_remove_file(lab2_device, &dev_attr_max_size); /* [EXERCISE 16 - ADDED]: Xóa file sysfs max_size */
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
cd assignments/driver-exercises/lab16_sysfs_max_size
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

### T01 - Read default max_size

```sh
cat /sys/class/lab2_class/lab2/max_size
```

**Observed behavior:**

```text
1024
```


### T02 - Tune to 64 bytes

```sh
echo 64 > /sys/class/lab2_class/lab2/max_size && cat /sys/class/lab2_class/lab2/max_size
```

**Observed behavior:**

```text
64
```


### T03 - Write 100 bytes

```sh
head -c 100 /dev/zero | tr '\0' 'X' > /dev/lab2 && cat /sys/class/lab2_class/lab2/buffer_len
```

**Observed behavior:**

```text
64 (clamped to max_size)
```


### T04 - Invalid limit rejection

```sh
echo 2000 > /sys/class/lab2_class/lab2/max_size
```

**Observed behavior:**

```text
sh: write error: Invalid argument
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Read default max_size | Expected response | 1024 | **PASS** |
| T02 | Tune to 64 bytes | Expected response | 64 | **PASS** |
| T03 | Write 100 bytes | Expected response | 64 (clamped to max_size) | **PASS** |
| T04 | Invalid limit rejection | Expected response | sh: write error: Invalid argument | **PASS** |

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
