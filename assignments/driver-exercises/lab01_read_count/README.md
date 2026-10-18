# LAB 01 — Read Operation Counter in Sysfs

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 5/5 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 01 of the 20 Extension Exercises sequence** (`lab01_read_count`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Track cumulative read operations and expose read_count as a read-only sysfs attribute.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Global read counter | Declare global static unsigned int read_count initialized to 0. |
| FR-02 | Increment on read() | Increment read_count on each read() invocation under mutex protection. |
| FR-03 | Sysfs attribute | Expose read_count under /sys/class/lab2_class/lab2/read_count with DEVICE_ATTR_RO. |
| FR-04 | EOF counting | Reads returning 0 bytes (EOF) are counted to match VFS invocation semantics. |
| FR-05 | Preserve baseline | Maintain standard device read/write buffer behavior and /proc/lab2_info. |

## 2. Baseline analysis and design reasoning

The character driver's read callback is invoked by userspace syscalls like cat or custom readers. When userspace reads /dev/lab2, the VFS invokes lab2_read(). Counting both payload reads and EOF return checks provides complete accounting of user-space interactions.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab01_read_count/lab2_driver.c`:

```c
static int open_count = 0;
static int read_count = 0; /* [EXERCISE 1 - ADDED]: Biến toàn cục đếm số lần gọi read() */
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    read_count++; /* [EXERCISE 1 - ADDED]: Tăng bộ đếm mỗi khi user-space gọi hàm read() */
    pr_info("lab2: read %d bytes (read_count=%d)\n", bytes_to_read, read_count);
    if (mutex_lock_interruptible(&lab2_mutex))
static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", open_count);
static DEVICE_ATTR_RO(open_count);
/* [EXERCISE 1 - ADDED]: Hàm hiển thị giá trị read_count khi đọc file /sys/class/lab2_class/lab2/read_count */
static ssize_t read_count_show(struct device *dev, struct device_attribute *attr, char *buf)
    return sprintf(buf, "%d\n", read_count);
static DEVICE_ATTR_RO(read_count); /* [EXERCISE 1 - ADDED]: Khai báo sysfs attribute chỉ đọc (Read-Only) */
    seq_printf(m, "Open Count  : %d\n", open_count);
    device_create_file(lab2_device, &dev_attr_open_count);
    device_create_file(lab2_device, &dev_attr_read_count); /* [EXERCISE 1 - ADDED]: Tạo file sysfs read_count */
    device_remove_file(lab2_device, &dev_attr_read_count); /* [EXERCISE 1 - ADDED]: Xóa file sysfs read_count khi gỡ driver */
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
cd assignments/driver-exercises/lab01_read_count
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
insmod /lib/modules/6.1.90/lab2_driver.ko && cat /sys/class/lab2_class/lab2/read_count
```

**Observed behavior:**

```text
0
```


### T02 - First read check

```sh
echo -n 'HELLO' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
HELLO
(read_count becomes 2 due to payload + EOF reads)
```


### T03 - Repeated read

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
HELLO
(read_count increments to 4)
```


### T04 - Write independence

```sh
echo 'TEST' > /dev/lab2 && cat /sys/class/lab2_class/lab2/read_count
```

**Observed behavior:**

```text
Counter remains unchanged by write operations
```


### T05 - Sysfs write protection

```sh
echo 10 > /sys/class/lab2_class/lab2/read_count
```

**Observed behavior:**

```text
sh: write error: Permission denied (read-only attribute)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Initialization | Expected response | 0 | **PASS** |
| T02 | First read check | Expected response | HELLO | **PASS** |
| T03 | Repeated read | Expected response | HELLO | **PASS** |
| T04 | Write independence | Expected response | Counter remains unchanged by write opera | **PASS** |
| T05 | Sysfs write protection | Expected response | sh: write error: Permission denied (read | **PASS** |

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
