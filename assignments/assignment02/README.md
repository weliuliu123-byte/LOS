# Assignment 02 — Sensor Monitoring System

## Overview

This project is part of the **Embedded Linux Systems** course.

Assignment 02 implements the kernel-space foundation of a **Sensor Monitoring System (SMS)** running on an ARMv7 Linux target under QEMU.

The current completed implementation contains:

- an ARM Linux character device driver,
- simulated temperature and humidity sensor data,
- monotonic timestamps,
- per-open sensor data handling,
- partial-read and EOF support,
- driver statistics through procfs,
- synchronization using mutexes,
- configurable sampling interval through `ioctl()`,
- `-EAGAIN` handling when the sensor is read before the configured interval expires,
- a userspace ARM test program for the ioctl interface.

The current kernel driver implementation has been functionally tested on QEMU with Linux **6.1.90**.

---

# 1. Current Status

## Kernel Driver

Completed and tested:

- [x] Kernel module initialization and cleanup
- [x] Dynamic character-device registration
- [x] `/dev/sms_sensor`
- [x] `open()`
- [x] `read()`
- [x] `release()`
- [x] Random temperature generation
- [x] Random humidity generation
- [x] Monotonic timestamp generation
- [x] CSV sensor output
- [x] Partial-read support
- [x] Correct EOF behavior
- [x] Per-open file context using `file->private_data`
- [x] `/proc/sms_stats`
- [x] `read_count`
- [x] Last temperature
- [x] Last humidity
- [x] Driver uptime
- [x] Mutex protection for shared statistics
- [x] `SMS_SET_INTERVAL` ioctl
- [x] Default sampling interval of 100 ms
- [x] Minimum sampling interval enforcement
- [x] `-EAGAIN` when the next sample is requested too early
- [x] Userspace ioctl test application
- [x] Clean `insmod` / `rmmod`
- [x] ARMv7 module build with correct kernel vermagic

The userspace multi-threaded `sms_app` is not included in the current completed baseline yet.

---

# 2. Development Environment

The project has been tested with the following environment:

| Component | Configuration |
|---|---|
| Development host | Ubuntu Linux x86_64 |
| Cross compiler | `arm-linux-gnueabihf-gcc 13.3.0` |
| Target architecture | ARMv7 |
| QEMU machine | `vexpress-a9` |
| QEMU CPU | Cortex-A9 |
| QEMU RAM | 512 MB |
| QEMU CPUs | 2 |
| Target kernel | Linux 6.1.90 |
| Kernel image | `zImage` |
| Device Tree | `vexpress-v2p-ca9.dtb` |
| Root filesystem | BusyBox initramfs |

Important target paths:

```text
Kernel source:
~/embedded_lab1/kernel/linux-6.1.90/

Kernel image:
~/embedded_lab1/output/zImage

Device Tree:
~/embedded_lab1/output/vexpress-v2p-ca9.dtb

Assignment workspace:
~/embedded_asm2/

Assignment initramfs:
~/embedded_asm2/output/initramfs_asgn2.cpio.gz
```

---

# 3. Repository Structure

Current Assignment 02 source layout:

```text
assignments/assignment02/
├── driver/
│   ├── Makefile
│   ├── sms_sensor.h
│   └── sms_sensor_driver.c
│
└── tests/
    └── test_ioctl.c
```

Development workspace:

```text
~/embedded_asm2/
├── app/
├── demo/
├── docs/
├── driver/
│   ├── Makefile
│   ├── sms_sensor.h
│   └── sms_sensor_driver.c
├── logs/
├── output/
│   └── initramfs_asgn2.cpio.gz
├── rootfs/
│   └── initramfs/
└── tests/
    ├── test_ioctl.c
    └── test_ioctl
```

Generated build artifacts such as `.ko`, `.o`, `.cmd`, `Module.symvers`, `modules.order`, initramfs images, and ARM test binaries are not intended to be tracked in Git.

---

# 4. Driver Architecture

The current driver provides two userspace interfaces:

```text
                     USER SPACE

          /dev/sms_sensor
                 |
                 | open/read/ioctl
                 v
      +-------------------------+
      | sms_sensor_driver.ko    |
      |                         |
      | Sensor generation       |
      | Sampling control        |
      | Statistics              |
      +------------+------------+
                   |
                   v
           /proc/sms_stats
```

The device interface is used for sensor data:

```text
/dev/sms_sensor
```

