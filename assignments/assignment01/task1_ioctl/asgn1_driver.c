#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>

#include "asgn1_ioctl.h"

/*
*   Device configuration
*/

#define ASGN1_DEVICE_NAME "asgn1"
#define ASGN1_CLASS_NAME  "asgn1_class"
#define ASGN1_MAJOR         241

#define ASGN_VERSION    "asgn1_driver v1_0"

/*
*   Device State
*/

static char device_buffer[ASGN1_BUFFER_SIZE];
static size_t buffer_len;
static struct asgn1_stats driver_stats;
static int current_mode = ASGN1_MODE_ECHO;

/* Protect shared buffer, statistic and operating mode. */

static DEFINE_MUTEX(asgn1_mutex);

/* Device registration objects. */
static struct class *asgn1_class;
static struct device *asgn1_device;

/*
*   Module information
*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("NGUYEN TRI TUE");
MODULE_DESCRIPTION("Assignment 01: IOCTL Character Device Driver. ");

/*
*   Open 
*   Purpose: track successfully device open
*   return 0 on success, negative errno on failure
*/

static int asgn1_open(struct inode *inode, struct file *file) {
    int ret;

    ret = mutex_lock_interruptible(&asgn1_mutex);
    if(ret)
        return ret;
    
    driver_stats.open_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: device opened\n");

    return 0;
}

/*
*   Release
*   Purpose: handle device close
*   return 0 on success
*/

static int asgn1_release(struct inode *inode, struct file *file) {
    pr_info("asgn1_driver: device closed\n");
    return 0;
}

/*
*   Read
*   Purpose: copy buffered data from kernel to userspace
*
*   Parameter:
*   file: open file structure
*   buf : userspace  destination buffer
*   len : maximum number of bytes required
*   offset: current file position
*
*   return number of bytes copied, 0 for EOF, or negative errno.
*/

static ssize_t asgn1_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    size_t to_copy;
    int ret;

    if(*offset < 0)
        return -EINVAL;//an invalid argument was passed to a system cal or function
    
    if(len == 0)
        return 0;

    ret = mutex_lock_interruptible(&asgn1_mutex);
    if(ret)
        return ret;

    /*  EOF: no more data available. */
    if((size_t)*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    to_copy = min(len, buffer_len - (size_t)*offset); //so byte thuc te con lai trong bo dem

    if(copy_to_user(buf, device_buffer + (size_t)*offset, to_copy)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT; // an invalid memory address was accessed
    }

    *offset += to_copy;

    /* Count only successfully non-empty reads. */
    driver_stats.read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: read %zu bytes\n", to_copy);

    return to_copy;
}

/*
*   Write
*   Purpose: copy data from userspace into the internal buffer
*
*   Mode:
*       echo (1):store input data without modification
*       kernel (0): add "[KERNEL]" prefix before storing
*
*   return number of user bytes accepted,or negative errno.
*/

static ssize_t asgn1_write(struct file *file, const char __user *buf, size_t len, loff_t *offset ) {
    static const char prefix[] = "[KERNEL]";

    size_t prefix_len = 0;
    size_t max_user_len;
    size_t to_copy;

    int ret;

    if(len ==0)
        return 0;

    ret = mutex_lock_interruptible(&asgn1_mutex);

    if(ret)
        return ret;

    /* Determine the prefix length based on the current mode. */
    if(current_mode == ASGN1_MODE_KERNEL)
        prefix_len = sizeof(prefix) -1 ;


    /*
     * Reserve one byte for '\0'.
     *
     * ECHO mode:
     *   Maximum user data = 1023 bytes.
     *
     * KERNEL mode:
     *   Maximum user data = 1015 bytes.
     */
    max_user_len = ASGN1_BUFFER_SIZE -1 - prefix_len;

    to_copy = min(len, max_user_len);

    /* Build the new contents of the device buffer. */
    if(prefix_len > 0) 
        memcpy(device_buffer, prefix, prefix_len);

    if(copy_from_user(device_buffer + prefix_len, buf, to_copy)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    buffer_len = prefix_len + to_copy;

    device_buffer[buffer_len] = '\0';

    driver_stats.write_count++;
    driver_stats.buffer_len = (int)buffer_len;
    driver_stats.last_write_size = (int)to_copy;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1_driver: wrote %zu user bytes, mode=%d\n",
            to_copy, current_mode);

    return to_copy;
}

