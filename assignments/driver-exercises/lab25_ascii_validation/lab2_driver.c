// SPDX-License-Identifier: GPL-2.0
/*
 * LAB-02 baseline character device driver (Linux 6.1.90 / ARMv7)
 * Interfaces:
 *   /dev/lab2
 *   /proc/lab2_info
 *   /sys/class/lab2_class/lab2/{buffer_len,open_count,last_data}
 *
 * This baseline uses a single shared text buffer. Individual exercises
 * Lab 21-40 may deliberately change these semantics.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/err.h>

#define DEVICE_NAME  "lab2"
#define CLASS_NAME   "lab2_class"
#define BUFFER_SIZE  1024
#define DRIVER_MAJOR 240

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Character Device Driver with procfs and sysfs");
MODULE_VERSION("1.1");

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];
static size_t buffer_len;
static unsigned int open_count; /* cumulative successful opens */

static struct class *lab2_class;
static struct device *lab2_device;
static struct proc_dir_entry *proc_entry;

/* Guard buffer contents, length, and open_count against concurrent access. */
static DEFINE_MUTEX(lab2_mutex);

/* ------------------------------------------------------------------------- */
/* Character device callbacks                                                 */
/* ------------------------------------------------------------------------- */

static int lab2_open(struct inode *inode, struct file *file)
{
    unsigned int count;

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    count = ++open_count;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: device opened (count=%u)\n", count);
    return 0;
}

static int lab2_release(struct inode *inode, struct file *file)
{
    pr_info("lab2_driver: device closed\n");
    return 0;
}

static ssize_t lab2_read(struct file *file, char __user *buf,
                         size_t len, loff_t *offset)
{
    size_t to_copy;

    if (*offset < 0)
        return -EINVAL;
    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    if ((size_t)*offset >= buffer_len) {
        mutex_unlock(&lab2_mutex);
        return 0; /* EOF */
    }

    to_copy = min(len, buffer_len - (size_t)*offset);

    /* Userspace pointers must be accessed via copy_to_user(), not memcpy(). */
    if (copy_to_user(buf, device_buffer + (size_t)*offset, to_copy)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    *offset += to_copy;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: sent %zu bytes to user\n", to_copy);
    return to_copy;
}

static ssize_t lab2_write(struct file *file, const char __user *buf,
                          size_t len, loff_t *offset)
{
    size_t to_copy;
    size_t i;
    char temp_buffer[BUFFER_SIZE];

    if(len ==0)
        return 0;

    to_copy = min(len, (size_t)BUFFER_SIZE - 1);

    if(len > to_copy)
        pr_warn("lab2_driver: write too large, trucating\n");

    /* Copy data from userspace to temporary buffer. */
    if(copy_from_user(temp_buffer, buf, to_copy)) {
        return -EFAULT; 
    }

     /* Validate printable ASCII */
    for (i = 0; i < to_copy; i++) {
        unsigned char c = (unsigned char)temp_buffer[i];

        if (c < 0x20 || c > 0x7E) {
            pr_warn("lab2_driver: invalid ASCII byte 0x%02x\n", c);
            return -EINVAL;
        }
    }

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    memcpy(device_buffer, temp_buffer, to_copy);

    buffer_len = to_copy;
    device_buffer[buffer_len] = '\0';

    mutex_unlock(&lab2_mutex);

    pr_info("lab2_driver: received %zu valid ASCII bytes\n", to_copy);

    return to_copy;
}

static const struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};

/* ------------------------------------------------------------------------- */
/* procfs: seq_file emits a formatted snapshot through /proc/lab2_info.        */
/* ------------------------------------------------------------------------- */

static int lab2_proc_show(struct seq_file *m, void *v)
{
    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    seq_puts(m, "LAB-02 Driver Statistics\n");
    seq_puts(m, "======================\n");
    seq_printf(m, "Driver name   : %s\n", DEVICE_NAME);
    seq_printf(m, "Major number  : %d\n", major_number);
    seq_printf(m, "Buffer size   : %d bytes\n", BUFFER_SIZE);
    seq_printf(m, "Data length   : %zu bytes\n", buffer_len);
    seq_printf(m, "Open count    : %u\n", open_count);
    if (buffer_len > 0)
        seq_printf(m, "Last data     : [%s]\n", device_buffer);

    mutex_unlock(&lab2_mutex);
    return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    /* single_open() simplifies procfs output generated by one show() call. */
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ------------------------------------------------------------------------- */
/* sysfs: device kobject exposes one read-only file per DEVICE_ATTR_RO().     */
/* ------------------------------------------------------------------------- */

static ssize_t buffer_len_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    size_t len;

    mutex_lock(&lab2_mutex);
    len = buffer_len;
    mutex_unlock(&lab2_mutex);
    return sysfs_emit(buf, "%zu\n", len);
}

static ssize_t open_count_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    unsigned int count;

    mutex_lock(&lab2_mutex);
    count = open_count;
    mutex_unlock(&lab2_mutex);
    return sysfs_emit(buf, "%u\n", count);
}

static ssize_t last_data_show(struct device *dev,
                              struct device_attribute *attr, char *buf)
{
    ssize_t n;

    mutex_lock(&lab2_mutex);
    if (buffer_len > 0)
        n = sysfs_emit(buf, "%s\n", device_buffer);
    else
        n = sysfs_emit(buf, "(empty)\n");
    mutex_unlock(&lab2_mutex);
    return n;
}

static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static const struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};

/* ------------------------------------------------------------------------- */
/* Module initialization / teardown with reverse-order error unwinding.       */
/* ------------------------------------------------------------------------- */

static int __init lab2_init(void)
{
    int ret;

    pr_info("lab2_driver: initializing module\n");

    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    /* Linux 6.1 signature; Linux >= 6.4 uses class_create(CLASS_NAME). */
    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        ret = PTR_ERR(lab2_class);
        goto err_unregister;
    }

    lab2_device = device_create(lab2_class, NULL,
                                MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        ret = PTR_ERR(lab2_device);
        goto err_class;
    }

    /* Check device_create() BEFORE accessing lab2_device->kobj. */
    ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
    if (ret) {
        pr_err("lab2_driver: sysfs_create_group failed: %d\n", ret);
        goto err_device;
    }

    proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
    if (!proc_entry) {
        pr_err("lab2_driver: failed to create /proc/lab2_info\n");
        ret = -ENOMEM;
        goto err_sysfs;
    }

    pr_info("lab2_driver: loaded, major=%d\n", major_number);
    return 0;

err_sysfs:
    sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
err_device:
    device_destroy(lab2_class, MKDEV(major_number, 0));
err_class:
    class_destroy(lab2_class);
err_unregister:
    unregister_chrdev(major_number, DEVICE_NAME);
    return ret;
}

static void __exit lab2_exit(void)
{
    proc_remove(proc_entry);
    sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
