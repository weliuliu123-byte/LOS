#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* BUG 1: Array out-of-bounds */
void process_data(int *data, int len)
{
    int sum = 0;

    for (int i = 0; i <= len; i++) {
        sum += data[i];
    }

    printf("Sum = %d\n", sum);
}

/* BUG 2: Use-after-free */
char *create_message(const char *text)
{
    char *buf = malloc(strlen(text) + 1);

    if (buf == NULL)
        return NULL;

    strcpy(buf, text);

    free(buf);

    return buf;
}

/* BUG 3: Signed integer overflow */
int calculate(int a, int b)
{
    int result = a * b;

    printf("calculate(%d, %d) = %d\n", a, b, result);

    return result;
}

int main(void)
{
    printf("=== LAB03 Debug Target ===\n");

    int arr[5] = {10, 20, 30, 40, 50};

    printf("[TEST1] Array processing\n");
    process_data(arr, 5);

    printf("[TEST2] Message creation\n");

    char *msg = create_message("Hello Embedded Linux");
    printf("Message: %s\n", msg);

    printf("[TEST3] Integer calculation\n");
    calculate(100000, 100000);

    printf("Done.\n");

    return 0;
}
