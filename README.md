# Embedded Linux Systems

Collection of labs, assignments, and Linux driver exercises for the **Embedded Linux Systems** course.

Main topics:

- ARM Linux with QEMU
- Linux kernel modules
- Character device drivers
- procfs / sysfs
- ioctl
- BusyBox initramfs
- MTD / JFFS2
- Driver synchronization and debugging

---

## Development Environment

| Component | Configuration |
|---|---|
| Host | Ubuntu 24.04.5 LTS x86_64 |
| Target | ARMv7 Cortex-A9 |
| Emulator | QEMU `vexpress-a9` |
| Kernel | Linux `6.1.90` |
| Cross Compiler | `arm-linux-gnueabihf-` GCC 13.3.0 |
| RootFS | BusyBox 1.35.0 + initramfs |

---

## Repository Structure

```text
embedded-linux-systems/
├── labs/
│   ├── lab01/
│   │   ├── README.md
│   │   ├── boot/
│   │   ├── configs/
│   │   └── rootfs/
│   ├── lab02/
│   │   ├── README.md
│   │   ├── driver/
│   │   ├── mtd/
│   │   └── rootfs/
│   └── lab03/
│       ├── README.md
│       ├── apps/
│       ├── bao_cao/
│       └── logs/
│
├── assignments/
│   ├── assignment01/
│   │   ├── README.md
│   │   └── task1_ioctl/
│   ├── assignment01_device_driver/
│   │   └── README.md
│   ├── assignment02/
│   │   ├── README.md
│   │   ├── driver/
│   │   └── tests/
│   │
│   └── driver-exercises/
│       ├── lab01_read_count/
│       ├── lab02_write_count/
│       ├── lab03_clear_command/
│       ├── lab04_max_write_limit/
│       ├── lab05_sysfs_buffer_size/
│       ├── lab06_sysfs_status/
│       ├── lab07_total_bytes_read/
│       ├── lab08_total_bytes_written/
│       ├── lab09_write_timestamp/
│       ├── lab10_sysfs_version/
│       ├── lab11_reset_stats/
│       ├── lab12_atomic_open_count/
│       ├── lab13_mutex_read_protection/
│       ├── lab14_buffer_append/
│       ├── lab15_clear_newline/
│       ├── lab16_sysfs_max_size/
│       ├── lab17_sysfs_last_write_size/
│       ├── lab18_expanded_procfs/
│       ├── lab19_one_shot_read/
│       └── lab20_ioctl_control/
│
└── resources/
```

---

## Progress

| Work | Status |
|---|---|
| LAB-01 | **PASS — ARM Linux direct QEMU boot** |
| LAB-02 | **PASS — Parts A–D completed** |
| Driver Exercises 01–20 | **20/20 PASS — All 20 Extension Exercises Completed** |
| Assignment-01 | **PASS — Tasks 1–4 completed** |
| Assignment-02 Kernel Driver | **PASS — baseline completed** |
| Assignment-02 Userspace App | **PASS — multi-threaded daemon** |
| LAB-03 Debugging & Profiling | **PASS — GDB, Valgrind, perf, cyclictest** |

---

# LAB-01

Verified direct ARM Linux boot using:

```text
QEMU
 ├── zImage
 ├── vexpress-v2p-ca9.dtb
 └── BusyBox initramfs
```

Target kernel:

```text
Linux 6.1.90 ARMv7
```

---

# LAB-02

Completed:

- character driver `/dev/lab2`
- `open/read/write/release`
- mutex protection
- `/proc/lab2_info`
- sysfs attributes
- BusyBox module auto-load
- MTD / JFFS2 testing
- Complete 20-exercise extension suite

Baseline:

```text
Module      : lab2_driver.ko
Device      : /dev/lab2
Major/minor : 240 / 0
Buffer      : 1024 bytes
```

---

# Driver Exercises

Each exercise is an independent modification of the LAB-02 baseline character driver.

Completed (20/20 PASS):

