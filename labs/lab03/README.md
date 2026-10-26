# LAB-03 — Debugging Kernel & Embedded Application Performance Analysis

## Overview

This lab focuses on debugging and performance analysis techniques for Embedded Linux applications on both the Ubuntu host and an ARMv7-A QEMU target.

The main tools used are:

- GDB multiarch + gdbserver
- Valgrind
- strace
- perf
- cyclictest
- stress-ng
- QEMU ARM

The target platform is:

- QEMU `vexpress-a9`
- ARM Cortex-A9
- ARMv7-A
- Linux kernel `6.1.90`
- 512 MB RAM
- 2 virtual CPUs

The host system used for performance and real-time analysis is Ubuntu 24.04 running Linux `7.0.0-38-generic`.

---

# 1. Repository Structure

```text
labs/lab03/
├── .gitignore
├── README.md
├── apps/
│   ├── buggy_app.c
│   ├── leaky_app.c
│   ├── leaky_app_fixed.c
│   ├── leaky_app_arm_measure.c
│   └── perf_target.c
├── logs/
│   ├── gdb_session.txt
│   ├── valgrind_report.txt
│   ├── valgrind_fixed.txt
│   ├── strace_summary.txt
│   ├── strace_sudo_crash.txt
│   ├── perf_report.txt
│   ├── perf_stat.txt
│   ├── hist_standard_baseline.txt
│   ├── hist_standard_stress.txt
│   ├── latency_histogram.png
│   └── plot_latency.gnuplot
└── bao_cao/
    └── SE205019_Lab03_BaoCao.md
```

Compiled binaries, downloaded packages, raw perf data, and initramfs images are intentionally excluded from Git.

---

# 2. Environment

## Host

```text
OS                  Ubuntu 24.04
Architecture        x86_64
Kernel              7.0.0-38-generic
GCC                 13.3.0
ARM GCC             arm-linux-gnueabihf-gcc 13.3.0
QEMU                8.2.2
GDB multiarch       15.1
Valgrind            3.22.0
strace              6.8
perf                7.0.14
cyclictest          2.50
```

## ARM Target

```text
Machine             vexpress-a9
CPU                 Cortex-A9
Architecture        ARMv7-A
Kernel              Linux 6.1.90
RAM                 512 MB
SMP                 2
```

---

# 3. Part A — Remote Debugging with GDB

## Objective

Debug an ARM application running inside QEMU using:

```text
Host:   gdb-multiarch
Target: gdbserver
```

Three intentional bugs were investigated.

## Bug 1 — Array Out-of-Bounds

The loop inside `process_data()` used an invalid termination condition that allowed:

```text
i = 5
len = 5
```

for an array containing only elements `0..4`.

Observed state:

```text
i   = 5
len = 5
sum = 150
```

### Cause

The loop used:

```c
i <= len
```

instead of:

```c
i < len
```

### Fix

Use the correct upper bound.

---

## Bug 2 — Use-After-Free

The function `create_message()` allocated a buffer and stored:

```text
"Hello Embedded Linux"
```

The observed buffer address was:

```text
0x67da8
```

The function then freed the memory before returning the same pointer.

### Cause

A dangling pointer was returned to the caller.

### Fix

Do not free the buffer before returning it. The caller should free the allocation after use.

---

## Bug 3 — Signed Integer Overflow

The following calculation was tested:

```text
100000 × 100000
```

Correct mathematical result:

```text
10000000000
```

Observed 32-bit integer result:

```text
1410065408
```

### Cause

The result exceeds the signed 32-bit `int` range.

### Fix

Perform the calculation using a sufficiently wide integer type such as `long long`.

---

## GDB Evidence

The complete debugging session is stored in:

```text
logs/gdb_session.txt
```

The log includes:

- breakpoints
- conditional breakpoints
- variable inspection
- backtrace
- register inspection
- memory addresses

---

# 4. Part B — Memory Analysis with Valgrind

## Build

```bash
gcc -g -O0 \
    -o leaky_app \
    leaky_app.c
```

Run Valgrind:

```bash
valgrind \
    --leak-check=full \
    --show-leak-kinds=all \
    ./leaky_app
```

---

## Initial Result

Valgrind detected:

```text
16,900 bytes definitely lost
250 blocks definitely lost
```

Leak breakdown:

| Allocation | Memory Lost |
|---|---:|
| Record names | 2,100 bytes |
| Score arrays | 2,000 bytes |
| Message buffers | 12,800 bytes |
| Total | 16,900 bytes |

Evidence:

```text
logs/valgrind_report.txt
```

---

## Fixed Version

The missing deallocations were added:

```c
free(r->name);
free(r->scores);
free(r);
free(buf);
```

Final Valgrind result:

```text
in use at exit: 0 bytes in 0 blocks
All heap blocks were freed
ERROR SUMMARY: 0 errors
```

Evidence:

```text
logs/valgrind_fixed.txt
```

---

# 5. ARM Memory Observation

The application was also cross-compiled for ARM.

Example command:

