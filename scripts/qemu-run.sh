#!/bin/sh
# Run a Thread-Metric ELF under QEMU mps2-an385 (Cortex-M3).
#
# Usage:
#   scripts/qemu-run.sh <elf> [extra-qemu-flags...]
#
# Semihosting mode (default when linked with --specs=rdimon.specs):
#   Pass "-semihosting-config enable=on,target=native" as extra flags.
#   If the test was built with TM_TEST_CYCLES=N, the reporter calls
#   exit(0) after N reports.  _exit() triggers ARM semihosting SYS_EXIT
#   (operation 0x18) and QEMU terminates with the program's exit code.
#   The script checks this exit code directly.
#
# UART mode (linked with --specs=nano.specs --specs=nosys.specs):
#   No semihosting flags.  Tests never self-terminate, so the script
#   kills QEMU after the timeout.
#
# Environment:
#   QEMU          -- path to qemu-system-arm (default: qemu-system-arm)
#   QEMU_TIMEOUT  -- outer timeout in seconds (default: 120)

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <elf> [extra-qemu-flags...]" >&2
    exit 1
fi

ELF="$1"
shift

QEMU="${QEMU:-qemu-system-arm}"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-120}"

if [ ! -f "$ELF" ]; then
    echo "Error: ELF not found: $ELF" >&2
    exit 1
fi

# Detect whether semihosting is among the extra flags.
semihosting=0
for arg in "$@"; do
    case "$arg" in
        *semihosting*) semihosting=1 ;;
    esac
done

# Locate a timeout command.  coreutils' timeout(1) is not shipped with
# macOS; Homebrew installs it as "gtimeout".
TIMEOUT_CMD=""
if command -v timeout >/dev/null 2>&1; then
    TIMEOUT_CMD="timeout"
elif command -v gtimeout >/dev/null 2>&1; then
    TIMEOUT_CMD="gtimeout"
else
    echo "Warning: neither timeout nor gtimeout found; running without timeout" >&2
fi

# Run QEMU with an outer timeout as a safety net.
set +e
if [ -n "$TIMEOUT_CMD" ]; then
    "$TIMEOUT_CMD" "$QEMU_TIMEOUT" "$QEMU" \
        -M mps2-an385 -cpu cortex-m3 -nographic \
        -kernel "$ELF" "$@" 2>&1
else
    "$QEMU" \
        -M mps2-an385 -cpu cortex-m3 -nographic \
        -kernel "$ELF" "$@" 2>&1
fi
rc=$?
set -e

if [ $rc -eq 124 ]; then
    # timeout(1) / gtimeout returns 124 when the child is killed.
    if [ $semihosting -eq 1 ]; then
        echo "FAIL: QEMU timed out (semihosting exit never reached)" >&2
        exit 1
    fi
    # UART mode: timeout is expected (tests loop forever).
    exit 0
fi

exit $rc
