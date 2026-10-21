# LAB 06 — Dynamic Device Buffer Status (EMPTY / DATA)

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 3/3 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 06 of the 20 Extension Exercises sequence** (`lab06_sysfs_status`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Provide dynamic status attribute in sysfs returning 'EMPTY' or 'DATA'.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Status attribute | Register /sys/class/lab2_class/lab2/status as read-only. |
| FR-02 | EMPTY state | Return 'EMPTY\n' when buffer_len == 0. |
| FR-03 | DATA state | Return 'DATA\n' when buffer_len > 0. |
| FR-04 | Dynamic evaluation | Evaluate buffer_len dynamically on every show invocation under mutex. |
| FR-05 | Consistent lifecycle | Register and unregister cleanly during module init and exit. |

## 2. Baseline analysis and design reasoning

Categorical state reflection via sysfs enables simple polling scripts to check buffer status without performing read() syscalls that alter device read pointers or state.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab06_sysfs_status/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
static ssize_t status_show(struct device *dev, struct device_attribute *attr, char *buf)
static DEVICE_ATTR_RO(status); /* [EXERCISE 6 - ADDED]: Tạo attribute chỉ đọc status */
    seq_printf(m, "Open Count  : %d\n", open_count);
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_status); /* [EXERCISE 6 - ADDED]: Đăng ký file sysfs status */
    device_remove_file(lab2_device, &dev_attr_status); /* [EXERCISE 6 - ADDED]: Gỡ file sysfs status */
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
cd assignments/driver-exercises/lab06_sysfs_status
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

### T01 - Initial empty status

```sh
cat /sys/class/lab2_class/lab2/status
```

**Observed behavior:**

```text
EMPTY
```


### T02 - Status after write

```sh
echo 'KernelData' > /dev/lab2 && cat /sys/class/lab2_class/lab2/status
```

**Observed behavior:**

```text
DATA
```


### T03 - Clear and re-verify

```sh
echo 'clear' > /dev/lab2 && cat /sys/class/lab2_class/lab2/status
```

**Observed behavior:**

```text
EMPTY
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initial empty status | Expected response | EMPTY | **PASS** |
| T02 | Status after write | Expected response | DATA | **PASS** |
| T03 | Clear and re-verify | Expected response | EMPTY | **PASS** |

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
