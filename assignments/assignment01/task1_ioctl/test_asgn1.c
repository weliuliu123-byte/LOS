#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "asgn1_ioctl.h"

#define DEVICE_PATH "/dev/asgn1"

/*
 * Reopen device to reset the file position.
 *
 * The driver does not support lseek().
 * Reopening creates a new file description
 * whose initial offset is zero.
 */
static int reopen_device(int fd)
{
    close(fd);

    fd = open(DEVICE_PATH, O_RDWR);

    if (fd < 0)
        perror("reopen device");

    return fd;
}

int main(void)
{
    int fd;
    int mode;

    char version[ASGN1_VERSION_SIZE] = {0};
    char read_buffer[ASGN1_BUFFER_SIZE] = {0};

    struct asgn1_stats stats = {0};

    const char *message = "Hello FPGA";

    ssize_t n;

    /*
     * Open character device
     */
    fd = open(DEVICE_PATH, O_RDWR);

    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    /*
     * TEST 1: GET_VERSION
     */
    printf("\n[TEST 1] GET_VERSION\n");

    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0) {
        perror("ASGN1_GET_VERSION");
        goto fail;
    }

    printf("Driver version: %s\n", version);

    if (strcmp(version, "asgn1_driver v1_0") != 0) {
        fprintf(stderr, "FAIL: Version mismatch\n");
        goto fail;
    }

    printf("PASS: GET_VERSION\n");

    /*
     * TEST 2: GET_STATS
     */
    printf("\n[TEST 2] GET_STATS\n");

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS");
        goto fail;
    }

    printf("Open count     : %d\n", stats.open_count);
    printf("Write count    : %d\n", stats.write_count);
    printf("Read count     : %d\n", stats.read_count);
    printf("Buffer length  : %d\n", stats.buffer_len);
    printf("Last write size: %d\n", stats.last_write_size);

    if (stats.open_count < 1) {
        fprintf(stderr, "FAIL: Invalid open count\n");
        goto fail;
    }

    printf("PASS: GET_STATS\n");

    /*
     * TEST 3: SET_MODE = KERNEL
     */
    printf("\n[TEST 3] SET_MODE = KERNEL\n");

    mode = ASGN1_MODE_KERNEL;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE KERNEL");
        goto fail;
    }

    n = write(fd, message, strlen(message));

    if (n != (ssize_t)strlen(message)) {
        perror("write KERNEL");
        goto fail;
    }

    /*
     * Reopen device to reset offset.
     */
    fd = reopen_device(fd);

    if (fd < 0)
        return EXIT_FAILURE;

    memset(read_buffer, 0, sizeof(read_buffer));

    n = read(fd, read_buffer, sizeof(read_buffer) - 1);

    if (n < 0) {
        perror("read KERNEL");
        goto fail;
    }

    read_buffer[n] = '\0';

    printf("Read result: %s\n", read_buffer);

    if (strcmp(read_buffer, "[KERNEL]Hello FPGA") != 0) {
        fprintf(stderr, "FAIL: Kernel Mode output mismatch\n");
        goto fail;
    }

    printf("PASS: Kernel Mode\n");

    /*
     * TEST 4: SET_MODE = ECHO
     */
    printf("\n[TEST 4] SET_MODE = ECHO\n");

    mode = ASGN1_MODE_ECHO;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0) {
        perror("SET_MODE ECHO");
        goto fail;
    }

    n = write(fd, message, strlen(message));

    if (n != (ssize_t)strlen(message)) {
        perror("write ECHO");
        goto fail;
    }

    /*
     * Reopen device to reset offset.
     */
    fd = reopen_device(fd);

    if (fd < 0)
        return EXIT_FAILURE;

    memset(read_buffer, 0, sizeof(read_buffer));

    n = read(fd, read_buffer, sizeof(read_buffer) - 1);

    if (n < 0) {
        perror("read ECHO");
        goto fail;
    }

    read_buffer[n] = '\0';

    printf("Read result: %s\n", read_buffer);

    if (strcmp(read_buffer, "Hello FPGA") != 0) {
        fprintf(stderr, "FAIL: Echo Mode output mismatch\n");
        goto fail;
    }

    printf("PASS: Echo Mode\n");

    /*
     * TEST 5: RESET_BUFFER
     */
    printf("\n[TEST 5] RESET_BUFFER\n");

    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0) {
        perror("RESET_BUFFER");
        goto fail;
    }

    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("GET_STATS after RESET");
        goto fail;
    }

    printf("Open count     : %d\n", stats.open_count);
    printf("Write count    : %d\n", stats.write_count);
    printf("Read count     : %d\n", stats.read_count);
    printf("Buffer length  : %d\n", stats.buffer_len);
    printf("Last write size: %d\n", stats.last_write_size);

    if (stats.read_count != 0 ||
        stats.write_count != 0 ||
        stats.buffer_len != 0 ||
        stats.last_write_size != 0) {

        fprintf(stderr, "FAIL: RESET_BUFFER statistics mismatch\n");
        goto fail;
    }

    /*
     * Reopen device to reset offset.
     */
    fd = reopen_device(fd);

    if (fd < 0)
        return EXIT_FAILURE;

    memset(read_buffer, 0, sizeof(read_buffer));

    n = read(fd, read_buffer, sizeof(read_buffer));

    if (n != 0) {
        fprintf(stderr, "FAIL: Buffer should be empty\n");
        goto fail;
    }

    printf("PASS: RESET_BUFFER\n");

    /*
     * TEST 6: INVALID MODE
     */
    printf("\n[TEST 6] INVALID MODE\n");

    mode = 99;
    errno = 0;

    if (ioctl(fd, ASGN1_SET_MODE, &mode) != -1 ||
        errno != EINVAL) {

        fprintf(stderr, "FAIL: Expected EINVAL\n");
        goto fail;
    }

    printf("PASS: Invalid mode rejected\n");

    /*
     * TEST 7: INVALID IOCTL
     */
    printf("\n[TEST 7] INVALID IOCTL\n");

    errno = 0;

    if (ioctl(fd, _IO('Z', 0x7F)) != -1 ||
        errno != ENOTTY) {

        fprintf(stderr, "FAIL: Expected ENOTTY\n");
        goto fail;
    }

    printf("PASS: Invalid ioctl rejected\n");

    /*
     * All tests completed successfully.
     */
    printf("\n====================================\n");
    printf(" ASSIGNMENT 01 - TASK 1 TEST PASS\n");
    printf("====================================\n");

    close(fd);

    return EXIT_SUCCESS;

fail:
    close(fd);
    return EXIT_FAILURE;
}