# Thread-Metric RTOS Test Suite

Thread-Metric measures RTOS primitive throughput by counting operations completed
in a fixed time interval (default 30 seconds). Results enable direct comparison
across different RTOS implementations on the same hardware.

Originally from Microsoft's [ThreadX](https://github.com/eclipse-threadx/threadx)
repository, released under the MIT license.

## Tests

| Test | File | What it measures |
|------|------|-----------------|
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
  tm_api.h              # RTOS-neutral API (14 functions)
  tm_port.h            # Platform-specific defines (e.g. TM_CAUSE_INTERRUPT)

src/
  *.c                   # One test per file, each defines tm_main()

ports/
  threadx/              # ThreadX porting layer + entry point
    tm_port.c
    main.c
```

Two layers, one boundary: tests call the API in `tm_api.h`, the porting layer
maps those calls to real RTOS primitives. Only one test links per build.

## Porting Layer

Implement the 14 functions declared in `tm_api.h`. See
`ports/threadx/tm_port.c` for a complete reference.

Requirements for fair benchmarking:
- Functions must be real calls, not macros
- `tm_thread_sleep` needs a 10 ms periodic tick source
- No cache locking of test or RTOS code regions
- Use 30-second measurement intervals

## Building (ThreadX on Linux)

```
make            # clones ThreadX, builds libtx.a, links all tests
make tm_basic_processing   # build a single test
make check      # build with 3s intervals, run + verify all tests
make clean      # remove binaries and build directory
make distclean  # also remove cloned ThreadX source
```

Run with `sudo` (ThreadX Linux port uses `pthread_setschedparam`):
```
sudo ./tm_basic_processing
```

## Adding a New RTOS Port

1. Create `ports/<rtos>/tm_port.c` implementing the 14 functions in `tm_api.h`
2. Create `ports/<rtos>/main.c` with RTOS-specific startup
3. Add a build target or adapt the Makefile
