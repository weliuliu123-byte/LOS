# Contributing

This repository contains labs and assignments for the **Embedded Linux Systems** course.

## Development Environment

- Linux Kernel 6.16
- ARMv7-A / Cortex-A9
- QEMU `vexpress-a9`
- `arm-linux-gnueabihf` cross compiler
- U-Boot
- BusyBox

## Repository Structure

```text
embedded-linux-systems/
├── common/         # Shared scripts and configurations
├── docs/           # Course documentation
├── labs/           # Laboratory exercises
├── assignments/    # Course assignments
└── resources/      # References and supporting materials
```

Each lab and assignment should be kept as self-contained as possible.

## Git Workflow

Before working:

```bash
git pull origin main
```

After making changes:

```bash
git status
git add .
git commit -m "your commit message"
git push origin main
```

## Commit Convention

Use clear commit messages:

```text
feat: add a new feature or implementation
fix: fix a bug or incorrect behavior
docs: update documentation
test: add or update tests
build: update Makefile or build scripts
refactor: restructure existing code
chore: repository maintenance
```

Examples:

```text
feat: add lab02 character device driver
feat: add Linux 6.16 kernel configuration
fix: correct QEMU boot arguments
docs: add lab01 boot sequence
test: add driver test script
```

## Coding Guidelines

### C / Kernel Modules

- Use meaningful function and variable names.
- Check return values from kernel APIs.
- Release allocated resources in module cleanup.
- Use `pr_info()`, `pr_warn()`, and `pr_err()` for kernel logs.
- Use `copy_to_user()` and `copy_from_user()` when transferring data between kernel space and user space.
- Use synchronization when shared data can be accessed concurrently.

### Shell Scripts

Use:

```bash
#!/usr/bin/env bash
set -e
```

Avoid hard-coded absolute paths when possible.

## Generated Files

Do not commit unnecessary generated files such as:

```text
*.o
*.cmd
*.mod
*.mod.c
*.symvers
build/
linux-*/
*.cpio
*.cpio.gz
*.img
```

Keep important configuration files such as:

```text
kernel-6.16.config
busybox.config
```

so the project can be reproduced.

## Testing

Before committing, verify that:

- The project builds successfully.
- Kernel modules compile successfully.
- QEMU boots correctly when required.
- Test scripts work as expected.
- Documentation matches the implementation.

Useful kernel-module commands:

```bash
make
modinfo module.ko
insmod module.ko
lsmod
dmesg
rmmod module
make clean
```

## Academic Integrity

This repository is used for personal coursework and learning.

Do not copy another student's source code or report. External references should be cited when appropriate.

Course and university academic-integrity requirements always take priority.