# Embedded Linux Systems (LOS201)

An end-to-end coursework and engineering repository demonstrating Linux kernel configuration, device driver architecture, filesystem persistence, systems debugging, and real-time computing on ARMv7-A Cortex-A9 hardware (QEMU `vexpress-a9`).

**Author:** Hà Gia Bảo  
**Institution:** FPT University  
**Contact:** `weliuliu123@gmail.com`  
**Repository:** [github.com/weliuliu123-byte/LOS](https://github.com/weliuliu123-byte/LOS)

---

## Technical Overview & Engineering Scope

This project covers the full life cycle of embedded Linux system development, structured into four core engineering pillars:

```text
+-------------------------------------------------------------------------+
|                        SYSTEM ARCHITECTURE LAYOUT                       |
+-------------------------------------------------------------------------+
|  USER SPACE APPLICATIONS                                                |
|  - BusyBox Utilities (ash, cat, echo, ls, mknod)                        |
|  - Real-Time Sensor Monitoring Daemon (sms_app: 4 POSIX threads)        |
|  - Userspace IOCTL Test Harnesses (test_ioctl, test_asgn1)              |
+-------------------------------------------------------------------------+
|  SYSTEM CALL INTERFACE (VFS & IOCTL)                                    |
|  - open()  read()  write()  ioctl()  close()                            |
+-------------------------------------------------------------------------+
|  KERNEL SUBSYSTEMS & CUSTOM DEVICE DRIVERS                              |
|  - LAB-02 Base Driver: /dev/lab2 (Major 240, mutex-locked shared buffer)|
|  - 20 Driver Extensions: sysfs/procfs telemetry, atomic_t, append mode  |
|  - Assignment 01: /dev/asgn1 (Major 241, IOCTL) & /dev/counter (Misc)   |
|  - Assignment 02: /dev/sms_sensor (Dynamic Major, Monotonic sampling)   |
|  - Procfs & Sysfs: /proc/lab2_info, /proc/sms_stats, /sys/class/...     |
+-------------------------------------------------------------------------+
|  MEMORY & FILESYSTEM LAYER                                              |
|  - Rootfs: BusyBox initramfs (CPIO + Gzip archive)                      |
|  - Flash Memory Subsystem: MTD / nandsim (32MB) with JFFS2              |
+-------------------------------------------------------------------------+
|  HARDWARE & EMULATION PLATFORM                                          |
|  - Emulated Platform: QEMU ARM Versatile Express (vexpress-a9)          |
|  - Core: ARMv7-A Cortex-A9 MPCore @ 512MB RAM                           |
|  - Kernel Target: Linux 6.1.90 LTS (ARM EABI5)                          |
+-------------------------------------------------------------------------+
```

---

## Development Environment Specifications

| Category | Specification / Toolchain |
|---|---|
| **Development Host** | Ubuntu 24.04 LTS (x86_64), Linux Kernel 6.8 |
| **Cross-Compilation Toolchain** | `arm-linux-gnueabihf-gcc` (GNU GCC 13.3.0, EABI5 hard-float) |
| **Virtual Hardware Platform** | QEMU System Emulator `qemu-system-arm` (v8.2+) |
| **Target Machine Profile** | ARM Versatile Express Cortex-A9 (`vexpress-a9`), 512MB RAM |
| **Target Operating System** | Linux Kernel `6.1.90` Long Term Support (LTS) |
| **Device Tree Blob (DTB)** | `vexpress-v2p-ca9.dtb` |
| **Target Root Filesystem** | BusyBox `1.35.0` compiled with static linkage (`CONFIG_STATIC=y`) |
| **Driver Binary Format** | ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV) |

---

## Coursework Milestones & Verification Summary

| Module | Technical Scope | Target Interface | Verification Status |
|---|---|---|:---:|
| **[LAB-01](labs/lab01/)** | Kernel build, BusyBox rootfs, U-Boot, QEMU boot | Direct QEMU & Serial Console | **PASS** |
| **[LAB-02](labs/lab02/)** | Character driver, procfs/sysfs, MTD/JFFS2 flash | `/dev/lab2`, `/proc/lab2_info` | **PASS** |
| **[Driver Exercises](assignments/driver-exercises/)** | 20 Hands-on Linux Character Driver Extensions | Labs 01–20 independent modules | **20/20 PASS** |
| **[Assignment 01](assignments/assignment01/)** | Advanced IOCTL driver & lock-free misc device | `/dev/asgn1`, `/dev/counter` | **PASS** |
| **[Assignment 02](assignments/assignment02/)** | Industrial Sensor Monitoring System (SMS) | `/dev/sms_sensor`, `sms_app` | **PASS** |
| **[LAB-03](labs/lab03/)** | GDB remote triage, Valgrind, perf & cyclictest | Diagnostic suite & RT latency | **PASS** |

---

## Repository Structure

