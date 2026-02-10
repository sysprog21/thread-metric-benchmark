CC      ?= cc
CFLAGS  ?= -O2 -Wall
LDFLAGS  = -lpthread

# -- Thread-Metric config -------------------------------------------------
# Reporting interval in seconds.  POSIX timer overhead may roughly double
# the actual wall-clock time (e.g. 30 -> ~60 s on macOS).
TM_TEST_DURATION ?= 30

# -- ThreadX ---------------------------------------------------------------
THREADX_DIR  = threadx
TX_CFLAGS    = -DTX_DISABLE_ERROR_CHECKING -DTM_TEST_DURATION=$(TM_TEST_DURATION)
POSIX_PORT   = ports/threadx/posix-host
TX_INC       = -I$(THREADX_DIR)/common/inc -I$(POSIX_PORT)

# -- Thread-Metric ---------------------------------------------------------
TM_INC       = -Iinclude
TM_PORT_SRC  = ports/threadx/tm_port.c
TM_MAIN_SRC  = ports/threadx/main.c

# Non-interrupt tests (build on any platform).
TESTS = basic_processing \
        cooperative_scheduling \
        preemptive_scheduling \
        message_processing \
        synchronization_processing \
        memory_allocation

# Interrupt tests require platform-specific TM_CAUSE_INTERRUPT and ISR
# setup.  Uncomment if your tm_port.h is configured correctly.
# TESTS += interrupt_processing interrupt_preemption_processing

BINS = $(addprefix tm_, $(TESTS))

# Build directory for intermediate objects (keeps project root clean).
BUILD = build

# -- Rules -----------------------------------------------------------------
.PHONY: all clean distclean check
.DELETE_ON_ERROR:

all: $(BINS)

# Clone ThreadX on first build.
$(THREADX_DIR)/.cloned:
	git clone https://github.com/eclipse-threadx/threadx --depth=1
	@touch $@

# Collect ThreadX kernel + port objects into a static library.
TX_SRCS = $(wildcard $(THREADX_DIR)/common/src/*.c) \
          $(wildcard $(POSIX_PORT)/tx_*.c)

$(BUILD)/libtx.a: $(THREADX_DIR)/.cloned | $(BUILD)
	$(CC) $(CFLAGS) $(TX_CFLAGS) $(TX_INC) -c $(TX_SRCS)
	mv *.o $(BUILD)/
	$(AR) rcs $@ $(BUILD)/*.o

$(BUILD):
	mkdir -p $@

tm_%: src/%.c $(TM_PORT_SRC) $(TM_MAIN_SRC) $(BUILD)/libtx.a
	$(CC) $(CFLAGS) $(TX_CFLAGS) $(TX_INC) $(TM_INC) \
	    -o $@ $< $(TM_PORT_SRC) $(TM_MAIN_SRC) $(BUILD)/libtx.a $(LDFLAGS)

clean:
	rm -f $(BINS)
	rm -rf $(BUILD)

distclean: clean
	rm -rf $(THREADX_DIR)

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
