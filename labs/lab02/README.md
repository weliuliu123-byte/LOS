# LAB-02 — Linux Device Driver & File System Management

**Status: COMPLETED — Parts A–D verified.**

This lab builds an ARMv7 Linux character device driver, exposes kernel information through procfs/sysfs, tests JFFS2 on a simulated NAND device, and configures BusyBox init to load the driver automatically at boot.

## Development environment

| Component | Verified configuration |
|---|---|
| Host | Ubuntu (x86_64); MTD tests on host kernel `7.0.0-31-generic` |
| Emulated target | QEMU `vexpress-a9`, ARM Cortex-A9 |
| Target kernel | Linux `6.1.90` |
| Cross compiler | `arm-linux-gnueabihf-` |
| Root filesystem | BusyBox + initramfs |
| Driver module | `lab2_driver.ko` (ARM EABI5, `vermagic: 6.1.90 SMP mod_unload ARMv7 p2v8`) |

**Execution environments are distinct:** parts A, B and D run in ARM QEMU; part C uses the host Linux kernel's `nandsim` driver.

## Part A — Character device driver (PASS)

The module implements `open()`, `read()`, `write()` and `release()` via `file_operations`, uses `copy_to_user()` / `copy_from_user()` to exchange bytes with userspace, and protects shared state with a mutex.

| Interface | Value |
|---|---|
| Character device | `/dev/lab2` |
| Major/minor | `240:0` |
| Shared buffer | `1024` bytes |
| procfs | `/proc/lab2_info` |
| sysfs | `/sys/class/lab2_class/lab2/` |

QEMU tests demonstrated successful reads/writes, including repeated messages `Message_1`, `Message_2`, and `Message_3`. The total successful device-open count increased from 0 to 10 during the recorded checks.

## Part B — procfs and sysfs (PASS)

The driver exposes operational statistics through `/proc/lab2_info` and read-only sysfs attributes `buffer_len`, `open_count`, and `last_data`. QEMU showed procfs statistics, including major 240, buffer size 1024, initial data length 0, and open count 0. The sysfs directory contained all three driver attributes.

## Part C — MTD, NAND simulator, JFFS2 (PASS)

### Simulated NAND configuration

| Property | Verified result |
|---|---|
| Device during this session | `/dev/mtd2` |
| Device name | `NAND simulator partition 0` |
| Type | `nand` |
| Size | `33,554,432` bytes (32 MiB) |
| Erase block | `16,384` bytes (16 KiB) |
| Page size | 512 bytes |
| OOB | 16 bytes |
| sysfs target | `/sys/devices/virtual/mtd/mtd2` |

**Hardware safety:** The host also exposes `/dev/mtd0` (`BIOS`) and `/dev/mtd1` (`BIOS1`) through its physical SPI flash controller. NEVER erase or write to these devices. MTD numbering is not stable; always re-identify the simulator before any destructive operation. The commands below describe the *verified session* and must not be blindly rerun against a future `mtd2`.

### Image creation

Three input files were created in `mtd/jffs2_data/`: `hello.txt`, `info.txt`, and `logs/boot.log`. The filesystem image was created with:

```bash
mkfs.jffs2 \
  --root="$HOME/embedded_lab2/mtd/jffs2_data" \
  --eraseblock=0x4000 \
  --output="$HOME/embedded_lab2/mtd/rootfs.jffs2" \
  --pad --no-cleanmarkers
```

The generated image was **16,384 bytes**. The verified simulator was erased (`flash_erase`) and programmed (`nandwrite`); the latter printed a warning about all-`0xff` blocks, but JFFS2 subsequently mounted and passed file read/write checks.

### Mount and persistence test

```bash
sudo modprobe jffs2
sudo mkdir -p /mnt/jffs2
sudo mount -t jffs2 mtd2 /mnt/jffs2
cat /mnt/jffs2/hello.txt
cat /mnt/jffs2/info.txt
```

Observed file contents:

```text
Hello from JFFS2!
Embedded Linux Lab02
```

A runtime file and an additional log entry were written to the mounted filesystem:

```text
runtime.txt: JFFS2 persistence test - LAB02
logs/boot.log appended: Runtime write test
```

After `sync`, `umount`, and mounting JFFS2 again, both changes were still present. **PASS: persistence across unmount/remount.** The RAM-backed simulator does **not** guarantee persistence across a host reboot or simulator unload.

## Part D — BusyBox init and module auto-load (PASS)

The initramfs contains:

```text
rootfs/initramfs/etc/inittab
rootfs/initramfs/etc/init.d/rcS
rootfs/initramfs/usr/bin/test_driver.sh
rootfs/initramfs/usr/bin/test_procfs.sh
```

`inittab` runs `/etc/init.d/rcS` as a `sysinit` action. `rcS` mounts pseudo filesystems and inserts `/lib/modules/6.1.90/lab2_driver.ko`, then creates `/dev/lab2` (major 240, minor 0).

The QEMU target was booted using:

```bash
qemu-system-arm \
  -M vexpress-a9 -cpu cortex-a9 -m 512M -smp 2 -nographic \
  -kernel "$HOME/embedded_lab1/output/zImage" \
  -dtb "$HOME/embedded_lab1/output/vexpress-v2p-ca9.dtb" \
  -initrd "$HOME/embedded_lab2/output/initramfs_lab2.cpio.gz" \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

Immediately after boot, *without manual `insmod`*, `lsmod` showed `lab2_driver`, `/dev/lab2` existed with permissions `crw-rw-rw-` and major/minor `240,0`, and procfs/sysfs were available. Writing and reading `LAB02 AUTOLOAD TEST` succeeded. `test_driver.sh` detected an already-loaded module and successfully exercised repeated read/write operations.

**Testing note:** The supplied `test_procfs.sh` invokes `insmod` unconditionally and should be updated to check `/proc/modules` before attempting a second load; its execution has not been recorded in the final boot log. Its printed `Test PASSED` line is not, by itself, an assertion-based test suite.

## Results

| Section | Scope | Result |
|---|---|---|
| A | Character device and ARM module | PASS |
| B | procfs/sysfs interfaces | PASS |
| C | NAND simulation, JFFS2 read/write, remount persistence | PASS |
| D | BusyBox startup, driver auto-load and basic device tests | PASS |

**Conclusion:** All four main LAB-02 sections were exercised successfully. The complete 20 character-driver extension exercises (Lab 01–20) extending this baseline are maintained separately under `assignments/driver-exercises/`.

## Character Driver Extension Exercises (20/20 PASS)

Each exercise is an independent extension of the LAB-02 baseline character driver (`/dev/lab2`), maintained under `assignments/driver-exercises/`:

| Exercise | Module Directory | Core Technical Feature | Status |
|---|---|---|---|
| Lab 01 | [`lab01_read_count`](../../assignments/driver-exercises/lab01_read_count/) | Read operation counter in sysfs | **PASS** |
| Lab 02 | [`lab02_write_count`](../../assignments/driver-exercises/lab02_write_count/) | Write operation counter in procfs | **PASS** |
| Lab 03 | [`lab03_clear_command`](../../assignments/driver-exercises/lab03_clear_command/) | Buffer clear command via `write()` | **PASS** |
| Lab 04 | [`lab04_max_write_limit`](../../assignments/driver-exercises/lab04_max_write_limit/) | Maximum write size enforcement (128 bytes) | **PASS** |
| Lab 05 | [`lab05_sysfs_buffer_size`](../../assignments/driver-exercises/lab05_sysfs_buffer_size/) | Read-only buffer capacity in sysfs | **PASS** |
| Lab 06 | [`lab06_sysfs_status`](../../assignments/driver-exercises/lab06_sysfs_status/) | Dynamic device buffer status (EMPTY / DATA) | **PASS** |
| Lab 07 | [`lab07_total_bytes_read`](../../assignments/driver-exercises/lab07_total_bytes_read/) | Cumulative transferred byte accounting (Read) | **PASS** |
| Lab 08 | [`lab08_total_bytes_written`](../../assignments/driver-exercises/lab08_total_bytes_written/) | Cumulative transferred byte accounting (Write) | **PASS** |
| Lab 09 | [`lab09_write_timestamp`](../../assignments/driver-exercises/lab09_write_timestamp/) | System uptime timestamp recording via jiffies | **PASS** |
| Lab 10 | [`lab10_sysfs_version`](../../assignments/driver-exercises/lab10_sysfs_version/) | Driver version string attribute in sysfs | **PASS** |
| Lab 11 | [`lab11_reset_stats`](../../assignments/driver-exercises/lab11_reset_stats/) | Operational statistics reset via sysfs store | **PASS** |
| Lab 12 | [`lab12_atomic_open_count`](../../assignments/driver-exercises/lab12_atomic_open_count/) | Lock-free concurrency control via `atomic_t` | **PASS** |
| Lab 13 | [`lab13_mutex_read_protection`](../../assignments/driver-exercises/lab13_mutex_read_protection/) | Mutex synchronization on `read()` callback | **PASS** |
| Lab 14 | [`lab14_buffer_append`](../../assignments/driver-exercises/lab14_buffer_append/) | Dynamic buffer concatenation via write append | **PASS** |
| Lab 15 | [`lab15_clear_newline`](../../assignments/driver-exercises/lab15_clear_newline/) | Newline-tolerant command parsing (`clear
`) | **PASS** |
| Lab 16 | [`lab16_sysfs_max_size`](../../assignments/driver-exercises/lab16_sysfs_max_size/) | Configurable maximum write boundary via sysfs | **PASS** |
| Lab 17 | [`lab17_sysfs_last_write_size`](../../assignments/driver-exercises/lab17_sysfs_last_write_size/) | Last write operation size tracking | **PASS** |
| Lab 18 | [`lab18_expanded_procfs`](../../assignments/driver-exercises/lab18_expanded_procfs/) | Comprehensive kernel diagnostic dashboard | **PASS** |
| Lab 19 | [`lab19_one_shot_read`](../../assignments/driver-exercises/lab19_one_shot_read/) | One-shot buffer read mode with replay protection | **PASS** |
| Lab 20 | [`lab20_ioctl_control`](../../assignments/driver-exercises/lab20_ioctl_control/) | Unlocked IOCTL hardware control interface | **PASS** |

## Git repository policy

Commit source, Makefile, init configuration, test scripts, and small reproducible test inputs. Exclude host/target kernel build products, `.ko` and `.o` files, copied BusyBox binaries, and full generated initramfs archives. Preserve executable permissions on shell scripts where needed.