The monitoring interface is:

```text
/proc/sms_stats
```

---

# 5. Sensor Data Format

Each successfully generated sensor sample has the format:

```text
<timestamp_ms>,<temperature>,<humidity>
```

Example:

```text
61719,34.95,62.75
```

Meaning:

```text
timestamp_ms = 61719 ms
temperature  = 34.95 °C
humidity     = 62.75 %
```

Temperature range:

```text
15.00 °C <= temperature <= 45.00 °C
```

Humidity range:

```text
30.00 % <= humidity <= 90.00 %
```

The kernel driver does not use floating-point arithmetic.

Values are stored internally as fixed-point integers.

Example:

```text
34.95 °C
```

is stored as:

```text
3495
```

and formatted as:

```c
value / 100
value % 100
```

---

# 6. Per-Open Context

Each successful `open()` allocates a private context:

```c
struct sms_file_ctx {
    char data[SENSOR_BUF_SIZE];
    size_t data_len;
};
```

The context is stored in:

```c
file->private_data
```

This guarantees that one opened file descriptor continues reading the same sensor sample even when the sample is consumed through multiple partial reads.

Example:

```bash
dd if=/dev/sms_sensor bs=5
```

A 19-byte sample may be returned as:

```text
5 bytes
5 bytes
5 bytes
4 bytes
```

but only one sensor sample is generated.

The context is released in `release()` using `kfree()`.

---

# 7. Driver Statistics

The driver maintains global statistics:

```text
read_count
last_temperature
last_humidity
driver uptime
```

They are exposed through:

```text
/proc/sms_stats
```

Example:

```text
read_count: 7
last_temperature: 32.22
last_humidity: 75.74
uptime_seconds: 166
```

`read_count` represents the number of sensor samples generated.

It does not increase once for every low-level partial `read()` callback.

For example:

```bash
dd if=/dev/sms_sensor bs=5
```

may execute multiple driver `read()` operations, but the counter increases only once because only one new sensor sample is generated.

---

# 8. Synchronization

The driver currently uses two mutexes.

## Statistics Mutex

```c
stats_lock
```

Protects shared statistics:

```text
read_count
last_temp_x100
last_hum_x100
```

This prevents `/proc/sms_stats` from reading partially updated statistics while another context is generating a new sample.

## Sampling Mutex

```c
sample_lock
```

Protects:

```text
interval_ms
last_sample_ms
sampling decision
```

This prevents multiple readers from simultaneously passing the interval check and generating samples too close together.

---

# 9. Sampling Interval

The default minimum sampling interval is:

```text
100 ms
```

It is defined in:

```c
SMS_DEFAULT_INTERVAL_MS
```

The userspace application may modify the interval using:

```c
SMS_SET_INTERVAL
```

Example:

```text
500 ms
```

After a successful sample:

```text
last_sample_ms = current timestamp
```

For the next request, the driver calculates:

```text
elapsed_ms = current_time - last_sample_ms
```

If:

```text
elapsed_ms < interval_ms
```

the driver returns:

```text
-EAGAIN
```

Otherwise a new sample is generated.

---

# 10. IOCTL Interface

The shared header is:

```text
driver/sms_sensor.h
```

The ioctl command is defined with:

```c
SMS_SET_INTERVAL
```

The same header is used by:

```text
kernel driver
userspace ioctl test
future sms_app
```

This ensures the kernel and userspace use the same ioctl command definition.

---

# 11. Build the Kernel Module

Enter the driver directory:

```bash
cd ~/embedded_asm2/driver
```

Clean previous build output:

```bash
make clean
```

Build:

```bash
make
```

Expected final build stages include:

```text
CC [M]  sms_sensor_driver.o
MODPOST Module.symvers
CC [M]  sms_sensor_driver.mod.o
LD [M]  sms_sensor_driver.ko
```

Verify architecture:

```bash
file sms_sensor_driver.ko
```

Expected:

```text
ELF 32-bit LSB relocatable, ARM, EABI5
```

Verify kernel compatibility:

```bash
modinfo sms_sensor_driver.ko | grep -E 'version|license|vermagic'
```

Expected to include:

```text
license:        GPL
vermagic:       6.1.90 SMP mod_unload ARMv7 p2v8
```

---

# 12. Build the IOCTL Test Application

Enter:

```bash
cd ~/embedded_asm2/tests
```

Compile for ARM:

```bash
arm-linux-gnueabihf-gcc \
  -static \
  -Wall \
  -Wextra \
  -O2 \
  -o test_ioctl \
  test_ioctl.c
```

Verify:

```bash
file test_ioctl
```

Expected:

```text
ELF 32-bit LSB executable, ARM, EABI5
```

The binary is statically linked so it can run directly in the BusyBox initramfs.

---

# 13. Copy Files into the Initramfs

Copy the kernel module:

```bash
sudo cp ~/embedded_asm2/driver/sms_sensor_driver.ko \
  ~/embedded_asm2/rootfs/initramfs/lib/modules/6.1.90/
```

Copy the ioctl test program:

```bash
sudo cp ~/embedded_asm2/tests/test_ioctl \
  ~/embedded_asm2/rootfs/initramfs/usr/bin/
```

Ensure it is executable:

```bash
sudo chmod +x \
  ~/embedded_asm2/rootfs/initramfs/usr/bin/test_ioctl
```

---

# 14. Rebuild the Initramfs

Enter the initramfs root:

```bash
cd ~/embedded_asm2/rootfs/initramfs
```

Create the archive:

```bash
sudo sh -c 'find . -print0 | cpio --null -o --format=newc | gzip -9 > /home/wis/embedded_asm2/output/initramfs_asgn2.cpio.gz'
```

The command normally prints only the number of generated blocks.

Verify:

```bash
ls -lh ~/embedded_asm2/output/initramfs_asgn2.cpio.gz
```

---

# 15. Boot QEMU

Run:

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

Expected target kernel:

```bash
uname -r
```

```text
6.1.90
```

Verify architecture:

```bash
uname -a
```

The target should report ARMv7.

---

# 16. Load the Driver

Inside QEMU:

```bash
insmod /lib/modules/6.1.90/sms_sensor_driver.ko
```

Expected kernel messages:

```text
sms_sensor: driver initialized
sms_sensor: registered with major <dynamic_major>
sms_sensor: default sampling interval 100 ms
sms_sensor: /proc/sms_stats created
```

The following message is expected for an externally built module:

```text
loading out-of-tree module taints kernel
```

This is not a functional driver error.

---

# 17. Find the Dynamic Major Number

Run:

```bash
grep sms_sensor /proc/devices
```

Example:

```text
248 sms_sensor
```

The major number is dynamically allocated and must not be assumed to always be `248`.

Always check `/proc/devices` after loading the module.

---

# 18. Create the Device Node

If the assigned major is `248`:

```bash
mknod /dev/sms_sensor c 248 0
```

Set permissions:

```bash
chmod 666 /dev/sms_sensor
```

Verify:

```bash
ls -l /dev/sms_sensor
```

Example:

```text
crw-rw-rw- 1 root root 248, 0 /dev/sms_sensor
```

Meaning:

```text
c   = character device
248 = major number
0   = minor number
```

The major connects the device node to the registered kernel driver.

---

# 19. Basic Sensor Test

Read one sample:

```bash
cat /dev/sms_sensor
```

Example:

```text
61719,34.95,62.75
```

Kernel log:

```text
sms_sensor: device opened
sms_sensor: generated sample timestamp=61719 temp=34.95 humidity=62.75
sms_sensor: read 18 bytes
sms_sensor: device closed
```

Read more samples:

```bash
cat /dev/sms_sensor
sleep 1
cat /dev/sms_sensor
sleep 1
cat /dev/sms_sensor
```

Each successful new open/read sequence generates one new sensor sample.

---

# 20. Test Partial Reads

Run:

```bash
dd if=/dev/sms_sensor bs=5 2>/dev/null
```

A sample may be returned through multiple `read()` callbacks.

Example kernel log:

```text
sms_sensor: read 5 bytes
sms_sensor: read 5 bytes
sms_sensor: read 5 bytes
sms_sensor: read 4 bytes
```

The complete userspace data still belongs to one sample.

This test verifies:

- file offset handling,
- partial-read behavior,
- EOF handling,
- per-open private context,
- one generated sample per open sequence.

---

# 21. Test `/proc/sms_stats`

Before reading the device:

```bash
cat /proc/sms_stats
```

Expected initial values:

```text
read_count: 0
last_temperature: 0.00
last_humidity: 0.00
uptime_seconds: <value>
```

Read a sensor sample:

```bash
cat /dev/sms_sensor
```

Then:

```bash
cat /proc/sms_stats
```

Example:

```text
read_count: 1
last_temperature: 30.37
last_humidity: 32.93
uptime_seconds: 76
```

The last temperature and humidity values must match the most recently generated sample.

---

# 22. Verify `read_count` with Partial Reads

Check current count:

```bash
cat /proc/sms_stats
```

Example:

```text
read_count: 6
```

Run:

```bash
dd if=/dev/sms_sensor bs=5 2>/dev/null
```

Then:

```bash
cat /proc/sms_stats
```

Expected:

```text
read_count: 7
```

Even if the driver executed several partial `read()` callbacks, only one new sensor sample was generated.

---

# 23. Verify Driver Uptime

Run:

```bash
cat /proc/sms_stats
```

Then:

```bash
sleep 5
```

And:

```bash
cat /proc/sms_stats
```

`uptime_seconds` must increase.

The value represents time since the driver module was loaded, not total Linux system uptime.

---

# 24. Procfs Permissions

Verify:

```bash
ls -l /proc/sms_stats
```

Expected:

```text
-r--r--r--
```

The statistics file is read-only.

Attempting to write:

```bash
echo test > /proc/sms_stats
```

must fail.

---

# 25. Test IOCTL Sampling Interval

Run:

```bash
test_ioctl
```

The test program performs the following sequence:

```text
1. Open /dev/sms_sensor
2. Set interval to 500 ms using SMS_SET_INTERVAL
3. Close configuration fd
4. Read first sample
5. Immediately request another sample
6. Expect EAGAIN
7. Sleep 550 ms
8. Read again
9. Expect success
```

Expected output is similar to:

```text
=== SMS IOCTL TEST ===

Interval configured to 500 ms

Sample #1:
READ: 97678,24.24,49.52

Sample #2 immediately:
READ: EAGAIN (sensor not ready)

Waiting 550 ms...

Sample #3:
READ: 98246,42.47,30.69

=== TEST COMPLETE ===
```

Kernel log should include:

```text
sms_sensor: sampling interval set to 500 ms
```

and for the rejected request:

```text
sms_sensor: sample not ready (elapsed=2 ms interval=500 ms)
```

The exact elapsed time is system-dependent.

---

# 26. Verify Statistics After the IOCTL Test

If the counter was `0` before `test_ioctl`, the expected result is:

```bash
cat /proc/sms_stats
```

```text
read_count: 2
```

Reason:

```text
Sample #1        -> generated -> +1
Immediate sample -> EAGAIN    -> +0
Sample #3        -> generated -> +1
```

Therefore:

```text
read_count = 2
```

If samples were read before the test, the final count will include those earlier successful samples.

---

# 27. Unload the Driver

Remove the module:

```bash
rmmod sms_sensor_driver
```

Expected:

```text
sms_sensor: driver exited
```

Verify procfs removal:

```bash
ls /proc/sms_stats
```

Expected:

```text
No such file or directory
```

Verify character-device registration removal:

```bash
grep sms_sensor /proc/devices
```

There should be no output.

Because `/dev/sms_sensor` was created manually with `mknod`, remove it manually:

```bash
rm -f /dev/sms_sensor
```

---

# 28. Verified Functional Test Results

The current baseline has been tested successfully for the following behaviors.

## Module Lifecycle

```text
insmod -> PASS
lsmod  -> PASS
rmmod  -> PASS
```

No kernel oops was observed during the recorded tests.

## Character Device

Dynamic registration was verified.

Example:

```text
248 sms_sensor
```

The exact major number may change between boots.

## Sensor Read

Verified example:

```text
128325,30.82,73.19
133315,30.70,36.81
135131,39.62,63.87
```

## Partial Read

Verified with:

```bash
dd if=/dev/sms_sensor bs=5
```

A sample was returned in several chunks without creating additional samples.

## Procfs

Verified output included:

```text
read_count: 7
last_temperature: 32.22
last_humidity: 75.74
uptime_seconds: 166
```

## Sampling Interval

The default interval was verified as:

```text
100 ms
```

The ioctl test successfully changed it to:

```text
500 ms
```

An immediate second sample request produced:

```text
-EAGAIN
```

After waiting longer than the configured interval, the next request succeeded.

---

# 29. Debugging History

Several issues were identified and fixed during development.

## Missing `MODULE_LICENSE`

Initial build failed with:

```text
ERROR: modpost: missing MODULE_LICENSE()
```

The module metadata was corrected by ensuring:

