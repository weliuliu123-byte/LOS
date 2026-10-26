# Assignment 01 — Linux Device Drivers

## Overview

This assignment focuses on developing Linux kernel device drivers
for an ARMv7 embedded Linux system using QEMU.

## Development Environment

- Host OS: Ubuntu 24.04
- Target architecture: ARMv7
- Linux kernel: 6.1.90
- Emulator: QEMU Versatile Express Cortex-A9
- Toolchain: arm-linux-gnueabihf-gcc
- Root filesystem: BusyBox initramfs

## Assignment Tasks

| Task | Description | Status |
|------|-------------|--------|
| Task 1 | Character Device Driver with IOCTL | Functional tests passed |
| Task 2 | Misc Device Driver with Atomic Counter | Not started |
| Task 3 | Technical Report | Pending |
| Task 4 | Theory Questions | Pending |

## Task 1 — IOCTL Character Device

Device: `/dev/asgn1`

Major number: `241`

Implemented operations:

- `open()`
- `release()`
- `read()`
- `write()`
- `unlocked_ioctl()`

Supported IOCTL commands:

- `ASGN1_RESET_BUFFER`
- `ASGN1_GET_STATS`
- `ASGN1_SET_MODE`
- `ASGN1_GET_VERSION`

The driver supports Echo Mode and Kernel Mode.

The module was cross-compiled for ARMv7 and functionally
verified using Linux 6.1.90 on QEMU.

## Testing

Kernel modules are loaded and unloaded manually using
`insmod` and `rmmod`.

Task 1 passed seven userspace functional tests, including
valid and invalid IOCTL requests.

## Notes

Generated binaries, kernel modules, and initramfs images
are excluded from version control.