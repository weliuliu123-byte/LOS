#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

/*
 * Assignment 01 - IOCTL Interface
 *
 * Shared header between kernel driver and userspace.
 */

/* IOCTL magic number */
#define ASGN1_IOC_MAGIC 'A'

/* Device configuration */
#define ASGN1_BUFFER_SIZE   1024
#define ASGN1_VERSION_SIZE  64

/* Operating modes */
#define ASGN1_MODE_KERNEL  0
#define ASGN1_MODE_ECHO    1

/*
 * Driver statistics.
 *
 * Fixed-width integers keep the structure layout
 * consistent between ARM kernel and userspace.
 */
struct asgn1_stats {
    __s32 open_count;
    __s32 write_count;
    __s32 read_count;
    __s32 buffer_len;
    __s32 last_write_size;
};

/*
 * IOCTL commands
 *
 * _IO  : no data transfer, used for reset_buffer
 * _IOR : read data from kernel into userspace, driver transfer data to userspace, used for get_status, get_version
 * _IOW : write data from userspace into kernel, driver transfer data to kernel, used for set_mode
 */

#define ASGN1_RESET_BUFFER \
    _IO(ASGN1_IOC_MAGIC, 0x01)

#define ASGN1_GET_STATS \
    _IOR(ASGN1_IOC_MAGIC, 0x02, struct asgn1_stats)

#define ASGN1_SET_MODE \
    _IOW(ASGN1_IOC_MAGIC, 0x03, int)

#define ASGN1_GET_VERSION \
    _IOR(ASGN1_IOC_MAGIC, 0x04, char[ASGN1_VERSION_SIZE])

#endif /* ASGN1_IOCTL_H */
