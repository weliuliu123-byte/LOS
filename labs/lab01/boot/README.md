# LAB-01 — Boot Sequence & Architecture Notes

**Target Hardware:** ARM Versatile Express CA9x4 (QEMU `vexpress-a9`)  
**Architecture:** ARMv7-A Cortex-A9 MPCore  
**Verification Target:** Direct Kernel Initialization & Bootloader Flow  

---

## 1. Overview of Emulated Boot Modes

In embedded systems engineering, booting Linux can occur through two distinct pathways:

```text
[ METHOD A: DIRECT QEMU KERNEL BOOT ]
QEMU Host Emulator
  │
  ├──> Load zImage into RAM @ 0x60008000
  ├──> Load vexpress-v2p-ca9.dtb @ 0x60500000
  ├──> Load initramfs.cpio.gz @ 0x61000000
  └──> Direct Execution Jump (r0=0, r1=0x8e0, r2=0x60500000)
        │
        └──> Linux Kernel Initialization -> BusyBox init (/sbin/init)

[ METHOD B: TWO-STAGE U-BOOT BOOTLOADER FLOW ]
QEMU Host Emulator
  │
  └──> Load u-boot binary @ 0x60000000
        │
        └──> U-Boot Interactive CLI (bootcmd, bootargs, tftp/mmc read)
              │
              └──> bootm / bootz -> Kernel Handoff
```

---

## 2. Method A: Direct Kernel Boot (Production Verified)

This is the primary automated workflow used throughout this course for testing kernel drivers and user-space binaries.

### QEMU Invocation Command

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

### Critical Parameters Explained

- `-M vexpress-a9`: Emulates the ARM Versatile Express Cortex-A9 motherboard.
- `-cpu cortex-a9 -smp 2`: Instantiates two symmetric ARM Cortex-A9 processing cores.
- `-m 512M`: Allocates 512 MiB of contiguous physical RAM (`0x60000000` to `0x80000000`).
- `-nographic`: Directs all serial console I/O to standard terminal streams (`stdio`), disabling graphical SDL/VNC windows.
- `-dtb`: Passes the compiled Flattened Device Tree defining peripheral MMIO bases and IRQ lines.
- `-append "console=ttyAMA0,115200 rdinit=/sbin/init"`:
  - `console=ttyAMA0,115200`: Configures the primary kernel printk output to the first PL011 UART controller.
  - `rdinit=/sbin/init`: Tells the kernel to mount the RAM disk and execute `/sbin/init` (BusyBox init) as PID 1.

---

## 3. Method B: U-Boot Bootloader Architecture

U-Boot (`2022.04`) is cross-compiled using `vexpress_ca9x4_defconfig`. In real physical hardware, U-Boot performs:
1. Low-level clock, DRAM controller, and voltage regulator initialization.
2. Loading kernel artifacts from MMC/eMMC, NAND flash, or network TFTP into designated RAM addresses.
3. Setting kernel `bootargs` dynamically and jumping to the kernel via `bootz ${kernel_addr} ${initrd_addr} ${fdt_addr}`.

The repository preserves `labs/lab01/configs/uboot.config` to allow reproducing the secondary bootloader build whenever physical deployment or U-Boot scripting is required.

---

## 4. Initialization Filesystem Hierarchy

Inside the emulated target, userspace initializes via:
1. `/sbin/init` reads `/etc/inittab`.
2. `/etc/inittab` executes `/etc/init.d/rcS` as a `sysinit` action.
3. `/etc/init.d/rcS` mounts virtual filesystems:
   - `mount -t proc none /proc`
   - `mount -t sysfs none /sys`
   - `mount -t devtmpfs none /dev`
4. `/etc/inittab` executes `::askfirst:-/bin/sh`, spawning the interactive root shell prompt.
