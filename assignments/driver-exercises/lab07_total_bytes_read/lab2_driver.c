#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#define DEVICE_NAME "lab2"
#define CLASS_NAME  "lab2_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 240

static int major_number = DRIVER_MAJOR;
static char device_buffer[BUFFER_SIZE];
static int buffer_len = 0;
static int open_count = 0;
/* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
static unsigned long total_bytes_read = 0; /* [EXERCISE 7 - ADDED]: Biến cộng dồn tổng số byte đã đọc */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

static struct class *lab2_class = NULL;
static struct device *lab2_device = NULL;

static DEFINE_MUTEX(lab2_mutex);

/* ==================== CHARACTER DEVICE ==================== */

static int lab2_open(struct inode *inodep, struct file *filep)
{
    open_count++;
    pr_info("lab2: device opened (%d times)\n", open_count);
    return 0;
}

static int lab2_release(struct inode *inodep, struct file *filep)
{
    pr_info("lab2: device closed\n");
    return 0;
}

static ssize_t lab2_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    int bytes_to_read;

    if (*offset >= buffer_len)
        return 0;

    bytes_to_read = buffer_len - *offset;
    if (len < bytes_to_read)
        bytes_to_read = len;

    if (copy_to_user(buffer, device_buffer + *offset, bytes_to_read))
        return -EFAULT;

    *offset += bytes_to_read;
    /* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
    total_bytes_read += bytes_to_read; /* [EXERCISE 7 - ADDED]: Cộng dồn số byte thực tế đã trả về cho user */
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
    pr_info("lab2: read %d bytes (total_bytes_read=%lu)\n", bytes_to_read, total_bytes_read);
    return bytes_to_read;
}

static ssize_t lab2_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    int bytes_to_write;

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    bytes_to_write = (len > BUFFER_SIZE - 1) ? (BUFFER_SIZE - 1) : len;

    memset(device_buffer, 0, BUFFER_SIZE);
    if (copy_from_user(device_buffer, buffer, bytes_to_write)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    device_buffer[bytes_to_write] = '\0';
    buffer_len = bytes_to_write;

    mutex_unlock(&lab2_mutex);
    pr_info("lab2: wrote %d bytes\n", bytes_to_write);
    return bytes_to_write;
}

static struct file_operations lab2_fops = {
    .owner   = THIS_MODULE,
    .open    = lab2_open,
    .release = lab2_release,
    .read    = lab2_read,
    .write   = lab2_write,
};

/* ==================== SYSFS ATTRIBUTES ==================== */

static ssize_t buffer_len_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", buffer_len);
}
static DEVICE_ATTR_RO(buffer_len);

static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", open_count);
}
static DEVICE_ATTR_RO(open_count);

/* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
/* [EXERCISE 7 - ADDED]: Hàm hiển thị total_bytes_read qua sysfs */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
static ssize_t total_bytes_read_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%lu\n", total_bytes_read);
}
static DEVICE_ATTR_RO(total_bytes_read);

/* ==================== PROCFS ==================== */

static int lab2_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Driver Name      : %s\n", DEVICE_NAME);
    seq_printf(m, "Major            : %d\n", major_number);
    seq_printf(m, "Buffer Len       : %d\n", buffer_len);
    seq_printf(m, "Open Count       : %d\n", open_count);
    /* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
    seq_printf(m, "Total Bytes Read : %lu\n", total_bytes_read); /* [EXERCISE 7 - ADDED]: Hiển thị trong /proc/lab2_info */
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
    seq_printf(m, "Content          : %s\n", device_buffer);
    return 0;
}

/* ==================== INIT / EXIT ==================== */

static int __init lab2_init(void)
{
    int ret;

    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2: failed to register chrdev\n");
        return ret;
    }

    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
    }

    lab2_device = device_create(lab2_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }

    device_create_file(lab2_device, &dev_attr_buffer_len);
    device_create_file(lab2_device, &dev_attr_open_count);
    /* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
    device_create_file(lab2_device, &dev_attr_total_bytes_read); /* [EXERCISE 7 - ADDED]: Tạo file sysfs total_bytes_read */
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

    proc_create_single("lab2_info", 0, NULL, lab2_proc_show);

    mutex_init(&lab2_mutex);
    pr_info("lab2: driver loaded successfully\n");
    return 0;
}

static void __exit lab2_exit(void)
{
    remove_proc_entry("lab2_info", NULL);
    /* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
    device_remove_file(lab2_device, &dev_attr_total_bytes_read); /* [EXERCISE 7 - ADDED]: Xóa file sysfs total_bytes_read */
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
    device_remove_file(lab2_device, &dev_attr_open_count);
    device_remove_file(lab2_device, &dev_attr_buffer_len);
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    mutex_destroy(&lab2_mutex);
    pr_info("lab2: driver unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ha Gia Bao");
MODULE_DESCRIPTION("Lab2 Character Driver - Exercise 7");