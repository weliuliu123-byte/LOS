# LAB-01 — Embedded Linux Kernel Configuration & Board Bring-Up

**Platform:** ARMv7-A Cortex-A9 (QEMU `vexpress-a9`)  
**Kernel:** Linux 6.1.90 LTS  
**Root Filesystem:** BusyBox 1.35.0 initramfs  
**Status:** **COMPLETED — Direct QEMU Boot Verified**

---

## 1. Laboratory Objectives

The primary objective of LAB-01 is to establish a verified, bare-metal-equivalent embedded Linux environment on emulated ARM hardware from primary source components:
1. Cross-compiling the Linux 6.1.90 kernel and Flattened Device Tree (FDT) for the ARM Versatile Express Cortex-A9 target.
2. Configuring and cross-compiling BusyBox 1.35.0 as a statically linked userspace utility suite.
3. Constructing an initial RAM filesystem (initramfs) incorporating custom init initialization scripts (`inittab`, `rcS`).
4. Booting the emulated target in QEMU, verifying serial console communication, devtmpfs mounting, and userspace shell spawn.

---

## 2. Target Hardware Specifications & Memory Layout

The emulated board corresponds to ARM Versatile Express CA9x4 (`vexpress-a9`):

```text
0x60000000 +---------------------------------------+
           | Physical RAM Base (512 MB Total)      |
           |                                       |
0x60008000 + - - - - - - - - - - - - - - - - - - - +
           | Kernel zImage Decompression Space     |
           |                                       |
0x60500000 + - - - - - - - - - - - - - - - - - - - +
           | Flattened Device Tree (DTB)           |
           |                                       |
0x61000000 + - - - - - - - - - - - - - - - - - - - +
           | Initramfs Root Filesystem (CPIO RAM)  |
           |                                       |
0x80000000 +---------------------------------------+ End of 512MB RAM
```

- **Processor:** Dual-core ARM Cortex-A9 MPCore with VFPv3 and NEON floating point.
- **Serial Console:** PL011 UART mapped at physical base `0x10009000` (`ttyAMA0`, baud 115200).
- **Interrupt Controller:** ARM Generic Interrupt Controller (GIC).
- **System Timer:** SP804 Dual-Timer module.

---

## 3. Build & Configuration Procedure

### Step 3.1: Kernel Configuration & Build

The kernel configuration is restored from `labs/lab01/configs/kernel.config`:

```bash
cd ~/embedded_lab1/kernel/linux-6.1.90
cp ~/embedded_linux_systems/labs/lab01/configs/kernel.config .config

# Cross-compile zImage and device tree blob
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc) zImage
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- dtbs
```

Verified output artifacts:
- `arch/arm/boot/zImage`: Compressed ARM Linux kernel image (~4.8 MB).
- `arch/arm/boot/dts/vexpress-v2p-ca9.dtb`: Compiled Device Tree representation (~14 KB).

### Step 3.2: BusyBox Userspace Compilation

BusyBox provides essential UNIX core utilities (sh, ls, cat, mknod, mount). It is configured with static linking to eliminate external dynamic C library dependencies in the initial RAM filesystem:

```bash
cd ~/embedded_lab1/rootfs/busybox-1.35.0
cp ~/embedded_linux_systems/labs/lab01/configs/busybox.config .config

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j$(nproc)
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- install CONFIG_PREFIX=~/embedded_lab1/rootfs/initramfs
```

### Step 3.3: Root Filesystem Packaging

A standard initramfs skeleton is structured with `/etc/inittab` and `/etc/init.d/rcS`:

```bash
cd ~/embedded_lab1/rootfs/initramfs
mkdir -p dev proc sys etc/init.d root

# Pack cpio archive
find . -print0 | cpio --null -ov -H newc | gzip -9 > ~/embedded_lab1/output/initramfs.cpio.gz
```

---

## 4. Boot Execution & Verified Terminal Log

The target system is launched via direct kernel boot:

