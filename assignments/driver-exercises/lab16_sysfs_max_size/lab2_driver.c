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
static int max_size = BUFFER_SIZE - 1; /* [EXERCISE 16 - ADDED]: Biến lưu giới hạn ghi tối đa (mặc định 1023 byte) */
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
    pr_info("lab2: read %d bytes\n", bytes_to_read);
    return bytes_to_read;
}

static ssize_t lab2_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    int bytes_to_write;

    if (mutex_lock_interruptible(&lab2_mutex))
        return -ERESTARTSYS;

    /* 🟧 >>>>>>>>>> [VỊ TRÍ THAY THẾ CODE GỐC] >>>>>>>>>> */
    /* [EXERCISE 16 - REPLACED]: Giới hạn số byte ghi theo biến max_size do người dùng cấu hình */
    /* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
    bytes_to_write = (len > max_size) ? max_size : len;

    memset(device_buffer, 0, BUFFER_SIZE);
    if (copy_from_user(device_buffer, buffer, bytes_to_write)) {
        mutex_unlock(&lab2_mutex);
        return -EFAULT;
    }

    device_buffer[bytes_to_write] = '\0';
    buffer_len = bytes_to_write;

    mutex_unlock(&lab2_mutex);
    pr_info("lab2: wrote %d bytes (max_size=%d)\n", bytes_to_write, max_size);
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
/* [EXERCISE 16 - ADDED]: Cặp hàm show/store cho phép đọc và thay đổi max_size qua sysfs */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
static ssize_t max_size_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", max_size);
}

static ssize_t max_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int val, ret;

    // Chuyển chuỗi số từ user-space sang số nguyên hệ 10
    ret = kstrtoint(buf, 10, &val);
    if (ret < 0)
        return ret;

    // Kiểm tra miền giá trị hợp lệ: từ 1 đến BUFFER_SIZE - 1 (1023)
    if (val < 1 || val > BUFFER_SIZE - 1) {
        pr_warn("lab2: invalid max_size %d (must be 1..%d)\n", val, BUFFER_SIZE - 1);
        return -EINVAL;
    }

    mutex_lock(&lab2_mutex);
    max_size = val;
    mutex_unlock(&lab2_mutex);

    pr_info("lab2: max_size updated to %d\n", max_size);
    return count;
}
/* 🟩 >>>>>>>>>> [VỊ TRÍ CHÈN CODE MỚI] >>>>>>>>>>>>>> */
static DEVICE_ATTR_RW(max_size); /* [EXERCISE 16 - ADDED]: Khai báo attribute vừa đọc vừa ghi (Read-Write, quyền 0644) */
/* <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */

/* ==================== PROCFS ==================== */

static int lab2_proc_show(struct seq_file *m, void *v)
{
    seq_printf(m, "Driver Name : %s\n", DEVICE_NAME);
    seq_printf(m, "Major       : %d\n", major_number);
    seq_printf(m, "Buffer Len  : %d\n", buffer_len);
    seq_printf(m, "Open Count  : %d\n", open_count);
    seq_printf(m, "Content     : %s\n", device_buffer);
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
    device_create_file(lab2_device, &dev_attr_max_size); /* [EXERCISE 16 - ADDED]: Đăng ký file sysfs max_size */
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
    device_remove_file(lab2_device, &dev_attr_max_size); /* [EXERCISE 16 - ADDED]: Xóa file sysfs max_size */
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
MODULE_DESCRIPTION("Lab2 Character Driver - Exercise 16");