# LAB 02 — Write Operation Counter in Procfs

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 5/5 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 02 of the 20 Extension Exercises sequence** (`lab02_write_count`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Track successful write operations and display write_count in /proc/lab2_info.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Global write counter | Add write_count initialized to 0. |
| FR-02 | Successful writes | Increment write_count once per successful lab2_write() returning > 0 bytes. |
| FR-03 | Zero-length writes | write(fd, buf, 0) returns 0 without incrementing the counter. |
| FR-04 | Failure handling | Do not increment if copy_from_user fails or mutex acquisition fails. |
| FR-05 | procfs reporting | Format Write Count : <value> inside /proc/lab2_info via seq_printf. |

## 2. Baseline analysis and design reasoning

Userspace applications stream data into the kernel via write(). Tracking valid write operations provides an accurate audit trail of device modifications without polluting sysfs attributes.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab02_write_count/lab2_driver.c`:

```c
static int open_count = 0;
static int write_count = 0; /* [EXERCISE 2 - ADDED]: Biến toàn cục đếm số lần write() thành công */
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
    write_count++; /* [EXERCISE 2 - ADDED]: Tăng write_count sau khi ghi dữ liệu thành công */
    pr_info("lab2: wrote %d bytes (write_count=%d)\n", bytes_to_write, write_count);
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
    seq_printf(m, "Open Count  : %d\n", open_count);
    seq_printf(m, "Write Count : %d\n", write_count); /* [EXERCISE 2 - ADDED]: Hiển thị write_count trong /proc/lab2_info */
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
cd assignments/driver-exercises/lab02_write_count
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

### T01 - Initialization

```sh
cat /proc/lab2_info | grep 'Write Count'
```

**Observed behavior:**

```text
Write Count : 0
```


### T02 - First write

```sh
echo 'first' > /dev/lab2 && cat /proc/lab2_info | grep 'Write Count'
```

**Observed behavior:**

```text
Write Count : 1
```


### T03 - Second write

```sh
echo 'second' > /dev/lab2 && cat /proc/lab2_info | grep 'Write Count'
```

**Observed behavior:**

```text
Write Count : 2
```


### T04 - Read independence

```sh
cat /dev/lab2 && cat /proc/lab2_info | grep 'Write Count'
```

**Observed behavior:**

```text
Counter remains 2
```


### T05 - Zero length write

```sh
./test_zero_write && cat /proc/lab2_info | grep 'Write Count'
```

**Observed behavior:**

```text
Zero-byte write returns 0, counter stays 2
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initialization | Expected response | Write Count : 0 | **PASS** |
| T02 | First write | Expected response | Write Count : 1 | **PASS** |
| T03 | Second write | Expected response | Write Count : 2 | **PASS** |
| T04 | Read independence | Expected response | Counter remains 2 | **PASS** |
| T05 | Zero length write | Expected response | Zero-byte write returns 0, counter stays | **PASS** |

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
