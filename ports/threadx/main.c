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
    tx_kernel_enter();
    return 0;
}

void tx_application_define(void *first_unused_memory)
{
    (void) first_unused_memory;
    tm_main();
}
