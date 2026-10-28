/*
 * test_ioctl.c
 *
 * Assignment 02 - Embedded Linux Systems
 *
 * Test program for SMS_SET_INTERVAL ioctl.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include "../driver/sms_sensor.h"


#define DEVICE_PATH "/dev/sms_sensor"
#define BUFFER_SIZE 128


static int read_one_sample(void)
{
    int fd;
    ssize_t ret;

    char buffer[BUFFER_SIZE];


    fd = open(
        DEVICE_PATH,
        O_RDONLY
    );


    if (fd < 0) {

        perror("open");

        return -1;
    }


    ret = read(
        fd,
        buffer,
        sizeof(buffer) - 1
    );


    if (ret < 0) {

        if (errno == EAGAIN) {

            printf(
                "READ: EAGAIN "
                "(sensor not ready)\n"
            );

        } else {

            perror("read");
        }


        close(fd);

        return -1;
    }


    if (ret == 0) {

        printf("READ: EOF\n");

        close(fd);

        return 0;
    }


    buffer[ret] = '\0';


    printf(
        "READ: %s",
        buffer
    );


    close(fd);


    return 1;
}


int main(void)
{
    int fd;

    unsigned int interval_ms = 500;


    printf(
        "=== SMS IOCTL TEST ===\n"
    );


    /*
     * Open the device for ioctl configuration.
     */
    fd = open(
        DEVICE_PATH,
        O_RDONLY
    );


    if (fd < 0) {

        perror("open");

        return EXIT_FAILURE;
    }


    /*
     * Configure minimum interval to 500 ms.
     */
    if (ioctl(
            fd,
            SMS_SET_INTERVAL,
            &interval_ms) < 0) {

        perror("ioctl SMS_SET_INTERVAL");

        close(fd);

        return EXIT_FAILURE;
    }


    printf(
        "Interval configured to %u ms\n",
        interval_ms
    );


    close(fd);


    /*
     * First read:
     * should succeed immediately.
     */
    printf("\nSample #1:\n");

    read_one_sample();


    /*
     * Immediate second read:
     * should return EAGAIN.
     */
    printf("\nSample #2 immediately:\n");

    read_one_sample();


    /*
     * Wait longer than the configured interval.
     */
    printf(
        "\nWaiting 550 ms...\n"
    );

    usleep(550000);


    /*
     * Third read:
     * should succeed.
     */
    printf("\nSample #3:\n");

    read_one_sample();


    printf(
        "\n=== TEST COMPLETE ===\n"
    );


    return EXIT_SUCCESS;
}