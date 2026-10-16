# LAB-01 — Boot Notes

## Target and artifacts

| Item | Value |
|---|---|
| Machine | QEMU `vexpress-a9` |
| CPU | ARM Cortex-A9 |
| Kernel | Linux `6.1.90` |
| Device Tree | `vexpress-v2p-ca9.dtb` |
| Userspace | BusyBox 1.35.0 initramfs |
| Bootloader source | U-Boot 2022.04 |

In the local LAB-01 workspace, generated artifacts are held in `~/embedded_lab1/output/`:

```text
zImage
vexpress-v2p-ca9.dtb
initramfs.cpio.gz
u-boot
```

These binaries are intentionally not committed to GitHub.

## Boot method A — Direct QEMU boot (verified)

Run on the Ubuntu **host**:

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

This loads Linux directly: **U-Boot is bypassed**.

Expected console milestone:

```text
Boot completed! Welcome to Embedded Linux Lab 1
```

Because `/etc/inittab` uses `::askfirst:-/bin/sh`, press Enter to access the BusyBox shell.

Inside the QEMU **guest**:

```sh
uname -r       # expected: 6.1.90
uname -m       # expected: armv7l
hostname      # expected: embedded-lab
cat /etc/inittab
mount
```

## Boot method B — U-Boot boot (not yet verified)

The LAB-01 workspace contains a U-Boot 2022.04 build and a saved `configs/uboot.config`. This does **not** establish that the kernel was successfully booted through U-Boot.

Potential boot flow:

```text
QEMU -> U-Boot -> zImage + DTB + initramfs -> BusyBox init -> shell
```

A reproducible U-Boot boot command sequence should only be added after confirming the image load addresses, boot arguments, and U-Boot commands on the emulated target.

## Startup files

- `/sbin/init` links to `../bin/busybox`.
- `/etc/inittab` launches `/etc/init.d/rcS`.
- `rcS` mounts `proc`, `sysfs`, and `tmpfs`, and sets the hostname.

See [LAB-01 README](../README.md) for configuration and build details.

## Troubleshooting

- **No console output:** verify `-nographic`, `console=ttyAMA0,115200`, and use the VExpress DTB.
- **No BusyBox shell:** check `/sbin/init`, `/etc/inittab`, executable `rcS`, and `rdinit=/sbin/init`.
- **Failed to access device nodes:** ensure initramfs contains required `/dev` nodes; copying them on the host generally requires elevated permissions.
- **Module loading failure in later labs:** check `uname -r` inside QEMU and compare it with the module's `vermagic`.