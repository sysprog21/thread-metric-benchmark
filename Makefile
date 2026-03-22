# Thread-Metric Benchmark -- Kconfig-driven multi-RTOS build
#
# Quick start:
#   make defconfig                     # ThreadX + POSIX host (default)
#   make freertos_posix_defconfig      # FreeRTOS + POSIX host
#   make threadx_cortex_m_defconfig    # ThreadX + Cortex-M3 QEMU
#   make config                        # interactive menuconfig
#   make                               # build all tests
#   make check                         # build + smoke-test (1 s QEMU, 3 s host)

.DEFAULT_GOAL := all

-include .config

# Determine whether .config is required for this invocation.
# _NEED_CONFIG is non-empty when build targets are present.
# _HAS_CONFGEN is non-empty when a config generator will produce .config.
# _BUILD_GOALS captures explicit build targets (everything that is not
# a config/clean target or a named defconfig).
_HAS_CONFGEN := $(strip $(filter config defconfig oldconfig,$(MAKECMDGOALS)) \
    $(filter %_defconfig,$(MAKECMDGOALS)))
_BUILD_GOALS := $(filter-out config defconfig oldconfig savedefconfig \
    clean clean-bins clean-build distclean help $(filter %_defconfig,$(MAKECMDGOALS)),$(MAKECMDGOALS))
_NEED_CONFIG := $(if $(MAKECMDGOALS),$(_BUILD_GOALS),all)

# When a config generator and build goals appear together, the config
# recipe handles re-invocation via $(rebuild-if-needed).  Suppress the
# outer build targets so they don't run with the stale .config.
_SUPPRESS_BUILD :=
ifneq ($(_HAS_CONFGEN),)
  ifneq ($(_BUILD_GOALS),)
    _SUPPRESS_BUILD := y
  endif
endif

ifneq ($(_NEED_CONFIG),)
  ifeq ($(_HAS_CONFGEN),)
    ifneq "$(CONFIG_CONFIGURED)" "y"
      $(info )
      $(info *** Configuration file ".config" not found!)
      $(info *** Please run 'make defconfig' or 'make config' first.)
      $(info *** Available defconfigs:)
      $(info ***   make defconfig                    (ThreadX + POSIX host))
      $(info ***   make freertos_posix_defconfig     (FreeRTOS + POSIX host))
      $(info ***   make threadx_cortex_m_defconfig   (ThreadX + Cortex-M3 QEMU))
      $(info ***   make freertos_cortex_m_defconfig  (FreeRTOS + Cortex-M3 QEMU))
      $(info )
      $(error Configuration required)
    endif
  endif
endif

include mk/build.mk

# Test parameters from .config (command-line overrides still work via ?=).
# Runtime env-var override is available on POSIX hosts via tm_report_init().
TM_TEST_DURATION ?= $(if $(CONFIG_TEST_DURATION),$(CONFIG_TEST_DURATION),30)
TM_TEST_CYCLES   ?= $(if $(CONFIG_TEST_CYCLES),$(CONFIG_TEST_CYCLES),0)

# check target parameters -- short runs for smoke testing.
ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  CHECK_TIMEOUT    := 30
  CHECK_QEMU_FLAGS := $(QEMU_FLAGS)
else
  CHECK_DURATION   := 3
  CHECK_TIMEOUT    := 10
endif

TM_INC    = -Iinclude
TM_CFLAGS = -DTM_TEST_DURATION=$(TM_TEST_DURATION)

ifneq ($(TM_TEST_CYCLES),0)
  TM_CFLAGS += -DTM_TEST_CYCLES=$(TM_TEST_CYCLES)
endif

# Human-readable RTOS + target label for check banner.
RTOS_NAME  := $(if $(CONFIG_RTOS_THREADX),ThreadX,$(if $(CONFIG_RTOS_FREERTOS),FreeRTOS,unknown))
TARGET_NAME := $(if $(CONFIG_TARGET_POSIX_HOST),POSIX host,$(if $(CONFIG_TARGET_CORTEX_M_QEMU),Cortex-M QEMU,unknown))

# Non-interrupt tests (all RTOS ports support these).
TESTS = \
    basic_processing \
    cooperative_scheduling \
    preemptive_scheduling \
    message_processing \
    synchronization_processing \
    memory_allocation

# Interrupt tests are added per-RTOS below (they need tm_cause_interrupt()
# and wired ISR handlers).

TM_COMMON_SRC = src/tm_report.c
CM_SRCS       =

