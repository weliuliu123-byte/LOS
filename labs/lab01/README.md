# LAB-01 — Embedded Linux Kernel Configuration and Boot

## Overview

LAB-01 builds a minimal ARMv7-A Embedded Linux environment for an emulated Cortex-A9 board using the Linux kernel, Device Tree, BusyBox, initramfs, and QEMU. U-Boot is included as a configured/built bootloader component, but the **verified boot path below is QEMU direct kernel boot**, not a U-Boot-initiated boot.

## Development environment

| Component | Version / configuration |
|---|---|
| Host OS | Ubuntu 24.04.5 LTS (x86_64) |
| Target | ARMv7-A, Cortex-A9 |
| QEMU machine | `vexpress-a9` |
| Linux kernel | `6.1.90` |
| BusyBox | `1.35.0` |
| U-Boot | `2022.04` (configuration saved; boot chain not yet verified) |
| Cross-compiler | `arm-linux-gnueabihf-gcc` 13.3.0 |
| QEMU | 8.2.2 |
| Device Tree | `vexpress-v2p-ca9.dtb` |
| Root filesystem | BusyBox + `initramfs.cpio.gz` |

## Repository layout

```text
labs/lab01/
├── README.md
├── boot/
│   └── README.md
├── configs/
│   ├── kernel.config
│   ├── busybox.config
│   └── uboot.config
└── rootfs/
    └── initramfs/
        └── etc/
            ├── inittab
            └── init.d/
                └── rcS
```

This repository tracks **configurations and initialization scripts**, not third-party source trees, generated binaries, or device nodes. The full root filesystem is generated locally.

## Local workspace

The commands below use the workspace used during LAB-01:

```text
~/embedded_lab1/
├── kernel/linux-6.1.90/
├── rootfs/busybox-1.35.0/
├── rootfs/initramfs/
├── uboot/u-boot-2022.04/
└── output/
    ├── zImage
    ├── vexpress-v2p-ca9.dtb
    ├── initramfs.cpio.gz
    └── u-boot
```

The workspace and Linux, BusyBox, and U-Boot source archives must be prepared separately; this repository is not a complete standalone source distribution.

## Kernel configuration and build

From the repository root, restore the recorded configuration to the **matching Linux 6.1.90 source tree**:

```bash
cp labs/lab01/configs/kernel.config ~/embedded_lab1/kernel/linux-6.1.90/.config
cd ~/embedded_lab1/kernel/linux-6.1.90
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- olddefconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)" zImage dtbs
```

Relevant local output paths include `arch/arm/boot/zImage` and the generated VExpress Device Tree. The DTB location under `arch/arm/boot/dts/` depends on the kernel tree layout; locate it before copying:

```bash
find arch/arm/boot/dts -name 'vexpress-v2p-ca9.dtb'
```

The verified LAB-01 workspace already has the required files in `~/embedded_lab1/output/`.

## BusyBox and initramfs

BusyBox `1.35.0` supplies the minimal userspace. Its configuration is preserved at `configs/busybox.config`.

The running rootfs has:

```text
/bin/busybox
/sbin/init -> ../bin/busybox
/etc/inittab
/etc/init.d/rcS
```

The stored `inittab` invokes `/etc/init.d/rcS` during system initialization and then provides an interactive shell. The `rcS` script mounts `/proc`, `/sys`, and `/tmp`, prints a boot message, and sets the hostname to `embedded-lab`.

To restore the scripts into an existing rootfs:

```bash
cp labs/lab01/rootfs/initramfs/etc/inittab ~/embedded_lab1/rootfs/initramfs/etc/inittab
cp labs/lab01/rootfs/initramfs/etc/init.d/rcS ~/embedded_lab1/rootfs/initramfs/etc/init.d/rcS
chmod +x ~/embedded_lab1/rootfs/initramfs/etc/init.d/rcS
```

Package an already prepared rootfs **with root-owned device nodes preserved**:

```bash
cd ~/embedded_lab1/rootfs/initramfs
sudo find . -print0 | sudo cpio --null -o -H newc | gzip -9 > \
  ~/embedded_lab1/output/initramfs.cpio.gz
```

Device nodes such as `/dev/console` and `/dev/null` may need elevated permissions when copying or creating a rootfs. Do not commit those special files to Git.

## Boot with QEMU

This is the **direct boot** command corresponding to the verified environment:

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
  -append 'console=ttyAMA0,115200 rdinit=/sbin/init'
```

The expected initialization message is:

```text
Boot completed! Welcome to Embedded Linux Lab 1
```

Press Enter when prompted by BusyBox `askfirst` to enter the shell.

### Verified guest checks

```sh
uname -a
uname -r
uname -m
hostname
mount
```

The observed LAB-01-derived QEMU guest reported:

```text
Linux embedded-lab 6.1.90 ... armv7l GNU/Linux
```

## Boot architecture

```text
QEMU vexpress-a9
    |
    +--> Linux zImage + Device Tree
               |
               v
          initramfs
               |
               v
          BusyBox init
               |
               v
         /etc/inittab
               |
               v
        /etc/init.d/rcS
               |
               v
           /bin/sh
```

**U-Boot is not executed by this command.** The U-Boot 2022.04 configuration is saved for reproducibility and further bootloader work. See [boot notes](boot/README.md).

## Verification status

| Check | Status |
|---|---|
| Linux 6.1.90 kernel image available | Verified |
| VExpress Device Tree available | Verified |
| BusyBox + initramfs boots in QEMU | Verified |
| ARMv7 guest shell accessible | Verified |
| U-Boot binary present in local output | Observed |
| Booting Linux *via* U-Boot | Not yet verified |

## Next lab

[LAB-02](../lab02/README.md) uses the same ARMv7/QEMU environment to build and test a Linux character device driver with `/dev`, procfs, and sysfs interfaces.