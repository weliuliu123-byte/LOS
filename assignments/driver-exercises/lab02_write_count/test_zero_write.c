#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    int fd;
    ssize_t ret;

    fd = open("/dev/lab2", O_WRONLY);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    ret = write(fd, "", 0);

    if (ret < 0) {
        perror("write");
        close(fd);
        return 1;
    }

    printf("write(fd, \"\", 0) returned: %zd\n", ret);

    close(fd);
    return 0;
}