# RTOS: ThreadX
ifeq ($(CONFIG_RTOS_THREADX),y)
THREADX_DIR  = threadx
TM_CFLAGS   += -DTX_DISABLE_ERROR_CHECKING
TM_PORT_SRC  = ports/threadx/tm_port.c
TM_MAIN_SRC  = ports/threadx/main.c
RTOS_DIR     = $(THREADX_DIR)

ifeq ($(CONFIG_TARGET_POSIX_HOST),y)
  POSIX_PORT = ports/threadx/posix-host
  RTOS_INC   = -I$(THREADX_DIR)/common/inc -I$(POSIX_PORT)
  RTOS_SRCS  = $(wildcard $(THREADX_DIR)/common/src/*.c) \
               $(wildcard $(POSIX_PORT)/tx_*.c)
  TM_CFLAGS += -DTM_ISR_VIA_THREAD
else ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  CM3_PORT   = $(THREADX_DIR)/ports/cortex_m3/gnu
  RTOS_INC   = -I$(THREADX_DIR)/common/inc -I$(CM3_PORT)/inc
  RTOS_SRCS  = $(wildcard $(THREADX_DIR)/common/src/*.c) \
               $(wildcard $(CM3_PORT)/src/*.S)
  CM_SRCS   += ports/threadx/cortex-m/tm_isr_dispatch.c \
               ports/threadx/cortex-m/tx_initialize_low_level.S
  TM_CFLAGS += -DTM_SEMIHOSTING
endif

TESTS += interrupt_processing interrupt_preemption_processing

CLONE_STAMP = $(THREADX_DIR)/.cloned
RTOS_LIB    = $(BUILD)/libtx.a
CLONE_URL   = https://github.com/eclipse-threadx/threadx

# RTOS: FreeRTOS
else ifeq ($(CONFIG_RTOS_FREERTOS),y)
FREERTOS_DIR = freertos-kernel
TM_PORT_SRC  = ports/freertos/tm_port.c
TM_MAIN_SRC  = ports/freertos/main.c
RTOS_DIR     = $(FREERTOS_DIR)

FREERTOS_SRCS = $(FREERTOS_DIR)/tasks.c \
                $(FREERTOS_DIR)/queue.c \
                $(FREERTOS_DIR)/list.c \
                $(FREERTOS_DIR)/portable/MemMang/heap_4.c

ifeq ($(CONFIG_TARGET_POSIX_HOST),y)
  FREERTOS_PORT = $(FREERTOS_DIR)/portable/ThirdParty/GCC/Posix
  RTOS_INC      = -I$(FREERTOS_DIR)/include \
                  -I$(FREERTOS_PORT) \
                  -I$(FREERTOS_PORT)/utils \
                  -Iports/freertos/posix-host
  RTOS_SRCS     = $(FREERTOS_SRCS) \
                  $(wildcard $(FREERTOS_PORT)/*.c) \
                  $(wildcard $(FREERTOS_PORT)/utils/*.c)
  TM_CFLAGS    += -DTM_ISR_VIA_THREAD
else ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  FREERTOS_PORT = $(FREERTOS_DIR)/portable/GCC/ARM_CM3
  RTOS_INC      = -I$(FREERTOS_DIR)/include \
                  -I$(FREERTOS_PORT) \
                  -Iports/freertos/cortex-m
  RTOS_SRCS     = $(FREERTOS_SRCS) \
                  $(FREERTOS_PORT)/port.c
  CM_SRCS      += ports/freertos/cortex-m/tm_isr_dispatch.c
  TM_CFLAGS    += -DTM_SEMIHOSTING
endif

TESTS += interrupt_processing interrupt_preemption_processing

CLONE_STAMP = $(FREERTOS_DIR)/.cloned
RTOS_LIB    = $(BUILD)/libfreertos.a
CLONE_URL   = https://github.com/FreeRTOS/FreeRTOS-Kernel

endif # RTOS selection

ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  CM_SRCS += ports/common/cortex-m/startup.S \
             ports/common/cortex-m/vector_table.c \
             ports/common/cortex-m/tm_putchar.c
endif

BINS = $(addprefix $(BUILD)/tm_, $(TESTS))

# CFLAGS sentinel: rebuild binaries automatically when compile flags change.
# Make tracks freshness by file timestamps, not flag values; without this a
# Makefile edit (e.g. changing TM_TEST_DURATION) silently uses stale binaries.
# The sentinel lives in BUILD so "make clean" discards it along with binaries.
CFLAGS_STAMP := $(BUILD)/.cflags_stamp
$(shell mkdir -p $(BUILD) 2>/dev/null; \
    printf '%s\n' '$(CFLAGS) $(TM_CFLAGS)' | \
    diff -q - $(CFLAGS_STAMP) >/dev/null 2>&1 || \
    printf '%s\n' '$(CFLAGS) $(TM_CFLAGS)' > $(CFLAGS_STAMP))

# Build rules

.PHONY: all clean distclean check run diagnose help
.DELETE_ON_ERROR:

ifeq ($(_SUPPRESS_BUILD),y)
$(_BUILD_GOALS):
	@:
else
all: $(BINS)
endif

$(CLONE_STAMP):
	@echo "  CLONE   $(CLONE_URL)"
	$(Q)git clone $(CLONE_URL) $(RTOS_DIR) --depth=1
	@touch $@

$(RTOS_LIB): $(CLONE_STAMP) .config | $(BUILD)
	@echo "  AR      $@"
	$(Q)rm -f $(BUILD)/*.o
	$(Q)$(CC) $(CFLAGS) $(TM_CFLAGS) $(RTOS_INC) -c $(RTOS_SRCS)
	$(Q)mv *.o $(BUILD)/
	$(Q)$(AR) rcs $@ $(BUILD)/*.o

$(BUILD)/tm_%: src/%.c $(TM_COMMON_SRC) $(TM_PORT_SRC) $(TM_MAIN_SRC) $(CM_SRCS) $(RTOS_LIB) $(CFLAGS_STAMP) include/tm_api.h | $(BUILD)
	@echo "  LD      $@"
	$(Q)$(CC) $(CFLAGS) $(TM_CFLAGS) $(RTOS_INC) $(TM_INC) \
	    -o $@ $< $(TM_COMMON_SRC) $(TM_PORT_SRC) $(TM_MAIN_SRC) $(CM_SRCS) \
	    $(RTOS_LIB) $(LDFLAGS)

# Shorthand: "make tm_basic_processing" builds build/tm_basic_processing
tm_%: $(BUILD)/tm_% ;


ifneq ($(_SUPPRESS_BUILD),y)
ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
run:
	@$(MAKE) --quiet $(firstword $(BINS)) TM_TEST_CYCLES=1
	QEMU=$(QEMU) scripts/qemu-run.sh $(firstword $(BINS)) $(QEMU_FLAGS)
endif

check:
	@$(if $(CONFIG_TARGET_CORTEX_M_QEMU),\
	    if ! command -v $(QEMU) >/dev/null 2>&1; then \
	        echo "Error: $(QEMU) not found (needed for Cortex-M check)"; \
	        exit 1; \
	    fi;)
	@$(if $(CONFIG_TARGET_CORTEX_M_QEMU),\
	    $(MAKE) --quiet all TM_TEST_DURATION=1 TM_TEST_CYCLES=1,\
	    $(MAKE) --quiet all)
	@printf "  CHECK   %s + %s\n" "$(RTOS_NAME)" "$(TARGET_NAME)"
	@TCMD=""; \
	if command -v timeout >/dev/null 2>&1; then TCMD="timeout"; \
	elif command -v gtimeout >/dev/null 2>&1; then TCMD="gtimeout"; fi; \
	passed=0; failed=0; \
	for t in $(BINS); do \
	    printf "  %-40s" "$$(basename $$t) ..."; \
	    if [ "$(CONFIG_TARGET_CORTEX_M_QEMU)" = "y" ]; then \
	        raw=$$(QEMU=$(QEMU) QEMU_TIMEOUT=$(CHECK_TIMEOUT) scripts/qemu-run.sh $$t $(CHECK_QEMU_FLAGS) 2>&1); \
	    elif [ -n "$$TCMD" ]; then \
	        raw=$$(TM_TEST_DURATION=$(CHECK_DURATION) TM_TEST_CYCLES=1 $$TCMD $(CHECK_TIMEOUT) $$t 2>&1); \
	    else \
	        raw=$$(TM_TEST_DURATION=$(CHECK_DURATION) TM_TEST_CYCLES=1 $$t 2>&1); \
	    fi; \
	    out=$$(printf '%s\n' "$$raw" | grep 'Time Period Total'); \
	    if [ -n "$$out" ]; then \
	        printf "OK\n"; passed=$$((passed + 1)); \
	    else \
	        printf "FAIL\n"; failed=$$((failed + 1)); \
	        printf '%s\n' "$$raw" | sed 's/^/    | /'; \
	    fi; \
	done; \
	printf "\n%d passed, %d failed\n" "$$passed" "$$failed"; \
	[ "$$failed" -eq 0 ]
endif

diagnose:
	@echo "=== Thread-Metric Build Diagnostics ==="
	@echo ""
	@echo "Host arch   : $$(uname -m) ($$(uname -s) $$(uname -r))"
	@echo "make binary : $(MAKE)"
	@$(MAKE) --version 2>&1 | head -1
	@echo ""
	@echo "CC          : $(CC)"
	@$(CC) --version 2>&1 | head -1 || echo "  (not found)"
	@echo ""
	@echo "Config      : $(RTOS_NAME) + $(TARGET_NAME)"
	@echo "CFLAGS      : $(CFLAGS)"
	@echo "TM_CFLAGS   : $(TM_CFLAGS)"
	@echo ""
	@echo "--- CFLAGS stamp ($(CFLAGS_STAMP)) ---"
	@if [ -f $(CFLAGS_STAMP) ]; then cat $(CFLAGS_STAMP); \
	 else echo "  (missing -- run 'make' first)"; fi
	@echo ""
	@echo "--- Check parameters (Makefile := values, not overridable by env) ---"
	@echo "  CHECK_TIMEOUT    : $(CHECK_TIMEOUT)"
	@echo "  CHECK_QEMU_FLAGS : $(CHECK_QEMU_FLAGS)"
	@echo ""
	@echo "--- Shell environment (these can poison ?= assignments) ---"
	@printf "  QEMU_TIMEOUT     = %s\n" "$${QEMU_TIMEOUT:-(not set)}"
	@printf "  TM_TEST_DURATION = %s\n" "$${TM_TEST_DURATION:-(not set)}"
	@printf "  TM_TEST_CYCLES   = %s\n" "$${TM_TEST_CYCLES:-(not set)}"
	@printf "  CHECK_TIMEOUT    = %s\n" "$${CHECK_TIMEOUT:-(not set)}"
	@echo ""
	@echo "--- QEMU ---"
	@echo "  QEMU : $(QEMU)"
	@if [ -n "$(QEMU)" ]; then $(QEMU) --version 2>&1 | head -1 || echo "  (not found)"; \
	 else echo "  (n/a -- POSIX target)"; fi
	@echo ""
	@echo "--- Binaries ---"
	@for t in $(BINS); do \
	    if [ -f $$t ]; then printf "  %-52s OK\n" $$t; \
	    else printf "  %-52s MISSING\n" $$t; fi; \
	done
	@if [ "$(CONFIG_TARGET_CORTEX_M_QEMU)" = "y" ]; then \
	    echo ""; \
	    echo "--- Rebuilding: $(firstword $(BINS)) ---"; \
	    $(MAKE) --quiet $(firstword $(BINS)) 2>&1 | sed 's/^/  /' || echo "  (build failed)"; \
	    echo ""; \
	    echo "--- Binary info: $(firstword $(BINS)) ---"; \
	    if [ -f $(firstword $(BINS)) ]; then \
	        $(CROSS_COMPILE)objdump -f $(firstword $(BINS)) 2>&1 | grep -E 'file format|architecture|start address'; \
	        echo "  $(shell $(CROSS_COMPILE)nm $(firstword $(BINS)) 2>/dev/null | grep -E 'Reset_Handler|_estack|main ' | head -5)"; \
	    else \
	        echo "  (binary missing -- run 'make' first)"; \
	    fi; \
	    echo ""; \
	    echo "--- Direct QEMU (no script, 5 s limit): $(firstword $(BINS)) ---"; \
	    if [ -f $(firstword $(BINS)) ]; then \
	        echo "  cmd: $(QEMU) -M mps2-an385 -cpu cortex-m3 -nographic $(CHECK_QEMU_FLAGS) -d cpu_reset -kernel $(firstword $(BINS))"; \
	        timeout 5 $(QEMU) \
	            -M mps2-an385 -cpu cortex-m3 -nographic \
	            $(CHECK_QEMU_FLAGS) -d cpu_reset \
	            -kernel $(firstword $(BINS)) 2>&1 || true; \
	        echo "  direct-exit: $$?"; \
	    fi; \
	    echo ""; \
	    echo "--- Script QEMU run (with CPU debug): $(firstword $(BINS)) ---"; \
	    if [ -f $(firstword $(BINS)) ]; then \
	        echo "  QEMU=$(QEMU) QEMU_TIMEOUT=$(CHECK_TIMEOUT)"; \
	        echo "  flags: $(CHECK_QEMU_FLAGS) -d unimp,guest_errors,cpu_reset"; \
	        QEMU=$(QEMU) QEMU_TIMEOUT=$(CHECK_TIMEOUT) scripts/qemu-run.sh $(firstword $(BINS)) $(CHECK_QEMU_FLAGS) -d unimp,guest_errors,cpu_reset; \
	        echo "  Exit: $$?"; \
	    else \
	        echo "  (binary missing -- run 'make' first)"; \
	    fi; \
	fi

clean-bins:
	rm -f $(BINS)

clean: clean-bins clean-build

distclean: clean
	rm -f .config
	rm -rf threadx freertos-kernel rt-thread || true
	rm -rf $(KCONFIG_DIR)

# Kconfig targets

KCONFIG_DIR := tools/kconfig
KCONFIG     := configs/Kconfig

KCONFIGLIB_REPO := https://github.com/sysprog21/Kconfiglib

$(KCONFIG_DIR)/kconfiglib.py:
	@echo "  CLONE   Kconfiglib"
	$(Q)git clone --depth=1 -q $(KCONFIGLIB_REPO) $(KCONFIG_DIR)

$(KCONFIG_DIR)/menuconfig.py $(KCONFIG_DIR)/defconfig.py \
$(KCONFIG_DIR)/genconfig.py $(KCONFIG_DIR)/oldconfig.py \
$(KCONFIG_DIR)/savedefconfig.py: $(KCONFIG_DIR)/kconfiglib.py

config: $(KCONFIG_DIR)/menuconfig.py
	@python3 $(KCONFIG_DIR)/menuconfig.py $(KCONFIG)
	@echo "Configuration saved to .config"
	$(rebuild-if-needed)

# After applying a defconfig, re-invoke make for any build goals that
# appeared on the same command line (e.g., "make defconfig all").
define rebuild-if-needed
	$(if $(strip $(_BUILD_GOALS)),@$(MAKE) clean && $(MAKE) $(_BUILD_GOALS))
endef

defconfig: $(KCONFIG_DIR)/defconfig.py
	@python3 $(KCONFIG_DIR)/defconfig.py --kconfig $(KCONFIG) configs/defconfig
	@echo "Default configuration applied (ThreadX + POSIX host)"
	$(rebuild-if-needed)

%_defconfig: $(KCONFIG_DIR)/defconfig.py
	@if [ ! -f configs/$@ ]; then \
	    echo "Error: configs/$@ not found"; \
	    echo "Available:"; \
	    ls configs/*_defconfig 2>/dev/null | sed 's|configs/||'; \
	    exit 1; \
	fi
	@python3 $(KCONFIG_DIR)/defconfig.py --kconfig $(KCONFIG) configs/$@
	@echo "Configuration '$@' applied"
	$(rebuild-if-needed)

oldconfig: $(KCONFIG_DIR)/oldconfig.py
	@python3 $(KCONFIG_DIR)/oldconfig.py $(KCONFIG)
	$(rebuild-if-needed)

savedefconfig: $(KCONFIG_DIR)/savedefconfig.py
	@python3 $(KCONFIG_DIR)/savedefconfig.py --kconfig $(KCONFIG) --out configs/defconfig
	@echo "Configuration saved to configs/defconfig"

help:
	@echo "Thread-Metric Benchmark Build System"
	@echo ""
	@echo "Configuration (run once before building):"
	@echo "  make defconfig                    - Apply default config (ThreadX + POSIX host)"
	@echo "  make config                       - Interactive menuconfig"
	@echo "  make <name>_defconfig             - Apply a named defconfig from configs/"
	@echo "  make savedefconfig                - Save current config as new default"
	@echo ""
	@echo "Named defconfigs:"
	@echo "  threadx_posix_defconfig           - ThreadX + POSIX host"
	@echo "  threadx_cortex_m_defconfig        - ThreadX + Cortex-M3 QEMU"
	@echo "  freertos_posix_defconfig          - FreeRTOS + POSIX host"
	@echo "  freertos_cortex_m_defconfig       - FreeRTOS + Cortex-M3 QEMU"
	@echo ""
	@echo "Building:"
	@echo "  make                              - Build all test binaries"
	@echo "  make tm_basic_processing          - Build a single test"
	@echo "  make check                        - Build + smoke-test (1 s QEMU, 3 s host)"
	@echo "  make run                          - Run under QEMU (cortex-m-qemu only)"
	@echo "  make diagnose                     - Print build/QEMU environment diagnostics"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean                        - Remove binaries and build directory"
	@echo "  make distclean                    - clean + remove .config, cloned RTOS trees"
	@echo ""
	@echo "Overrides:"
	@echo "  make TM_TEST_DURATION=5           - Set reporting interval"
	@echo "  make TM_TEST_CYCLES=1             - Set number of cycles (1 = single report)"
	@echo "  make CROSS_COMPILE=/path/prefix-  - Set cross-compiler prefix"
	@echo "  make V=1                          - Verbose build output"

.PHONY: config defconfig oldconfig savedefconfig check run diagnose help clean-bins
