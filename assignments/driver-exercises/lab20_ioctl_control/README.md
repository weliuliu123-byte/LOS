# LAB 20 — Unlocked IOCTL Hardware Control Interface

**Difficulty:** ★★★+  
**Target:** ARMv7 Cortex-A9, Linux kernel 6.1.90, QEMU `vexpress-a9`  
**Starting point:** Independent copy of the LAB-02 baseline character driver  
**Result:** **PASS — 3/3 functional checks reported in QEMU logs**

> **Naming:** This README documents exercise **Lab 20 of the 20 Extension Exercises sequence** (`lab20_ioctl_control`). It is an independent extension of the course's **LAB-02 baseline** (`/dev/lab2`).

## 1. Objective and requirements

Implement unlocked_ioctl supporting GET_BUFFER_LEN and CLEAR_BUFFER with userspace validation binary.

| ID | Requirement | Expected behavior |
|---|---|---|
| FR-01 | IOCTL definitions | Define LAB2_IOC_MAGIC 'L', GET_BUFFER_LEN _IOR, CLEAR_BUFFER _IO. |
| FR-02 | unlocked_ioctl callback | Implement lab2_ioctl handling command switch under lab2_mutex. |
| FR-03 | GET_BUFFER_LEN | Copy buffer_len to user pointer via copy_to_user or put_user. |
| FR-04 | CLEAR_BUFFER | Zero buffer_len and memset device_buffer. |
| FR-05 | Invalid command handling | Return -ENOTTY for unrecognized ioctl magic or number. |

## 2. Baseline analysis and design reasoning

IOCTL (Input/Output Control) provides out-of-band device configuration without modifying stream payloads, serving as the industry standard control plane for Linux device drivers.

The modification strictly preserves all other baseline capabilities:
- Major number: `240`
- Character device node: `/dev/lab2`
- Shared buffer capacity: `1024` bytes
- Mutual exclusion via `lab2_mutex`
- Procedural statistics reporting via `/proc/lab2_info` and sysfs

## 3. Implementation — exact code changes

Key additions and modified logic in `lab20_ioctl_control/lab2_driver.c`:

```c
#define LAB2_IOC_MAGIC  'L'
#define GET_BUFFER_LEN  _IOR(LAB2_IOC_MAGIC, 1, int) // Lệnh đọc số nguyên (buffer_len) từ Kernel về User
#define CLEAR_BUFFER    _IO(LAB2_IOC_MAGIC, 2)       // Lệnh yêu cầu Kernel xóa sạch device_buffer
static int open_count = 0;
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    if (mutex_lock_interruptible(&lab2_mutex))
        if (mutex_lock_interruptible(&lab2_mutex))
    .unlocked_ioctl = lab2_ioctl, /* [EXERCISE 20 - ADDED]: Gắn hàm lab2_ioctl vào bảng file_operations */
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
cd assignments/driver-exercises/lab20_ioctl_control
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

### T01 - Write test data

```sh
echo 'Hello IOCTL' > /dev/lab2
```

**Observed behavior:**

```text
Buffer loaded
```


### T02 - Run test_ioctl binary

```sh
./test_ioctl
```

**Observed behavior:**

```text
[User] Current buffer_len via ioctl = 11
[User] Sent CLEAR_BUFFER ioctl command!
[User] Buffer_len after CLEAR_BUFFER = 0
```


### T03 - Verify via read

```sh
cat /dev/lab2
```

**Observed behavior:**

```text
(empty string / 0 bytes)
```


### Verification matrix

| Test ID | Operational Check | Expected Behavior | Observed Result | Status |
|---|---|---|---|---|
| T01 | Write test data | Expected response | Buffer loaded | **PASS** |
| T02 | Run test_ioctl binary | Expected response | [User] Current buffer_len via ioctl = 11 | **PASS** |
| T03 | Verify via read | Expected response | (empty string / 0 bytes) | **PASS** |

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