```text
Lab 01  read_count sysfs attribute                   PASS
Lab 02  write_count procfs statistic                PASS
Lab 03  clear command via write()                   PASS
Lab 04  128-byte maximum write size limit           PASS
Lab 05  buffer_size sysfs attribute                 PASS
Lab 06  status sysfs attribute (EMPTY / DATA)       PASS
Lab 07  total_bytes_read accounting                 PASS
Lab 08  total_bytes_written accounting              PASS
Lab 09  write timestamp with jiffies in procfs      PASS
Lab 10  driver version sysfs attribute              PASS
Lab 11  reset_stats store callback                  PASS
Lab 12  atomic_t open_count for lockless safety     PASS
Lab 13  mutex synchronization on read()             PASS
Lab 14  dynamic buffer append                       PASS
Lab 15  newline-tolerant clear command parsing      PASS
Lab 16  read-write max_size sysfs attribute         PASS
Lab 17  last_write_size sysfs tracking              PASS
Lab 18  expanded procfs diagnostic dashboard        PASS
Lab 19  one-shot buffer read with replay protection PASS
Lab 20  unlocked_ioctl (GET_BUFFER_LEN / CLEAR)     PASS
```

All 20 exercises have passed verification in QEMU ARM Cortex-A9 running Linux 6.1.90.

---

# Assignment-02 — Sensor Monitoring System

Current completed baseline:

```text
D1  Module init/exit                   PASS
D2  Character device                  PASS
D3  Sensor CSV read                   PASS
D4  /proc/sms_stats                   PASS
D5  SMS_SET_INTERVAL ioctl            PASS
```

Implemented driver features:

- `/dev/sms_sensor`
- dynamic major number
- random temperature `15–45 °C`
- random humidity `30–90 %`
- monotonic timestamp
- CSV sensor output
- partial-read support
- `file->private_data`
- `/proc/sms_stats`
- `read_count`
- last temperature / humidity
- driver uptime
- mutex synchronization
- default sampling interval `100 ms`
- `SMS_SET_INTERVAL`
- `-EAGAIN` when reading too early
- ARM userspace ioctl test

Example sensor output:

```text
61719,34.95,62.75
```

Example procfs output:

```text
read_count: 4
last_temperature: 42.47
last_humidity: 30.69
uptime_seconds: 89
```

---

## Assignment-02 Build

```bash
cd ~/embedded_asm2/driver
make clean
make
```

Verify:

```bash
file sms_sensor_driver.ko
modinfo sms_sensor_driver.ko | grep vermagic
```

Expected target:

```text
ARM 32-bit
Linux 6.1.90 ARMv7
```

---

## Assignment-02 QEMU Boot

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_asm2/output/initramfs_asgn2.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

Inside QEMU:

```bash
insmod /lib/modules/6.1.90/sms_sensor_driver.ko
grep sms_sensor /proc/devices
```

Create the device using the returned major:

```bash
mknod /dev/sms_sensor c <major> 0
chmod 666 /dev/sms_sensor
```

Test:

```bash
cat /dev/sms_sensor
cat /proc/sms_stats
test_ioctl
```

Cleanup:

```bash
rmmod sms_sensor_driver
rm -f /dev/sms_sensor
```

---

## Git Workflow

```bash
cd ~/Github_embedded_system/embedded-linux-systems

git status
git add <file>
git diff --cached --name-only
git commit -m "<type>: <message>"
git push origin main
```

Commit convention:

```text
feat     new feature
fix      bug fix
docs     documentation
test     tests
build    Makefile/build
refactor code restructuring
chore    maintenance
```

---

## Notes

- Do not load ARM `.ko` modules into the x86_64 host kernel.
- Do not commit unnecessary build artifacts such as `.ko`, `.o`, `.cmd`, `Module.symvers`, or generated initramfs files.
- Keep completed baselines unchanged.
- Test changes in QEMU before committing them.

---

## Author

**Hà Gia Bảo**  
FPT University  
Embedded Linux Systems (LOS201)