```text
embedded-linux-systems/
├── README.md                                # Master repository documentation
├── CONTRIBUTING.md                          # Engineering workflow and git conventions
├── LICENSE                                  # Open-source license (MIT)
│
├── labs/
│   ├── lab01/                               # LAB-01: Kernel Build & System Boot
│   │   ├── README.md                        # In-depth kernel boot documentation
│   │   ├── boot/
│   │   │   └── README.md                    # Direct QEMU vs U-Boot boot notes
│   │   ├── configs/                         # Kernel, U-Boot and BusyBox configs
│   │   │   ├── kernel.config
│   │   │   ├── busybox.config
│   │   │   └── uboot.config
│   │   └── rootfs/initramfs/                # Rootfs skeleton (inittab, rcS)
│   │
│   ├── lab02/                               # LAB-02: Device Driver & Storage
│   │   ├── README.md                        # Full report on Parts A, B, C, D
│   │   ├── driver/                          # Baseline character driver (/dev/lab2)
│   │   │   ├── Makefile
│   │   │   └── lab2_driver.c
│   │   ├── mtd/jffs2_data/                  # Persistent flash image source files
│   │   └── rootfs/initramfs/                # Automated init scripts (test_driver.sh)
│   │
│   └── lab03/                               # LAB-03: Debugging & Profiling
│       ├── README.md                        # Analysis of GDB, Valgrind, perf, cyclictest
│       ├── apps/                            # Buggy, leaky, and profiling targets
│       ├── bao_cao/                         # Technical laboratory report
│       └── logs/                            # Real verification logs & latency plots
│
└── assignments/
    ├── assignment01/                        # Assignment 01: Advanced Driver Architecture
    │   ├── README.md                        # IOCTL interface & Misc device guide
    │   └── task1_ioctl/                     # Character driver with 4 IOCTL commands
    │       ├── asgn1_driver.c
    │       ├── asgn1_ioctl.h
    │       ├── Makefile
    │       └── test_asgn1.c
    │
    ├── assignment02/                        # Assignment 02: Sensor Monitoring System
    │   ├── README.md                        # Real-time multi-threaded daemon guide
    │   ├── driver/                          # Simulated sensor kernel module
    │   │   ├── sms_sensor_driver.c
    │   │   ├── sms_sensor.h
    │   │   └── Makefile
    │   └── tests/                           # Userspace ioctl validation harness
    │       └── test_ioctl.c
    │
    └── driver-exercises/                    # 20 Hands-on Driver Extension Exercises
        ├── README.md                        # Complete catalog and test matrix
        ├── lab01_read_count/                # Read operation counter via sysfs
        ├── lab02_write_count/               # Write operation counter via procfs
        ├── lab03_clear_command/             # Buffer clear command via write()
        ├── lab04_max_write_limit/           # Maximum write size enforcement (128B)
        ├── lab05_sysfs_buffer_size/         # Static buffer capacity exposure
        ├── lab06_sysfs_status/              # Dynamic status attribute (EMPTY / DATA)
        ├── lab07_total_bytes_read/          # Cumulative transferred byte accounting (Read)
        ├── lab08_total_bytes_written/       # Cumulative transferred byte accounting (Write)
        ├── lab09_write_timestamp/           # Last write jiffies timestamp in procfs
        ├── lab10_sysfs_version/             # Driver version attribute
        ├── lab11_reset_stats/               # Operational statistics reset handler
        ├── lab12_atomic_open_count/         # Lock-free concurrency with atomic_t
        ├── lab13_mutex_read_protection/     # Read callback mutual exclusion
        ├── lab14_buffer_append/             # Dynamic buffer concatenation
        ├── lab15_clear_newline/             # Newline-tolerant command parser
        ├── lab16_sysfs_max_size/            # Configurable write limit via sysfs
        ├── lab17_sysfs_last_write_size/     # Last write byte volume tracking
        ├── lab18_expanded_procfs/           # Multi-metric diagnostic dashboard
        ├── lab19_one_shot_read/             # One-shot read mode with replay prevention
        └── lab20_ioctl_control/             # Unlocked IOCTL hardware control plane
```

---

## Quick Start & Verification Instructions

### 1. Cross-Compiling Kernel Modules

Ensure the cross-compiler is available in `$PATH`:

```bash
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-
```

To compile any module (e.g., baseline driver or an extension exercise):

```bash
cd assignments/driver-exercises/lab01_read_count
make clean && make
```

Check module ABI compatibility:

```bash
file lab2_driver.ko
# Expected: ELF 32-bit LSB relocatable, ARM, EABI5 version 1 (SYSV)
modinfo lab2_driver.ko | grep vermagic
# Expected: 6.1.90 SMP mod_unload ARMv7 p2v8
```

### 2. Launching Emulated ARM Target in QEMU

Execute the QEMU launcher with serial console output redirected to terminal:

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab2/output/initramfs_lab2.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

Inside QEMU shell:

```sh
# Verify device node and load driver
ls -la /dev/lab2
cat /proc/lab2_info
cat /sys/class/lab2_class/lab2/buffer_len
```

To exit QEMU at any time, press `Ctrl+A` then `X`.

---

## Engineering Commit Discipline

The git history strictly follows **Conventional Commits**:

- `feat`: Implementation of kernel drivers, ioctl handlers, or userspace applications.
- `fix`: Remediation of memory leaks, race conditions, or off-by-one pointer errors.
- `docs`: Architecture blueprints, verification matrices, and technical lab reports.
- `test`: Automated test harnesses, Valgrind logs, and real-time latency samples.
- `build`: Cross-compilation Kbuild Makefiles and configuration recipes.
- `chore`: Repository layout maintenance and ignore rules.

Every commit is cryptographically signed and authored by:
`Hà Gia Bảo <weliuliu123@gmail.com>`
