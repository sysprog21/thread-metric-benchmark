# RTOS selects the kernel.
# TARGET selects the platform.
#
#   make                                        # threadx, posix-host (default)
#   make RTOS=freertos                          # freertos, posix-host
#   make RTOS=threadx TARGET=cortex-m-qemu      # threadx, QEMU Cortex-M3

RTOS    ?= threadx
TARGET  ?= posix-host

CC      ?= cc
CFLAGS  ?= -O2 -Wall
LDFLAGS  =

# Reporting interval in seconds.  POSIX timer overhead may roughly double
# the actual wall-clock time (e.g. 30 -> ~60 s on macOS).
TM_TEST_DURATION ?= 30

TM_INC    = -Iinclude
TM_CFLAGS = -DTM_TEST_DURATION=$(TM_TEST_DURATION)

# Non-interrupt tests (build on any platform).
TESTS = \
    basic_processing \
    cooperative_scheduling \
    preemptive_scheduling \
    message_processing \
    synchronization_processing \
    memory_allocation

# Interrupt tests require tm_cause_interrupt() and wired ISR handlers in the
# porting layer.  Each supported target provides its own dispatch mechanism:
# POSIX host uses TM_ISR_VIA_THREAD; Cortex-M uses SVC (tm_isr_dispatch.c).

BINS = $(addprefix tm_, $(TESTS))

# Build directory for intermediate objects (keeps project root clean).
BUILD = build

# Target-specific toolchain
CM_SRCS =

ifeq ($(TARGET),posix-host)
  LDFLAGS += -lpthread
else ifeq ($(TARGET),cortex-m-qemu)
  CROSS_COMPILE ?= arm-none-eabi-
  CC       := $(CROSS_COMPILE)gcc
  override AR := $(CROSS_COMPILE)ar
  CFLAGS   += -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
  LDFLAGS  += -T ports/common/cortex-m/mps2_an385.ld -nostartfiles
  CM_SRCS  += ports/common/cortex-m/startup.S \
              ports/common/cortex-m/vector_table.c
  LDFLAGS  += --specs=rdimon.specs
endif

# RTOS: ThreadX
ifeq ($(RTOS),threadx)
THREADX_DIR  = threadx
TM_CFLAGS   += -DTX_DISABLE_ERROR_CHECKING
TM_PORT_SRC  = ports/threadx/tm_port.c
TM_MAIN_SRC  = ports/threadx/main.c
RTOS_DIR     = $(THREADX_DIR)

ifeq ($(TARGET),posix-host)
  POSIX_PORT = ports/threadx/posix-host
  RTOS_INC   = -I$(THREADX_DIR)/common/inc -I$(POSIX_PORT)
  RTOS_SRCS  = $(wildcard $(THREADX_DIR)/common/src/*.c) \
               $(wildcard $(POSIX_PORT)/tx_*.c)
  TM_CFLAGS += -DTM_ISR_VIA_THREAD
else ifeq ($(TARGET),cortex-m-qemu)
  CM_SRCS   += ports/threadx/cortex-m/tm_isr_dispatch.c
endif

TESTS += interrupt_processing interrupt_preemption_processing

CLONE_STAMP = $(THREADX_DIR)/.cloned
RTOS_LIB    = $(BUILD)/libtx.a
CLONE_URL   = https://github.com/eclipse-threadx/threadx

endif

.PHONY: all clean distclean check
.DELETE_ON_ERROR:

all: $(BINS)

# Clone RTOS source tree on first build.
$(CLONE_STAMP):
	git clone $(CLONE_URL) $(RTOS_DIR) --depth=1
	@touch $@

# Collect RTOS kernel + port objects into a static library.
$(RTOS_LIB): $(CLONE_STAMP) | $(BUILD)
	$(CC) $(CFLAGS) $(TM_CFLAGS) $(RTOS_INC) -c $(RTOS_SRCS)
	mv *.o $(BUILD)/
	$(AR) rcs $@ $(BUILD)/*.o

$(BUILD):
	mkdir -p $@

tm_%: src/%.c $(TM_PORT_SRC) $(TM_MAIN_SRC) $(RTOS_LIB)
	$(CC) $(CFLAGS) $(TM_CFLAGS) $(RTOS_INC) $(TM_INC) \
	    -o $@ $< $(TM_PORT_SRC) $(TM_MAIN_SRC) $(CM_SRCS) \
	    $(RTOS_LIB) $(LDFLAGS)

clean:
	rm -f $(BINS)
	rm -rf $(BUILD)

distclean: clean
	rm -rf threadx freertos-kernel rt-thread

# Quick smoke test: build with 3-second intervals, run each test under
# a timeout.  Useful for verifying a new port or code change.
check:
	$(MAKE) clean
	$(MAKE) TM_TEST_DURATION=3
	@passed=0; failed=0; \
	for t in $(BINS); do \
	    printf "  %-35s" "$$t ..."; \
	    out=$$(timeout 10 ./$$t 2>&1 | grep 'Time Period Total'); \
	    if [ -n "$$out" ]; then \
	        printf "OK\n"; passed=$$((passed + 1)); \
	    else \
	        printf "FAIL\n"; failed=$$((failed + 1)); \
	    fi; \
	done; \
	printf "\n%d passed, %d failed\n" "$$passed" "$$failed"; \
	[ "$$failed" -eq 0 ]
