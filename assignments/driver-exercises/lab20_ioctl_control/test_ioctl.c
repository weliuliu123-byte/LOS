#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define LAB2_IOC_MAGIC  'L'
#define GET_BUFFER_LEN  _IOR(LAB2_IOC_MAGIC, 1, int)
#define CLEAR_BUFFER    _IO(LAB2_IOC_MAGIC, 2)

int main(void)
{
    int fd, len = 0;

    fd = open("/dev/lab2", O_RDWR);
    if (fd < 0) {
        perror("Failed to open /dev/lab2");
        return 1;
    }

    if (ioctl(fd, GET_BUFFER_LEN, &len) == 0)
        printf("[User] Current buffer_len via ioctl = %d\n", len);

    if (ioctl(fd, CLEAR_BUFFER) == 0)
        printf("[User] Sent CLEAR_BUFFER ioctl command!\n");

    if (ioctl(fd, GET_BUFFER_LEN, &len) == 0)
        printf("[User] Buffer_len after CLEAR_BUFFER = %d\n", len);

    close(fd);
    return 0;
}