```c
MODULE_LICENSE("GPL");
```

was present in the compiled source.

---

## `ktime_get_ms()` Build Error

The original implementation attempted to use:

```c
ktime_get_ms()
```

but the current kernel build environment reported an implicit declaration error.

The implementation was changed to:

```c
div_u64(ktime_get_ns(), 1000000)
```

using:

```c
#include <linux/math64.h>
```

This provides millisecond timestamps while remaining compatible with the ARMv7 Linux 6.1.90 build environment used by this project.

---

## Incorrect Device Major During Manual `mknod`

At one stage the driver registered:

```text
248 sms_sensor
```

but `/dev/sms_sensor` was accidentally created with major `240`.

The incorrect node was removed:

```bash
rm -f /dev/sms_sensor
```

and recreated using the actual dynamically allocated major:

```bash
mknod /dev/sms_sensor c 248 0
```

The correct procedure is always:

```bash
grep sms_sensor /proc/devices
```

before creating the node.

---

## Device Nodes During Rootfs Copy

Copying the LAB-02 initramfs as a normal user failed for special files such as:

```text
/dev/null
/dev/console
/dev/zero
/dev/random
```

because creating device nodes requires elevated privileges.

The rootfs was copied using appropriate privileges before rebuilding the initramfs.

---

# 30. Important Design Notes

## Dynamic Major Number

The driver calls:

```c
register_chrdev(0, ...)
```

Therefore the kernel selects an available major number.

Do not hardcode:

```text
248
```

Always obtain the current value from:

```bash
grep sms_sensor /proc/devices
```

---

## Kernel Fixed-Point Representation

Floating-point operations are avoided in kernel space.

Temperature and humidity values are represented in hundredths.

Examples:

```text
30.37 -> 3037
75.74 -> 7574
```

---

## `file->private_data`

Each successful `open()` receives its own `sms_file_ctx`.

This ensures that partial reads from one file descriptor continue reading the same generated sample.

---

## Global Driver State

The following information belongs to the entire driver rather than one file descriptor:

```text
read_count
last temperature
last humidity
driver start time
sampling interval
last sample timestamp
```

Mutexes protect shared state when multiple users access the driver concurrently.

---

# 31. Current Project Boundary

This README documents only functionality that has already been implemented and functionally tested.

The following Assignment 02 components are not yet documented here as completed:

```text
Sensor Thread
Processor Thread
Alert Thread
Logger Thread
Circular shared buffer
POSIX alarm queue
POSIX log queue
Command-line options
SIGINT graceful shutdown
Valgrind analysis
GDB session
strace analysis
perf analysis
SCHED_FIFO comparison
jitter statistics
final report
demo video
```

They will be added only after implementation and verification.

---

# 32. Quick Run Reference

Build driver:

```bash
cd ~/embedded_asm2/driver
make clean
make
```

Build ioctl test:

```bash
cd ~/embedded_asm2/tests

arm-linux-gnueabihf-gcc \
  -static \
  -Wall \
  -Wextra \
  -O2 \
  -o test_ioctl \
  test_ioctl.c
```

Copy artifacts:

```bash
sudo cp ~/embedded_asm2/driver/sms_sensor_driver.ko \
  ~/embedded_asm2/rootfs/initramfs/lib/modules/6.1.90/

sudo cp ~/embedded_asm2/tests/test_ioctl \
  ~/embedded_asm2/rootfs/initramfs/usr/bin/
```

Pack initramfs:

```bash
cd ~/embedded_asm2/rootfs/initramfs

sudo sh -c 'find . -print0 | cpio --null -o --format=newc | gzip -9 > /home/wis/embedded_asm2/output/initramfs_asgn2.cpio.gz'
```

Boot:

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

Create `/dev/sms_sensor` using the returned major:

```bash
mknod /dev/sms_sensor c <major> 0
chmod 666 /dev/sms_sensor
```

Basic tests:

```bash
cat /dev/sms_sensor
cat /proc/sms_stats
test_ioctl
cat /proc/sms_stats
```

Cleanup:

```bash
rmmod sms_sensor_driver
rm -f /dev/sms_sensor
```

---

## Status

**Assignment 02 kernel sensor driver baseline: COMPLETED AND FUNCTIONALLY VERIFIED.**

Current completed baseline includes character-device access, simulated sensor generation, procfs statistics, synchronization, sampling interval control, `-EAGAIN` behavior, and an ARM userspace ioctl test.