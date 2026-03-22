# Toolchain, flags, and build infrastructure
#
# Reads CONFIG_TARGET_* from .config to select the toolchain and
# architecture-specific flags.  Provides verbosity control, build
# directory management, and QEMU semihosting defaults.

MAKEFLAGS += --no-builtin-rules --no-builtin-variables

BUILD := build

# Verbosity: V=1 shows full commands
ifeq ($(V),1)
  Q :=
else
  Q := @
  MAKEFLAGS += --no-print-directory
endif

# Cross-compilation
CROSS_COMPILE ?=

ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  # Auto-detect ARM cross-compiler when CROSS_COMPILE is not set.
  # Probe PATH first, then well-known installation directories:
  #   /usr/bin                              Debian/Ubuntu apt package
  #   /usr/local/bin                        manual install / FreeBSD
  #   /opt/homebrew/bin                     Homebrew on Apple Silicon
  #   /opt/toolchain/arm-none-eabi/bin      custom install (see CLAUDE.md)
  ifeq ($(CROSS_COMPILE),)
    ifneq ($(shell which arm-none-eabi-gcc 2>/dev/null),)
      CROSS_COMPILE := arm-none-eabi-
    else ifneq ($(wildcard /usr/bin/arm-none-eabi-gcc),)
      CROSS_COMPILE := /usr/bin/arm-none-eabi-
    else ifneq ($(wildcard /usr/local/bin/arm-none-eabi-gcc),)
      CROSS_COMPILE := /usr/local/bin/arm-none-eabi-
    else ifneq ($(wildcard /opt/homebrew/bin/arm-none-eabi-gcc),)
      CROSS_COMPILE := /opt/homebrew/bin/arm-none-eabi-
    else
      CROSS_COMPILE := arm-none-eabi-
    endif
  endif

  # Auto-detect QEMU when not set.
  QEMU ?=
  ifeq ($(QEMU),)
    ifneq ($(shell which qemu-system-arm 2>/dev/null),)
      QEMU := qemu-system-arm
    else ifneq ($(wildcard /usr/bin/qemu-system-arm),)
      QEMU := /usr/bin/qemu-system-arm
    else ifneq ($(wildcard /usr/local/bin/qemu-system-arm),)
      QEMU := /usr/local/bin/qemu-system-arm
    else ifneq ($(wildcard /opt/homebrew/bin/qemu-system-arm),)
      QEMU := /opt/homebrew/bin/qemu-system-arm
    else
      QEMU := qemu-system-arm
    endif
  endif
endif

# Apply CROSS_COMPILE prefix.  Respect explicit user values (CC=clang)
# via $(origin); check both 'default' and 'undefined' (the latter
# appears in sub-makes inheriting --no-builtin-variables).
ifneq ($(filter default undefined,$(origin CC)),)
  CC := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)gcc,cc)
endif
ifneq ($(filter default undefined,$(origin AR)),)
  AR := $(if $(CROSS_COMPILE),$(CROSS_COMPILE)ar,ar)
endif

# Validate cross-compiler for Cortex-M build goals.
# _NEED_CONFIG and _HAS_CONFGEN are computed in the main Makefile before
# this file is included.  Skip when a config generator will re-invoke make.
ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  ifneq ($(_NEED_CONFIG),)
    ifeq ($(_HAS_CONFGEN),)
    ifeq ($(shell which $(CC) 2>/dev/null),)
      $(info )
      $(info *** Cross-compiler '$(CC)' not found!)
      $(info *** Install arm-none-eabi-gcc or set CROSS_COMPILE:)
      $(info ***   make CROSS_COMPILE=/opt/toolchain/arm-none-eabi/bin/arm-none-eabi-)
      $(info )
      $(error Missing cross-compiler)
    endif
    endif
  endif
endif

# Base CFLAGS (user CFLAGS appended below)
CFLAGS_BASE := -Wall

ifeq ($(CONFIG_OPTIMIZE_SIZE),y)
  CFLAGS_BASE += -Os
else
  CFLAGS_BASE += -O2
endif

ifeq ($(CONFIG_DEBUG_SYMBOLS),y)
  CFLAGS_BASE += -g
endif

ifeq ($(CONFIG_TARGET_CORTEX_M_QEMU),y)
  CFLAGS_BASE += -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
  LDFLAGS     += -T ports/common/cortex-m/mps2_an385.ld -nostartfiles
  LDFLAGS     += --specs=rdimon.specs

  QEMU_SEMIHOSTING_TARGET ?= native
  QEMU_FLAGS = -semihosting-config enable=on,target=$(QEMU_SEMIHOSTING_TARGET)
endif

ifeq ($(CONFIG_TARGET_POSIX_HOST),y)
  LDFLAGS += -lpthread
endif

ifeq ($(CONFIG_SANITIZERS),y)
  CFLAGS_BASE += -fsanitize=address,undefined -fno-omit-frame-pointer
  LDFLAGS     += -fsanitize=address,undefined
endif

# Base flags always present even when the user passes CFLAGS on the
# command line; their flags are appended.
override CFLAGS := $(CFLAGS_BASE) $(CFLAGS)

$(BUILD):
	$(Q)mkdir -p $@

clean-build:
	rm -rf $(BUILD)

.PHONY: clean-build
