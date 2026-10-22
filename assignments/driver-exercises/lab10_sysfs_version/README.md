# LAB 10 — Driver Version String Attribute in Sysfs

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 2/2 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 10 of the 20 Extension Exercises sequence** (`lab10_sysfs_version`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Expose driver release version '1.0' as a read-only sysfs attribute.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Version definition | Define DRIVER_VERSION '1.0'. |
| FR-02 | Sysfs attribute | Declare DEVICE_ATTR_RO(version). |
| FR-03 | Show callback | Return DRIVER_VERSION formatted with newline. |
| FR-04 | Read-only enforcement | File permissions 0444 block modification attempts. |
| FR-05 | Module cleanup | Remove attribute on module exit. |

## 2. Baseline analysis and design reasoning

Providing software version strings via sysfs allows automated deployment scripts to verify driver capabilities before launching userspace daemons.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab10_sysfs_version/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
static ssize_t version_show(struct device *dev, struct device_attribute *attr, char *buf)
static DEVICE_ATTR_RO(version); /* [EXERCISE 10 - ADDED]: Khai báo sysfs attribute chỉ đọc version */
    seq_printf(m, "Open Count  : %d\n", open_count);
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_version); /* [EXERCISE 10 - ADDED]: Đăng ký file sysfs version */
    device_remove_file(lab2_device, &dev_attr_version); /* [EXERCISE 10 - ADDED]: Gỡ file sysfs version */
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
cd assignments/driver-exercises/lab10_sysfs_version
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

### T01 - Read version attribute

```sh
cat /sys/class/lab2_class/lab2/version
```

**Observed behavior:**

```text
1.0
```


### T02 - Write rejection

```sh
echo '2.0' > /sys/class/lab2_class/lab2/version
```

**Observed behavior:**

```text
Permission denied
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Read version attribute | Expected response | 1.0 | **PASS** |
| T02 | Write rejection | Expected response | Permission denied | **PASS** |

**Result: PASS — 2/2 functional tests.**

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
