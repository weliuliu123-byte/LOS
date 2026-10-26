# Assignment 01 – How to Write a Device Driver

## 1. Overview

This assignment is part of the **Embedded Linux Systems** course.

It focuses on advanced Linux device driver development,
including ioctl interfaces, miscellaneous devices,
kernel synchronization, and driver architecture analysis.

The implementation will target an ARMv7 Linux system
running on QEMU.

**Status:** Planned – Implementation starts after Labs 21–40.

## 2. Development Environment

| Component | Version / Configuration |
|---|---|
| Host OS | Ubuntu 24.04 |
| Target Architecture | ARMv7-A |
| CPU | ARM Cortex-A9 |
| Linux Kernel | 6.1.90 |
| BusyBox | 1.35.0 |
| U-Boot | 2022.04 |
| Emulator | QEMU vexpress-a9 |
| Cross Compiler | arm-linux-gnueabihf-gcc |

## 3. Assignment Tasks

| Task | Description | Weight | Status |
|---|---|---:|---|
| Task 1 | Character driver with ioctl interface | 40% | Not started |
| Task 2 | Misc device counter driver | 30% | Not started |
| Task 3 | Driver analysis and technical report | 20% | Not started |
| Task 4 | Theoretical questions | 10% | Not started |

## 4. Task 1 – Character Device with ioctl

Extend the LAB-02 character device driver.

Driver name:

`asgn1_driver`

Device node:

`/dev/asgn1`

### Required ioctl Commands

| Command | Description |
|---|---|
| `ASGN1_RESET_BUFFER` | Clear buffer and reset read/write counters |
| `ASGN1_GET_STATS` | Return driver statistics |
| `ASGN1_SET_MODE` | Select kernel-prefix or echo mode |
| `ASGN1_GET_VERSION` | Return driver version string |

### Technical Requirements

- Define ioctl commands in `asgn1_ioctl.h`.
- Implement `.unlocked_ioctl`.
- Use `_IO`, `_IOR`, and `_IOW` macros.
- Use `copy_to_user()` and `copy_from_user()` appropriately.
- Protect shared data with mutexes.
- Return `-ENOTTY` for unsupported ioctl commands.
- Provide kernel logs for ioctl operations.
- Test both operation modes.
- Verify buffer reset and driver statistics.
- Verify module loading and unloading.

### Expected Files

```text
task1_ioctl/
├── asgn1_driver.c
├── asgn1_ioctl.h
├── Makefile
└── test_asgn1.sh
```

## 5. Task 2 – Misc Device Counter Driver

Implement a new Linux misc device driver independently
from Task 1.

Driver name:

`counter_driver`

Device node:

`/dev/counter`

Procfs interface:

`/proc/counter_info`

### Functional Requirements

- Register the driver using `misc_register()`.
- Use `atomic_t` for the internal counter.
- Return the current counter value on read.
- Increment the counter after each read.
- Accept non-negative integer values through write.
- Reject invalid input with `-EINVAL`.
- Handle file offsets correctly.
- Expose statistics through procfs.
- Release registered resources during module unload.

### Expected Files

```text
task2_misc/
├── counter_driver.c
├── Makefile
└── test_counter.sh
```

## 6. Task 3 – Driver Analysis and Technical Report

The report must include:

1. A combined architecture diagram for both drivers.
2. Character, block, and network driver comparison.
3. Analysis of `register_chrdev()` and `misc_register()`.
4. Explanation of mutexes and atomic operations.
5. Explanation of ioctl command encoding.
6. Test results and kernel logs.
7. At least one debugging case study.

The final report will be exported as PDF.

Required report length: 8–20 pages.

## 7. Task 4 – Theoretical Questions

The assignment includes four theoretical questions:

1. Linux kernel module reference counting.
2. User/kernel memory access and `copy_to_user()`.
3. MTD subsystem versus block devices.
4. File offset handling in the VFS read path.

Answers will be included in the technical report.

## 8. Project Structure

```text
assignment01_device_driver/
├── README.md
├── task1_ioctl/
│   └── .gitkeep
├── task2_misc/
│   └── .gitkeep
├── logs/
│   └── .gitkeep
└── bao_cao/
    └── .gitkeep
```

The `.gitkeep` files preserve empty directories
until implementation begins.

## 9. Implementation Workflow

The assignment will be completed after Labs 21–40.

Planned workflow:

1. Review the existing LAB-02 character driver.
2. Design and implement the ioctl interface.
3. Cross-compile Task 1 for ARMv7.
4. Deploy and test Task 1 on QEMU.
5. Implement the independent misc device driver.
6. Cross-compile and test Task 2.
7. Collect terminal logs and kernel messages.
8. Prepare the architecture diagrams and report.
9. Answer the theoretical questions.
10. Package and validate the final submission.

## 10. Submission Requirements

The final submission must include:

- Task 1 source code, header, Makefile, module, and test script.
- Task 2 source code, Makefile, module, and test script.
- Terminal logs and kernel messages.
- Technical report in PDF format.

Required archive name:

`MSSV_HoTen_Asgn01.tar.gz`

The compiled `.ko` files will be included in the
submission archive but excluded from the GitHub repository.

## 11. Related Work

- [LAB-01 – Embedded Linux Boot](../../labs/lab01/)
- [LAB-02 – Character Device Driver](../../labs/lab02/)
- [Driver Exercises – Labs 21–40](../driver-exercises/)

## 12. Progress

| Milestone | Status |
|---|---|
| Assignment skeleton | Completed |
| LAB-02 prerequisite | Completed |
| Labs 21–23 | Completed |
| Labs 24–40 | In progress |
| Task 1 | Not started |
| Task 2 | Not started |
| Task 3 | Not started |
| Task 4 | Not started |

---

**Current status: Skeleton prepared. Implementation pending.**