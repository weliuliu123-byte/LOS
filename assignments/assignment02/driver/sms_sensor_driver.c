/*
 * sms_sensor_driver.c
 *
 * Assignment 02 - Embedded Linux Systems
 * Sensor Monitoring System
 *
 * Stage D5:
 *   - Character device /dev/sms_sensor
 *   - Random temperature and humidity
 *   - Timestamp using monotonic clock
 *   - Driver statistics via /proc/sms_stats
 *   - ioctl SMS_SET_INTERVAL
 *   - Minimum sampling interval enforcement
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>

#include "sms_sensor.h"


#define DRIVER_NAME        "sms_sensor"
#define PROC_NAME          "sms_stats"

#define SENSOR_BUF_SIZE    64

#define TEMP_MIN_X100      1500
#define TEMP_MAX_X100      4500

#define HUM_MIN_X100       3000
#define HUM_MAX_X100       9000


/*
 * Per-open file context.
 *
 * One open session owns one generated sensor sample.
 * This guarantees that partial reads continue reading
 * the same sample.
 */
struct sms_file_ctx {
    char data[SENSOR_BUF_SIZE];
    size_t data_len;
};


/*
 * Character device major number.
 */
static int sms_major;


/*
 * /proc/sms_stats entry.
 */
static struct proc_dir_entry *sms_proc_entry;


/*
 * Global statistics.
 */
static unsigned long read_count;
static u32 last_temp_x100;
static u32 last_hum_x100;
static u64 driver_start_ms;


/*
 * Sampling control.
 *
 * interval_ms:
 *     minimum allowed interval between two generated samples.
 *
 * last_sample_ms:
 *     monotonic timestamp of the most recently generated sample.
 */
static unsigned int interval_ms = SMS_DEFAULT_INTERVAL_MS;
static u64 last_sample_ms;


/*
 * Protect statistics.
 */
static DEFINE_MUTEX(stats_lock);


/*
 * Protect sampling interval and last_sample_ms.
 *
 * It also prevents two readers from generating samples
 * simultaneously and violating the minimum interval.
 */
static DEFINE_MUTEX(sample_lock);


/*
 * Device open
 */
static int sms_sensor_open(struct inode *inode, struct file *file)
{
    struct sms_file_ctx *ctx;

    ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

    if (!ctx)
        return -ENOMEM;

    file->private_data = ctx;

    pr_info("%s: device opened\n", DRIVER_NAME);

    return 0;
}


/*
 * Device read
 */
