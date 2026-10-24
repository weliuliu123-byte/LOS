# LAB 19 — One-Shot Buffer Read Mode with Replay Protection

**Difficulty:** ★★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 5/5 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 19 of the 20 Extension Exercises sequence** (`lab19_one_shot_read`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Implement single-read semantics where reading the buffer returns EOF on subsequent attempts until new data is written.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Read flag | Add static int data_read = 0. |
| FR-02 | One-shot enforcement | If data_read == 1, lab2_read returns 0 (EOF) immediately. |
| FR-03 | Flag assertion | Set data_read = 1 after first successful read transfer. |
| FR-04 | Flag reset | Reset data_read = 0 when new data is written to the device. |
| FR-05 | Mutex protection | Synchronize flag checks and transitions under lab2_mutex. |

## 2. Baseline analysis and design reasoning

One-shot semantics turn the character device into a consume-once message queue, preventing multiple consumers from reading stale data repeatedly.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab19_one_shot_read/lab2_driver.c`:

```c
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
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
cd assignments/driver-exercises/lab19_one_shot_read
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

### T01 - Write payload

```sh
echo 'SingleMessage' > /dev/lab2
```

**Observed behavior:**

```text
Data ready
```


### T02 - First read (success)

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
SingleMessage
```


### T03 - Second read (EOF)

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
(returns empty / EOF)
```


### T04 - New write re-arms read

```sh
echo 'SecondMessage' > /dev/lab2 && cat /dev/lab2
```

**Observed behavior:**

```text
SecondMessage
```


### T05 - Third read (EOF again)

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
(returns empty / EOF)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Write payload | Expected response | Data ready | **PASS** |
| T02 | First read (success) | Expected response | SingleMessage | **PASS** |
| T03 | Second read (EOF) | Expected response | (returns empty / EOF) | **PASS** |
| T04 | New write re-arms read | Expected response | SecondMessage | **PASS** |
| T05 | Third read (EOF again) | Expected response | (returns empty / EOF) | **PASS** |

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
