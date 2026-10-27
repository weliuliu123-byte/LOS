#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

/*
*   IOCTL interface shared by kernel driver and userspace
*/

#define SMS_IOC_MAGIC       'S'
#define SMS_SET_INTERVAL    _IOW(SMS_IOC_MAGIC, 1, unsigned int)

/*
*   Driver default minimum interval between two sensor sample
*/

#define SMS_DEFAULT_INTERVAL_MS 100U

#endif 