```bash
qemu-system-arm \
  -M vexpress-a9 \
  -cpu cortex-a9 \
  -m 512M \
  -smp 2 \
  -nographic \
  -kernel ~/embedded_lab1/output/zImage \
  -dtb ~/embedded_lab1/output/vexpress-v2p-ca9.dtb \
  -initrd ~/embedded_lab1/output/initramfs.cpio.gz \
  -append "console=ttyAMA0,115200 rdinit=/sbin/init"
```

### Observed Boot Sequence (dmesg Extract):

```text
Booting Linux on physical CPU 0x0
Linux version 6.1.90 (hagiabao@ubuntu) (arm-linux-gnueabihf-gcc 13.3.0) #1 SMP PREEMPT
CPU: ARMv7 Processor [410fc090] revision 0 (ARMv7), cr=10c5387d
CPU: PIPT / VIPT nonaliasing data cache, VIPT nonaliasing instruction cache
OF: fdt: Machine model: V2P-CA9
Memory policy: Data cache writealloc
percpu: Embedded 14 pages/cpu s25740 r8192 d23412 u57344
Kernel command line: console=ttyAMA0,115200 rdinit=/sbin/init
Dentry cache hash table entries: 65536 (order: 6, 262144 bytes)
Inode-cache hash table entries: 32768 (order: 5, 131072 bytes)
Built 1 zonelists, mobility grouping on.  Total pages: 130048
Kernel code: 0x60008000 - 0x60c00000 (12256 KiB)
SMP: Total of 2 processors activated (4186.11 BogoMIPS).
devtmpfs: initialized
random: fast init done
10009000.uart: ttyAMA0 at MMIO 0x10009000 (irq = 37, base_baud = 0) is a PL011 rev1
printk: console [ttyAMA0] enabled
Freeing unused kernel memory: 1024K
Run /sbin/init as init process
Starting system initialization via /etc/init.d/rcS...
Mounting virtual filesystems (/proc, /sys, /dev)...
Device nodes initialized successfully.

Welcome to Embedded Linux (LOS201) - ARMv7 Cortex-A9
Target Hostname: embedded-lab1
/ # uname -a
Linux embedded-lab1 6.1.90 #1 SMP PREEMPT ARMv7
/ # 
```

---

## 5. System Verification Matrix

| Check ID | Verification Task | Expected Result | Observed Behavior | Status |
|---|---|---|---|:---:|
| **V01** | Kernel Image Integrity | Valid ARM EABI5 compressed zImage | Verified via `file zImage` | **PASS** |
| **V02** | Device Tree Compilation | DTB matches Cortex-A9 architecture | Recognized without OF warnings | **PASS** |
| **V03** | SMP Multi-Core Activation | 2 CPUs detected and brought online | `SMP: Total of 2 processors activated` | **PASS** |
| **V04** | Serial Console Output | `ttyAMA0` operational at 115200 baud | Console prompt received over UART | **PASS** |
| **V05** | Devtmpfs Mounting | Pseudo devices populated in `/dev` | `/dev/console`, `/dev/null` present | **PASS** |
| **V06** | BusyBox Userspace Execution | `/sbin/init` executes `inittab` & `rcS` | Interactive root shell prompt ready | **PASS** |

---

## 6. Root-Cause Troubleshooting Guide

| Failure Mode | Root Cause | Engineering Remediation |
|---|---|---|
| `Kernel panic - not syncing: VFS: Unable to mount root fs` | Kernel launched without initramfs or unsupported ramdisk format | Confirm `-initrd` points to valid gzip-compressed CPIO archive and `CONFIG_BLK_DEV_INITRD=y`. |
| `Kernel panic - not syncing: Attempted to kill init!` | `/sbin/init` binary missing, unexecutable, or dynamic linker absent | Ensure BusyBox compiled with `CONFIG_STATIC=y` or copy required dynamic libraries (`libc.so`, `ld-linux.so`) into `/lib`. |
| Terminal blank after `Uncompressing Linux...` | Console argument mismatch or wrong UART address in DTB | Verify bootargs contains `console=ttyAMA0,115200`. Check PL011 driver is enabled in kernel config. |
