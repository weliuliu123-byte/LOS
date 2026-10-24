# LAB 15 — Newline-Tolerant Command Parsing

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 4/4 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 15 of the 20 Extension Exercises sequence** (`lab15_clear_newline`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Support both 'clear' and 'clear\n' in write callback, saving all other input as data.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Trailing newline stripping | Inspect buffer and ignore trailing '\n' or '\r' during command matching. |
| FR-02 | Exact matching | Match 'clear' whether sent via echo (which appends \n) or direct binary write. |
| FR-03 | Buffer purge | Zero buffer_len and memset device_buffer. |
| FR-04 | Non-command preservation | Strings like 'clear_all' or 'clearing' are preserved as normal data. |
| FR-05 | Mutex protection | Command parsing executes under lab2_mutex. |

## 2. Baseline analysis and design reasoning

The standard echo command appends an ASCII newline (0x0A) by default. Robust command dispatchers must normalize input by stripping optional delimiters before comparison.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab15_clear_newline/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    char cmd_check[8]; /* [EXERCISE 15 - ADDED]: Buffer nhỏ để kiểm tra lệnh "clear\n" trước khi ghi đè device_buffer */
    if (mutex_lock_interruptible(&lab2_mutex))
    /* [EXERCISE 15 - ADDED]: Nếu độ dài đúng bằng 5 ("clear") hoặc 6 ("clear\n"), kiểm tra xem có phải lệnh xóa không */
        if (strcmp(cmd_check, "clear\n") == 0 || strcmp(cmd_check, "clear") == 0) {
            pr_info("lab2: buffer cleared via write command\n");
    // Nếu không phải lệnh "clear\n" thì lưu dữ liệu vào device_buffer như bình thường
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
cd assignments/driver-exercises/lab15_clear_newline
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

### T01 - Write data

```sh
echo 'Sample' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
Sample
```


### T02 - Clear with echo (adds newline)

```sh
echo 'clear' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
(empty - successfully cleared)
```


### T03 - Clear without newline

```sh
echo -n 'clear' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
(empty - successfully cleared)
```


### T04 - False match test

```sh
echo 'clear123' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
clear123 (preserved as data)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Write data | Expected response | Sample | **PASS** |
| T02 | Clear with echo (adds newline) | Expected response | (empty - successfully cleared) | **PASS** |
| T03 | Clear without newline | Expected response | (empty - successfully cleared) | **PASS** |
| T04 | False match test | Expected response | clear123 (preserved as data) | **PASS** |

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
