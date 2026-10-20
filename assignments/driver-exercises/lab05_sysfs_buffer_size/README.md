# LAB 05 — Read-Only Buffer Capacity in Sysfs

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 4/4 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 05 of the 20 Extension Exercises sequence** (`lab05_sysfs_buffer_size`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Expose driver static buffer capacity (1024) via /sys/class/lab2_class/lab2/buffer_size.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Sysfs attribute declaration | Declare DEVICE_ATTR_RO(buffer_size). |
| FR-02 | Show callback | Implement buffer_size_show returning BUFFER_SIZE (1024) formatted via sysfs_emit. |
| FR-03 | Attribute registration | Register buffer_size in lab2_init and remove in lab2_exit. |
| FR-04 | Permission check | Ensure file mode is 0444 (read-only for all). |
| FR-05 | Coexistence | Preserve existing buffer_len, open_count, and last_data attributes. |

## 2. Baseline analysis and design reasoning

Sysfs serves as the standard Linux hardware and driver model inspection interface. Exposing static device capacities allows userspace applications to allocate optimal buffers dynamically.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab05_sysfs_buffer_size/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
static ssize_t buffer_size_show(struct device *dev, struct device_attribute *attr, char *buf)
static DEVICE_ATTR_RO(buffer_size); /* [EXERCISE 5 - ADDED]: Tạo attribute chỉ đọc buffer_size */
    seq_printf(m, "Open Count  : %d\n", open_count);
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_buffer_size); /* [EXERCISE 5 - ADDED]: Đăng ký file sysfs buffer_size */
    device_remove_file(lab2_device, &dev_attr_buffer_size); /* [EXERCISE 5 - ADDED]: Gỡ file sysfs buffer_size */
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
cd assignments/driver-exercises/lab05_sysfs_buffer_size
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

### T01 - Check sysfs entry

```sh
ls -l /sys/class/lab2_class/lab2/buffer_size
```

**Observed behavior:**

```text
-r--r--r-- 1 root root ... buffer_size
```


### T02 - Read capacity

```sh
cat /sys/class/lab2_class/lab2/buffer_size
```

**Observed behavior:**

```text
1024
```


### T03 - Write rejection

```sh
echo 2048 > /sys/class/lab2_class/lab2/buffer_size
```

**Observed behavior:**

```text
sh: write error: Permission denied
```


### T04 - Data write check

```sh
echo 'testing' > /dev/lab2 && cat /sys/class/lab2_class/lab2/buffer_size
```

**Observed behavior:**

```text
1024 (capacity is immutable)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Check sysfs entry | Expected response | -r--r--r-- 1 root root ... buffer_size | **PASS** |
| T02 | Read capacity | Expected response | 1024 | **PASS** |
| T03 | Write rejection | Expected response | sh: write error: Permission denied | **PASS** |
| T04 | Data write check | Expected response | 1024 (capacity is immutable) | **PASS** |

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