static ssize_t sms_sensor_read(struct file *file,
                               char __user *buf,
                               size_t len,
                               loff_t *offset)
{
    struct sms_file_ctx *ctx = file->private_data;

    u64 timestamp_ms;
    u64 elapsed_ms;

    u32 temp_x100;
    u32 hum_x100;

    size_t remaining;
    size_t to_copy;


    /*
     * Generate one sample only once for this open session.
     */
    if (*offset == 0 && ctx->data_len == 0) {

        /*
         * Sampling state is shared globally, therefore it must
         * be protected.
         */
        mutex_lock(&sample_lock);

        timestamp_ms =
            div_u64(ktime_get_ns(), 1000000);


        /*
         * The very first sample is always allowed.
         *
         * For later samples:
         *
         *     elapsed = current_time - last_sample_time
         *
         * If elapsed is less than interval_ms, the sensor is
         * considered not ready yet.
         */
        if (last_sample_ms != 0) {

            elapsed_ms =
                timestamp_ms - last_sample_ms;

            if (elapsed_ms < interval_ms) {

                mutex_unlock(&sample_lock);

                pr_info(
                    "%s: sample not ready "
                    "(elapsed=%llu ms interval=%u ms)\n",
                    DRIVER_NAME,
                    elapsed_ms,
                    interval_ms
                );

                return -EAGAIN;
            }
        }


        /*
         * Generate random temperature:
         *
         *     1500 ... 4500
         *
         * representing:
         *
         *     15.00 ... 45.00 C
         */
        temp_x100 =
            TEMP_MIN_X100 +
            (get_random_u32() %
             (TEMP_MAX_X100 -
              TEMP_MIN_X100 + 1));


        /*
         * Generate random humidity:
         *
         *     3000 ... 9000
         *
         * representing:
         *
         *     30.00 ... 90.00 %
         */
        hum_x100 =
            HUM_MIN_X100 +
            (get_random_u32() %
             (HUM_MAX_X100 -
              HUM_MIN_X100 + 1));


        /*
         * Format one complete CSV sensor sample.
         */
        ctx->data_len = scnprintf(
            ctx->data,
            SENSOR_BUF_SIZE,
            "%llu,%u.%02u,%u.%02u\n",
            timestamp_ms,

            temp_x100 / 100,
            temp_x100 % 100,

            hum_x100 / 100,
            hum_x100 % 100
        );


        /*
         * Record the time at which this sample was generated.
         */
        last_sample_ms = timestamp_ms;


        /*
         * Update global statistics.
         */
        mutex_lock(&stats_lock);

        read_count++;

        last_temp_x100 = temp_x100;
        last_hum_x100 = hum_x100;

        mutex_unlock(&stats_lock);


        /*
         * Sampling state update is complete.
         */
        mutex_unlock(&sample_lock);


        pr_info(
            "%s: generated sample "
            "timestamp=%llu temp=%u.%02u humidity=%u.%02u\n",
            DRIVER_NAME,
            timestamp_ms,

            temp_x100 / 100,
            temp_x100 % 100,

            hum_x100 / 100,
            hum_x100 % 100
        );
    }


    /*
     * EOF after the complete sample has been consumed.
     */
    if (*offset >= ctx->data_len)
        return 0;


    remaining =
        ctx->data_len - *offset;


    /*
     * Support partial read.
     */
    to_copy =
        min(len, remaining);


    if (copy_to_user(
            buf,
            ctx->data + *offset,
            to_copy))
        return -EFAULT;


    *offset += to_copy;


    pr_info(
        "%s: read %zu bytes\n",
        DRIVER_NAME,
        to_copy
    );


    return to_copy;
}


/*
 * ioctl handler
 */
static long sms_sensor_ioctl(struct file *file,
                             unsigned int cmd,
                             unsigned long arg)
{
    unsigned int new_interval;


    switch (cmd) {

    case SMS_SET_INTERVAL:

        /*
         * _IOW means userspace writes data to the driver.
         *
         * arg is therefore a userspace pointer to an
         * unsigned int.
         */
        if (copy_from_user(
                &new_interval,
                (unsigned int __user *)arg,
                sizeof(new_interval)))
            return -EFAULT;


        /*
         * A zero minimum interval is rejected.
         */
        if (new_interval == 0)
            return -EINVAL;


        mutex_lock(&sample_lock);

        interval_ms = new_interval;

        mutex_unlock(&sample_lock);


        pr_info(
            "%s: sampling interval set to %u ms\n",
            DRIVER_NAME,
            new_interval
        );


        return 0;


    default:

        /*
         * Unknown ioctl command.
         */
        return -ENOTTY;
    }
}


/*
 * Device release
 */
static int sms_sensor_release(struct inode *inode,
                              struct file *file)
{
    struct sms_file_ctx *ctx =
        file->private_data;


    kfree(ctx);

    file->private_data = NULL;


    pr_info(
        "%s: device closed\n",
        DRIVER_NAME
    );


    return 0;
}


/*
 * Character device operations
 */
static const struct file_operations sms_sensor_fops = {

    .owner          = THIS_MODULE,

    .open           = sms_sensor_open,

    .read           = sms_sensor_read,

    .unlocked_ioctl = sms_sensor_ioctl,

    .release        = sms_sensor_release,
};


/*
 * /proc/sms_stats show function
 */
