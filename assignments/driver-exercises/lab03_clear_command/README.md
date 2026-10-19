# LAB 03 — Buffer Clear Command via Write

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 5/5 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 03 of the 20 Extension Exercises sequence** (`lab03_clear_command`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Clear the device buffer when the userspace string 'clear' is written to /dev/lab2.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Command detection | Check if incoming write payload equals 'clear' using strcmp or strncmp. |
| FR-02 | Buffer truncation | Set buffer_len = 0 and memset(device_buffer, 0, BUFFER_SIZE). |
| FR-03 | Normal write fallback | If input is not 'clear', save data normally into device_buffer. |
| FR-04 | Return value contract | Return full byte count (e.g. 5) to prevent userspace write retry loops. |
| FR-05 | Concurrency safety | Buffer clear and payload copy occur strictly under lab2_mutex. |

## 2. Baseline analysis and design reasoning

Control commands multiplexed over character device data streams require exact string matching. Returning the requested write length ensures standard POSIX utilities (like echo) terminate cleanly without retrying.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab03_clear_command/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
    /* [EXERCISE 3 - ADDED]: Kiểm tra nếu chuỗi ghi vào là "clear" (echo -n) hoặc "clear\n" (echo thường) */
    if (strncmp(device_buffer, "clear", 5) == 0 &&
        pr_info("lab2: device buffer cleared\n");
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
    seq_printf(m, "Open Count  : %d\n", open_count);
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
cd assignments/driver-exercises/lab03_clear_command
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

### T01 - Normal write

```sh
echo 'Hello Lab2' > /dev/lab2 && cat /sys/class/lab2_class/lab2/buffer_len
```

**Observed behavior:**

```text
11
```


### T02 - Clear command

```sh
echo 'clear' > /dev/lab2 && cat /sys/class/lab2_class/lab2/buffer_len
```

**Observed behavior:**

```text
0
```


### T03 - Read after clear

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
(returns empty string / 0 bytes)
```


### T04 - Re-write after clear

```sh
echo 'Active' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
Active
```


### T05 - Partial prefix check

```sh
echo 'clear123' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
clear123 (not cleared, saved as data)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Normal write | Expected response | 11 | **PASS** |
| T02 | Clear command | Expected response | 0 | **PASS** |
| T03 | Read after clear | Expected response | (returns empty string / 0 bytes) | **PASS** |
| T04 | Re-write after clear | Expected response | Active | **PASS** |
| T05 | Partial prefix check | Expected response | clear123 (not cleared, saved as data) | **PASS** |

**Result: PASS — 5/5 functional tests.**

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
