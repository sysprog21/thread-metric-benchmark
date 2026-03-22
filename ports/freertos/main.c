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

int main(int argc, char *argv[])
{
#ifndef TM_SEMIHOSTING
    /* Ensure tm_putchar()/putchar() output is visible immediately
     * when stdout is redirected or piped (e.g. make check, CI logs).
     */
    setvbuf(stdout, NULL, _IONBF, 0);
#endif
    tm_report_init();
    tm_report_init_argv(argc, argv);
    tm_printf("Thread-Metric: reporting interval = %d s\n", tm_test_duration);
    tm_main();
    return 0;
}