```bash
arm-linux-gnueabihf-gcc \
    -g -O0 \
    -march=armv7-a \
    -mfloat-abi=hard \
    -o leaky_app_arm \
    leaky_app.c
```

A measurement variant was used to keep the process alive long enough to inspect `/proc`.

Observed values:

```text
VmSize: 1548 kB
VmRSS:   156 kB
VmData:  192 kB
VmStk:   132 kB
```

Observed heap mapping:

```text
00486000-004a7000 rw-p ... [heap]
```

This corresponds to approximately:

```text
132 KiB
```

of mapped heap address space.

---

# 6. Part C — Profiling with strace and perf

## Build

```bash
gcc -g -O1 \
    -o perf_target \
    perf_target.c \
    -lm
```

---

## strace

Full syscall tracing:

```bash
strace \
    -o strace_full.txt \
    ./perf_target
```

Summary:

```bash
strace -c ./perf_target
```

Observed host result:

```text
124 total system calls
75 write() calls
```

The application remained primarily CPU-bound because most execution time was spent in userspace calculations.

Evidence:

```text
logs/strace_summary.txt
```

---

# 7. Profiling Bug Found During perf Testing

Initially, running the application through `sudo perf` caused:

```text
Segmentation fault
```

A targeted strace showed:

```text
openat(... "/tmp/perf_test.txt", O_WRONLY|O_CREAT|O_TRUNC, 0666)
    = -1 EACCES (Permission denied)

SIGSEGV
```

The host configuration was:

```text
fs.protected_regular = 2
```

The program did not check whether `fopen()` returned `NULL`.

The corrected code now checks:

```c
if (f == NULL) {
    perror("fopen /tmp/perf_test.txt");
    return;
}
```

After this fix, the program no longer crashes when the output file cannot be opened.

Evidence:

```text
logs/strace_sudo_crash.txt
```

---

# 8. perf record / perf report

Collect samples:

```bash
sudo perf record \
    -g \
    -o perf.data \
    ./perf_target
```

Generate report:

```bash
sudo perf report \
    -i perf.data \
    --stdio
```

Observed dominant hotspot:

```text
__ieee754_log_fma
```

Measured overhead:

```text
Self      ≈ 61.49%
Children  ≈ 63.86%
```

This originates from the logarithm operation inside:

```c
heavy_compute()
```

Evidence:

```text
logs/perf_report.txt
```

---

# 9. perf stat

Command:

```bash
sudo perf stat ./perf_target
```

Representative result:

```text
task-clock                    26.70 ms
cpu_core instructions        405,363,961
cpu_core cycles               83,896,676
P-core IPC                    ≈ 4.8
elapsed time                  0.027643317 s
```

Evidence:

```text
logs/perf_stat.txt
```

Because the host CPU uses a hybrid core design, the process may migrate between performance and efficiency cores.

---

# 10. ARM/QEMU Profiling

Cross-compile:

```bash
arm-linux-gnueabihf-gcc \
    -g -O1 \
    -march=armv7-a \
    -mfpu=vfpv3 \
    -mfloat-abi=hard \
    -o perf_target_arm \
    perf_target.c \
    -lm
```

Required ARM libraries included:

```text
libm.so.6
libc.so.6
ld-linux-armhf.so.3
```

ARM `strace` additionally required:

```text
libtinfo.so.6
```

---

## ARM Runtime

Inside QEMU:

```bash
time /usr/bin/perf_target_arm
```

Observed:

```text
real    0.82 s
user    0.78 s
sys     0.03 s
```

ARM syscall summary:

```text
120 total system calls
75 write() calls
```

Host execution was approximately:

```text
0.028–0.040 s
```

Therefore QEMU ARM execution was roughly 20–30 times slower than native x86_64 execution.

This is expected because the Cortex-A9 CPU is emulated by QEMU.

---

# 11. Part D — Real-Time Analysis with cyclictest

Real-time latency measurements were performed on the Ubuntu host rather than QEMU.

QEMU is unsuitable for meaningful real-time latency measurements because CPU emulation and virtualization add additional scheduling uncertainty.

---

# 12. Host Kernel Preemption Configuration

Kernel:

```text
7.0.0-38-generic
```

Relevant configuration:

```text
CONFIG_PREEMPT_BUILD=y
CONFIG_PREEMPT_LAZY=y
CONFIG_PREEMPTION=y
CONFIG_PREEMPT_DYNAMIC=y

# CONFIG_PREEMPT is not set
# CONFIG_PREEMPT_RT is not set
```

Therefore:

```text
The tested host is not running a full PREEMPT_RT kernel.
```

---

# 13. Baseline cyclictest

Command:

```bash
sudo cyclictest \
    --mlockall \
    --threads=4 \
    --priority=80 \
    --interval=1000 \
    --loops=10000 \
    --histogram=200 \
    --histfile=hist_standard_baseline.txt
```

Results:

| Thread | Min | Avg | Max |
|---|---:|---:|---:|
| 0 | 2 us | 3 us | 123 us |
| 1 | 2 us | 3 us | 242 us |
| 2 | 2 us | 6 us | 136 us |
| 3 | 2 us | 3 us | 140 us |

Worst-case baseline latency:

```text
242 us
```

Samples below 50 us:

```text
39989 / 39999
99.9750%
```

One sample exceeded the original 200-us histogram range.

Evidence:

```text
logs/hist_standard_baseline.txt
```

---

# 14. cyclictest Under Stress

The stress workload was run concurrently with cyclictest:

```bash
stress-ng \
    --cpu $(nproc) \
    --io 4 \
    --vm 2 \
    --vm-bytes 256M \
    --timeout 60s
```

The system used:

```text
20 CPU workers
4 I/O workers
2 virtual-memory workers
```

Cyclictest used a larger histogram range:

```text
500 us
```

Results:

| Thread | Min | Avg | Max |
|---|---:|---:|---:|
| 0 | 5 us | 9 us | 344 us |
| 1 | 5 us | 8 us | 356 us |
| 2 | 5 us | 7 us | 431 us |
| 3 | 5 us | 7 us | 244 us |

Worst-case stressed latency:

```text
431 us
```

All 40,000 samples were recorded without histogram overflow.

Samples below 50 us:

```text
39915 / 40000
99.7875%
```

---

# 15. Baseline vs Stress Comparison

| Metric | Baseline | Stress |
|---|---:|---:|
| Minimum latency | 2 us | 5 us |
| Average latency | ≈ 3.75 us | ≈ 7.75 us |
| Worst-case latency | 242 us | 431 us |
| Samples < 50 us | 99.9750% | 99.7875% |
| Histogram overflow | 1 | 0 |

Worst-case latency increased by:

```text
189 us
```

or approximately:

```text
78.1%
```

The stressed result demonstrates that low average latency does not guarantee low worst-case latency.

This distinction is important for real-time systems, where deterministic worst-case behavior is more important than typical latency.

---

# 16. Histogram

The latency distribution is stored in:

```text
logs/latency_histogram.png
```

The source gnuplot script is:

```text
logs/plot_latency.gnuplot
```

---

# 17. PREEMPT_RT Discussion

The current kernel uses:

```text
CONFIG_PREEMPT_DYNAMIC=y
CONFIG_PREEMPT_LAZY=y
```

but does not enable:

```text
CONFIG_PREEMPT_RT=y
```

Ubuntu low-latency packages are available, including:

```text
linux-image-lowlatency
linux-image-lowlatency-hwe-24.04
```

A low-latency kernel is not identical to a full PREEMPT_RT kernel.

PREEMPT_RT mainly improves real-time determinism by reducing long non-preemptible kernel sections and improving the real-time behavior of interrupt handling and locking mechanisms.

The main expected improvement is not simply lower average latency, but lower and more predictable worst-case latency.

Practical PREEMPT_RT installation and benchmarking were outside the required scope of this lab.

---

# 18. Key Results

| Test | Result |
|---|---|
| ARM remote GDB | PASS |
| Array OOB analysis | PASS |
| Use-after-free analysis | PASS |
| Signed overflow analysis | PASS |
| Valgrind leak detection | PASS |
| Memory leak fix | PASS |
| ARM `/proc` memory analysis | PASS |
| Host strace | PASS |
| perf record/report | PASS |
| perf stat | PASS |
| ARM strace under QEMU | PASS |
| cyclictest baseline | PASS |
| cyclictest under stress | PASS |
| Latency histogram | PASS |
| PREEMPT configuration analysis | PASS |

---

# 19. Important Troubleshooting Notes

## gdbserver address

Inside the minimal QEMU root filesystem, use:

```bash
gdbserver 0.0.0.0:1234 /usr/bin/buggy_app
```

instead of:

```bash
gdbserver :1234 ...
```

to avoid hostname-resolution issues.

## `/tmp/perf_test.txt`

When profiling through `sudo perf`, a regular file in `/tmp` owned by another user may be rejected when:

```text
fs.protected_regular = 2
```

Always check the return value of `fopen()` before using the returned pointer.

## cyclictest histogram

When histogram mode is enabled, the installed cyclictest version reported:

```text
distance is ignored and set to 0
```

A sufficiently large histogram range should be selected to avoid losing latency samples to overflow.

---

# 20. Evidence

Verified logs are stored under:

```text
logs/
```

The detailed technical report is stored under:

```text
bao_cao/
```

---

# Conclusion

LAB-03 demonstrated a complete debugging and performance-analysis workflow for Embedded Linux applications.

Remote GDB debugging was used to inspect application state on an ARMv7 target, Valgrind identified and verified the removal of memory leaks, strace exposed syscall behavior and a real error-handling problem, perf identified the dominant CPU hotspot, and cyclictest quantified the effect of system load on real-time latency.

The experiments also showed an important distinction between average performance and deterministic behavior: even when more than 99% of latency samples remained below 50 microseconds, the worst-case latency increased from 242 microseconds to 431 microseconds under load.
