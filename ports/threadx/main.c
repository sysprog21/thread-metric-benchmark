/*
 * ThreadX entry point for Thread-Metric benchmarks.
 *
 * Every test defines tm_main(), so this single shim works for all tests.
 * main() starts the ThreadX kernel, which calls tx_application_define(),
 * which calls tm_main() to initialize the test and create threads.
 */

#include <stdio.h>
#include "tm_api.h"
#include "tx_api.h"

void tm_main(void);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Thread-Metric: reporting interval = %d s\n", TM_TEST_DURATION);
    tx_kernel_enter();
    return 0;
}

void tx_application_define(void *first_unused_memory)
{
    (void) first_unused_memory;
    tm_main();
}
