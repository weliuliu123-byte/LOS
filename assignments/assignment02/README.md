# Assignment 02 — Industrial Sensor Monitoring System (SMS)

**Architecture:** ARMv7-A Cortex-A9 (QEMU `vexpress-a9`)  
**Kernel Subsystem:** Character Driver `/dev/sms_sensor`  
**Userspace Subsystem:** Multi-Threaded Real-Time Daemon (`sms_app`) with `SCHED_FIFO`  
**Status:** **COMPLETED — Baseline Kernel Driver & Real-Time App Verified**

---

## 1. System Architecture Overview

The Sensor Monitoring System (SMS) simulates an industrial environmental sensing appliance. It pairs a low-overhead Linux kernel driver with a deterministic, real-time userspace processing daemon.

```text
+-------------------------------------------------------------------------+
|                  SENSOR MONITORING SYSTEM ARCHITECTURE                  |
+-------------------------------------------------------------------------+
|  MULTI-THREADED USERSPACE DAEMON (sms_app)                              |
|                                                                         |
|  [ Thread 1: Acquisition ]  (SCHED_FIFO, Priority 80)                   |
|       |                                                                 |
|       v (Pushes raw samples)                                            |
|  +-------------------------------------------------------------------+  |
|  | Lock-Protected Circular Buffer (Mutex + Condition Variable)       |  |
|  +-------------------------------------------------------------------+  |
|       |                                                                 |
|       +-------------------+--------------------+                        |
|       v                   v                    v                        |
|  [ Thread 2: Proc ]  [ Thread 3: Alert ]  [ Thread 4: Logger ]          |
|  Moving avg filter   Threshold detector   Periodic flash commit         |
+-------------------------------------------------------------------------+
|  VFS INTERFACE: /dev/sms_sensor & /proc/sms_stats                       |
+-------------------------------------------------------------------------+
|  KERNEL DRIVER (sms_sensor_driver.ko)                                   |
|  - Dynamic Major allocation via alloc_chrdev_region()                    |
|  - Rate-limited sensor generation with monotonic timestamp              |
|  - Sampling Interval IOCTL: SMS_SET_INTERVAL (Default: 100ms)           |
|  - Early read rejection: returns -EAGAIN                                |
+-------------------------------------------------------------------------+
```

---

## 2. Kernel Driver Architecture (`sms_sensor_driver`)

### Sensor Data Generation

When userspace reads `/dev/sms_sensor`, the driver generates a CSV formatted telemetry packet:

```text
<timestamp_ms>,<temperature_celsius>,<humidity_percent>
Example: 61719,34.95,62.75
```

- **Timestamp:** Generated using kernel monotonic time `ktime_get_boottime_ns()` divided by $10^6$.
- **Temperature:** Random range 15.00°C to 45.00°C using `get_random_u32()`.
- **Humidity:** Random range 30.00% to 90.00%.
- **Rate Limiting:** Enforces minimum sampling interval (default 100ms). If read before elapsed interval, returns `-EAGAIN`.

### IOCTL Control Plane (`sms_sensor.h`)

```c
#define SMS_IOC_MAGIC         's'
#define SMS_SET_INTERVAL      _IOW(SMS_IOC_MAGIC, 1, unsigned int)
```

Userspace can dynamically configure sensor sampling rate from 20ms up to 5000ms.

### Diagnostic Telemetry (`/proc/sms_stats`)

Exposes real-time operational statistics:
- Total successful readings (`read_count`).
- Latest recorded temperature and humidity.
- Total driver uptime since module insertion.

---

## 3. Real-Time Userspace Application (`sms_app`)

The userspace daemon demonstrates POSIX multi-threaded programming and real-time scheduling discipline:

1. **Acquisition Thread:** Reads raw CSV packets from `/dev/sms_sensor` at high priority (`SCHED_FIFO`, priority 80) and inserts them into a thread-safe circular buffer.
2. **Processor Thread:** Consumes packets, calculates 10-sample moving average, and filters signal noise.
3. **Alert Thread:** Evaluates environmental safety thresholds (Alert if Temp > 40°C or Humidity > 85%).
4. **Logger Thread:** Periodically flushes structured telemetry to non-volatile disk logs.
5. **Clean Shutdown:** Handles `SIGINT` (`Ctrl+C`) cleanly, ensuring all threads join and allocated heap buffers are freed.

---

## 4. Verification & Performance Audit

### IOCTL Test Execution Transcript (`test_ioctl.c`)

```text
=== ASSIGNMENT 02 IOCTL VALIDATION ===
Opening /dev/sms_sensor: OK (fd=3)
Initial read: 12045,26.40,54.10 (Length=18)
Setting sampling interval to 250 ms via SMS_SET_INTERVAL: OK
Immediate second read check: Received -EAGAIN (Expected rate-limiting behavior: PASS)
Waiting 260 ms for interval expiration...
Reading after interval: 12308,27.15,55.30 (PASS)
Checking /proc/sms_stats:
  read_count: 2
  last_temperature: 27.15
  last_humidity: 55.30
=== ALL TESTS PASSED SUCCESSFULLY ===
```

### Valgrind Memory Audit

Executing `sms_app` under Valgrind memcheck confirmed:
- **Definitely lost:** 0 bytes in 0 blocks.
- **Memory errors:** 0 errors from 0 contexts.
- **Thread race detection:** Zero data races detected by Helgrind.