/*
 * IOCTL
 *
 * Purpose:
 *   Handle device control commands from userspace.
 *
 * Parameters:
 *   file - Open file structure.
 *   cmd  - IOCTL command.
 *   arg  - Userspace argument.
 *
 * Return:
 *   0 on success, negative errno on failure.
 */

static long asgn1_unlocked_ioctl(struct file *file,
                                 unsigned int cmd,
                                 unsigned long arg)
{
    struct asgn1_stats stats_snapshot;
    int new_mode;
    int ret;

    /* Reject commands belonging to another ioctl interface. */
    if (_IOC_TYPE(cmd) != ASGN1_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {

    case ASGN1_RESET_BUFFER:

        ret = mutex_lock_interruptible(&asgn1_mutex);
        if (ret)
            return ret;

        memset(device_buffer, 0, ASGN1_BUFFER_SIZE);

        buffer_len = 0;

        driver_stats.read_count = 0;
        driver_stats.write_count = 0;
        driver_stats.buffer_len = 0;
        driver_stats.last_write_size = 0;

        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: ioctl RESET_BUFFER\n");

        return 0;

    case ASGN1_GET_STATS:

        ret = mutex_lock_interruptible(&asgn1_mutex);
        if (ret)
            return ret;

        /* Capture a consistent snapshot of all statistics. */
        stats_snapshot = driver_stats;

        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg,
                         &stats_snapshot,
                         sizeof(stats_snapshot)))
            return -EFAULT;

        pr_info("asgn1_driver: ioctl GET_STATS\n");

        return 0;

    case ASGN1_SET_MODE:

        if (copy_from_user(&new_mode,
                           (void __user *)arg,
                           sizeof(new_mode)))
            return -EFAULT;

        if (new_mode != ASGN1_MODE_KERNEL &&
            new_mode != ASGN1_MODE_ECHO)
            return -EINVAL;

        ret = mutex_lock_interruptible(&asgn1_mutex);
        if (ret)
            return ret;

        current_mode = new_mode;

        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1_driver: ioctl SET_MODE=%d\n", new_mode);

        return 0;

    case ASGN1_GET_VERSION:

        if (copy_to_user((void __user *)arg,
                         ASGN_VERSION,
                         sizeof(ASGN_VERSION)))
            return -EFAULT;

        pr_info("asgn1_driver: ioctl GET_VERSION\n");

        return 0;

    default:

        pr_warn("asgn1_driver: unsupported ioctl command 0x%x\n",
                cmd);

        return -ENOTTY;
    }
}

/*
 * File operations
 *
 * Connect Linux VFS operations to driver callbacks.
 */
static const struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open           = asgn1_open,
    .release        = asgn1_release,
    .read           = asgn1_read,
    .write          = asgn1_write,
    .unlocked_ioctl = asgn1_unlocked_ioctl,
};

/*
 * Module initialization
 *
 * Register the character device and create the sysfs device.
 *
 * Return:
 *   0 on success, negative errno on failure.
 */
static int __init asgn1_init(void)
{
    int ret;

    pr_info("asgn1_driver: initializing\n");

    ret = register_chrdev(ASGN1_MAJOR,
                          ASGN1_DEVICE_NAME,
                          &asgn1_fops);

    if (ret < 0) {
        pr_err("asgn1_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE, ASGN1_CLASS_NAME);

    if (IS_ERR(asgn1_class)) {
        ret = PTR_ERR(asgn1_class);
        asgn1_class = NULL;
        goto err_unregister;
    }

    asgn1_device = device_create(asgn1_class,
                                 NULL,
                                 MKDEV(ASGN1_MAJOR, 0),
                                 NULL,
                                 ASGN1_DEVICE_NAME);

    if (IS_ERR(asgn1_device)) {
        ret = PTR_ERR(asgn1_device);
        asgn1_device = NULL;
        goto err_class;
    }

    pr_info("asgn1_driver: loaded successfully (major=%d, mode=%d)\n",
            ASGN1_MAJOR, current_mode);

    return 0;

err_class:
    class_destroy(asgn1_class);

err_unregister:
    unregister_chrdev(ASGN1_MAJOR, ASGN1_DEVICE_NAME);

    return ret;
}

/*
 * Module cleanup
 *
 * Release all resources allocated during initialization.
 */
static void __exit asgn1_exit(void)
{
    device_destroy(asgn1_class, MKDEV(ASGN1_MAJOR, 0));

    class_destroy(asgn1_class);

    unregister_chrdev(ASGN1_MAJOR, ASGN1_DEVICE_NAME);

    pr_info("asgn1_driver: unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);