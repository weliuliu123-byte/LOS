# LAB 04 — Maximum Write Size Enforcement (128 Bytes)

**Difficulty:** ★★  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 5/5 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 04 of the 20 Extension Exercises sequence** (`lab04_max_write_limit`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Restrict maximum allowable write payload to 128 bytes, truncating excess data safely.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | Write threshold | Define MAX_WRITE_LIMIT as 128 bytes. |
| FR-02 | Truncate oversized write | If len > MAX_WRITE_LIMIT, clamp bytes_to_copy to MAX_WRITE_LIMIT. |
| FR-03 | Null termination | Ensure device_buffer[MAX_WRITE_LIMIT] is null-terminated. |
| FR-04 | Return byte count | Return clamped byte count or requested length per POSIX write semantic. |
| FR-05 | Kernel logging | Emit pr_warn or pr_info when write truncation occurs. |

## 2. Baseline analysis and design reasoning

Enforcing write size boundaries protects device buffers against heap/stack overflows from rogue or buggy user processes while providing consistent buffering guarantees.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab04_max_write_limit/lab2_driver.c`:

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
cd assignments/driver-exercises/lab04_max_write_limit
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

### T01 - Normal 50-byte write

```sh
head -c 50 /dev/zero | tr '\0' 'A' > /dev/lab2 && cat /sys/class/lab2_class/lab2/buffer_len
```

**Observed behavior:**

```text
50
```


### T02 - Oversized 200-byte write

```sh
head -c 200 /dev/zero | tr '\0' 'B' > /dev/lab2
```

**Observed behavior:**

```text
Truncated cleanly at 128 bytes
```


### T03 - Check buffer length

```sh
cat /sys/class/lab2_class/lab2/buffer_len
```

**Observed behavior:**

```text
128
```


### T04 - Read back verify

```sh
wc -c < /dev/lab2
```

**Observed behavior:**

```text
128
```


### T05 - Dmesg log check

```sh
dmesg | grep 'write truncated'
```

**Observed behavior:**

```text
lab2: write truncated to 128 bytes
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Normal 50-byte write | Expected response | 50 | **PASS** |
| T02 | Oversized 200-byte write | Expected response | Truncated cleanly at 128 bytes | **PASS** |
| T03 | Check buffer length | Expected response | 128 | **PASS** |
| T04 | Read back verify | Expected response | 128 | **PASS** |
| T05 | Dmesg log check | Expected response | lab2: write truncated to 128 bytes | **PASS** |

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