static int sms_proc_show(struct seq_file *m,
                         void *v)
{
    unsigned long current_read_count;

    u32 current_temp_x100;
    u32 current_hum_x100;

    u64 now_ms;
    u64 uptime_seconds;


    /*
     * Take a consistent snapshot of statistics.
     */
    mutex_lock(&stats_lock);

    current_read_count =
        read_count;

    current_temp_x100 =
        last_temp_x100;

    current_hum_x100 =
        last_hum_x100;

    mutex_unlock(&stats_lock);


    /*
     * Calculate driver uptime.
     */
    now_ms =
        div_u64(
            ktime_get_ns(),
            1000000
        );


    uptime_seconds =
        div_u64(
            now_ms - driver_start_ms,
            1000
        );


    seq_printf(
        m,
        "read_count: %lu\n",
        current_read_count
    );


    seq_printf(
        m,
        "last_temperature: %u.%02u\n",
        current_temp_x100 / 100,
        current_temp_x100 % 100
    );


    seq_printf(
        m,
        "last_humidity: %u.%02u\n",
        current_hum_x100 / 100,
        current_hum_x100 % 100
    );


    seq_printf(
        m,
        "uptime_seconds: %llu\n",
        uptime_seconds
    );


    return 0;
}


/*
 * /proc/sms_stats open
 */
static int sms_proc_open(struct inode *inode,
                         struct file *file)
{
    return single_open(
        file,
        sms_proc_show,
        NULL
    );
}


/*
 * procfs operations
 */
static const struct proc_ops sms_proc_ops = {

    .proc_open =
        sms_proc_open,

    .proc_read =
        seq_read,

    .proc_lseek =
        seq_lseek,

    .proc_release =
        single_release,
};


/*
 * Module initialization
 */
static int __init sms_sensor_init(void)
{
    /*
     * Dynamically allocate a character-device major.
     */
    sms_major =
        register_chrdev(
            0,
            DRIVER_NAME,
            &sms_sensor_fops
        );


    if (sms_major < 0) {

        pr_err(
            "%s: failed to register character device\n",
            DRIVER_NAME
        );

        return sms_major;
    }


    /*
     * Initialize statistics.
     */
    read_count = 0;

    last_temp_x100 = 0;
    last_hum_x100 = 0;


    /*
     * Initialize sampling state.
     */
    interval_ms =
        SMS_DEFAULT_INTERVAL_MS;

    last_sample_ms = 0;


    /*
     * Record driver start time.
     */
    driver_start_ms =
        div_u64(
            ktime_get_ns(),
            1000000
        );


    /*
     * Create /proc/sms_stats.
     */
    sms_proc_entry =
        proc_create(
            PROC_NAME,
            0444,
            NULL,
            &sms_proc_ops
        );


    if (!sms_proc_entry) {

        pr_err(
            "%s: failed to create /proc/%s\n",
            DRIVER_NAME,
            PROC_NAME
        );


        unregister_chrdev(
            sms_major,
            DRIVER_NAME
        );


        return -ENOMEM;
    }


    pr_info(
        "%s: driver initialized\n",
        DRIVER_NAME
    );


    pr_info(
        "%s: registered with major %d\n",
        DRIVER_NAME,
        sms_major
    );


    pr_info(
        "%s: default sampling interval %u ms\n",
        DRIVER_NAME,
        interval_ms
    );


    pr_info(
        "%s: /proc/%s created\n",
        DRIVER_NAME,
        PROC_NAME
    );


    return 0;
}


/*
 * Module cleanup
 */
static void __exit sms_sensor_exit(void)
{
    proc_remove(
        sms_proc_entry
    );


    unregister_chrdev(
        sms_major,
        DRIVER_NAME
    );


    pr_info(
        "%s: driver exited\n",
        DRIVER_NAME
    );
}


module_init(sms_sensor_init);
module_exit(sms_sensor_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wis");
MODULE_DESCRIPTION(
    "Assignment 02 simulated sensor driver"
);
MODULE_VERSION("0.6");