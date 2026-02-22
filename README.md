# Thread-Metric RTOS Test Suite

Thread-Metric measures RTOS primitive throughput by counting operations completed
in a fixed time interval (default 30 seconds). Results enable direct comparison
across different RTOS implementations on the same hardware.

Originally from Microsoft's [ThreadX](https://github.com/eclipse-threadx/threadx)
repository, released under the MIT license.

## Tests

| Test | File | What it measures |
|------|------|--------------------|
| Basic Processing | `src/basic_processing.c` | Single-thread throughput (baseline for scaling results) |
| Cooperative Scheduling | `src/cooperative_scheduling.c` | 5 equal-priority threads doing round-robin relinquish |
| Preemptive Scheduling | `src/preemptive_scheduling.c` | 5 threads at different priorities doing resume/suspend chains |
| Interrupt Processing | `src/interrupt_processing.c` | Software trap -> ISR posts semaphore -> thread picks it up |
| Interrupt Preemption | `src/interrupt_preemption_processing.c` | Software trap -> ISR resumes higher-priority thread |
| Message Processing | `src/message_processing.c` | Single thread send/receive of 4-unsigned-long queue messages |
| Synchronization | `src/synchronization_processing.c` | Single thread semaphore get/put cycle |
| Memory Allocation | `src/memory_allocation.c` | Single thread 128-byte block allocate/deallocate cycle |

## Architecture

```
include/
  tm_api.h              # RTOS-neutral API (14 functions + tm_cause_interrupt)
  tm_port.h             # Platform-specific defines

src/
  *.c                   # One test per file, each defines tm_main()

ports/
  common/
    cortex-m/           # Shared Cortex-M3 bootstrap for QEMU mps2-an385
      startup.S         #   Reset handler, BSS/data init, semihosting setup
      vector_table.c    #   Default NVIC handlers (weak aliases)
      mps2_an385.ld     #   Linker script (4 MB FLASH + 4 MB SRAM)
  threadx/              # ThreadX porting layer
    tm_port.c           #   14-function API implementation
    main.c              #   Entry point
    posix-host/         #   POSIX simulator config (Linux/macOS)
    cortex-m/           #   Cortex-M3 QEMU support (SVC dispatch, SysTick)
  freertos/             # FreeRTOS porting layer
    tm_port.c           #   14-function API implementation
    main.c              #   Entry point
    posix-host/         #   POSIX simulator config (FreeRTOSConfig.h)
    cortex-m/           #   Cortex-M3 QEMU support (NVIC IRQ dispatch)

scripts/
  qemu-run.sh           # QEMU runner with semihosting + timeout
```

Two layers, one boundary: tests call the API in `tm_api.h`, the porting layer
maps those calls to real RTOS primitives. Each binary links exactly one test
source file.

## Supported RTOS Ports

| RTOS | POSIX Host | Cortex-M QEMU | Interrupt Tests |
|------|-----------|---------------|-----------------|
| ThreadX | yes | yes (mps2-an385) | yes |
| FreeRTOS | yes | yes (mps2-an385) | yes |

Both ports have been tested with all 8 tests on both targets (`make check`).

## Porting Layer

Implement the 14 functions declared in `tm_api.h` plus `tm_cause_interrupt()`.
See `ports/threadx/tm_port.c` or `ports/freertos/tm_port.c` for references.

Requirements for fair benchmarking:
- Functions must be real calls, not macros
- `tm_thread_sleep` needs a 10 ms periodic tick source
- Queue messages are `4 * sizeof(unsigned long)` bytes (16 on ILP32, 32 on LP64)
- Memory pool blocks are 128 bytes
- No cache locking of test or RTOS code regions
- Use 30-second measurement intervals

## Building

The build system selects an RTOS and target platform via two variables:

```shell
make                                        # ThreadX, POSIX host (default)
make RTOS=freertos                          # FreeRTOS, POSIX host
make RTOS=threadx  TARGET=cortex-m-qemu     # ThreadX, QEMU Cortex-M3
make RTOS=freertos TARGET=cortex-m-qemu     # FreeRTOS, QEMU Cortex-M3
```

The RTOS kernel is cloned automatically on first build (ThreadX from
eclipse-threadx/threadx, FreeRTOS from FreeRTOS/FreeRTOS-Kernel).

Build a single test:
```shell
make tm_basic_processing
```

Other targets:
```shell
make check      # build with 3s intervals + 1 cycle, run all tests
make clean      # remove binaries and build directory
make distclean  # also remove cloned RTOS sources
```

### POSIX Host

ThreadX POSIX port uses `pthread_setschedparam`, which requires elevated
privileges on Linux:
```shell
sudo ./tm_basic_processing
```

FreeRTOS POSIX port runs without `sudo`.

### Cortex-M QEMU

Requires `arm-none-eabi-gcc` and `qemu-system-arm`. Set `CROSS_COMPILE` in advance.
```shell
make RTOS=freertos TARGET=cortex-m-qemu CROSS_COMPILE=arm-none-eabi-
```

Run under QEMU (auto-terminates via semihosting after one report):
```shell
make run TARGET=cortex-m-qemu
```

Or run the QEMU script directly:
```shell
scripts/qemu-run.sh tm_basic_processing -semihosting-config enable=on,target=native
```

Control reporting cycles with `TM_TEST_CYCLES` (0 = infinite, default):
```shell
make TARGET=cortex-m-qemu TM_TEST_CYCLES=1
```

## Adding a New RTOS Port

1. Create `ports/<rtos>/tm_port.c` implementing the 14 functions in `tm_api.h`
2. Create `ports/<rtos>/main.c` with RTOS-specific startup
3. For POSIX host: provide configuration headers in `ports/<rtos>/posix-host/`
4. For Cortex-M QEMU: provide `FreeRTOSConfig.h` (or equivalent) and ISR
   dispatch in `ports/<rtos>/cortex-m/`; the shared bootstrap in
   `ports/common/cortex-m/` handles reset, vector table, and linker script
5. Add an `ifeq ($(RTOS),<rtos>)` block in the Makefile
