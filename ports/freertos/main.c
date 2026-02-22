/*
 * FreeRTOS entry point for Thread-Metric benchmarks.
 *
 * Every test defines tm_main(), so this single shim works for all tests.
 * tm_main() calls tm_initialize(), which calls vTaskStartScheduler()
 * and never returns.  No FreeRTOS equivalent of tx_kernel_enter().
 */

#include <stdio.h>
#include "tm_api.h"

void tm_main(void);

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Thread-Metric: reporting interval = %d s\n", TM_TEST_DURATION);
    tm_main();
    return 0;
}
