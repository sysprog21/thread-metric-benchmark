#!/usr/bin/env bash
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

set -euo pipefail

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

# Run QEMU under a shell-managed timeout instead of timeout(1).
# Some host timeout wrappers mis-handle QEMU semihosting exit and report a
# false timeout even though the guest reached SYS_EXIT successfully.
qemu_pid=""
watchdog_pid=""
timeout_flag=""

cleanup()
{
    if [ -n "$watchdog_pid" ]; then
        kill "$watchdog_pid" 2> /dev/null || true
        wait "$watchdog_pid" 2> /dev/null || true
        watchdog_pid=""
    fi
    if [ -n "$qemu_pid" ]; then
        kill "$qemu_pid" 2> /dev/null || true
        sleep 1
        kill -9 "$qemu_pid" 2> /dev/null || true
        qemu_pid=""
    fi
    [ -z "$timeout_flag" ] || rm -f "$timeout_flag"
}

# EXIT fires on normal exit.  INT/TERM handlers clear EXIT first to
# avoid double-invocation, then clean up and exit with the right code.
trap cleanup EXIT
trap 'trap - EXIT; cleanup; exit 130' INT
trap 'trap - EXIT; cleanup; exit 143' TERM

# The watchdog writes a sentinel file to distinguish its kill from an
# external signal.  Created via mktemp to avoid predictable paths.
timeout_flag=$(mktemp "${TMPDIR:-/tmp}/tm-qemu-timeout.XXXXXX")
rm -f "$timeout_flag"

set +e
"$QEMU" \
    -M mps2-an385 -cpu cortex-m3 -nographic \
    -kernel "$ELF" "$@" 2>&1 &
qemu_pid=$!

# The watchdog subshell redirects its own stdout/stderr to /dev/null
# so it does not hold the pipe open when this script is invoked inside
# $().  Without this, $() blocks until the watchdog's sleep finishes
# even after QEMU has exited.
(
    sleep "$QEMU_TIMEOUT"
    : > "$timeout_flag"
    kill "$qemu_pid" 2> /dev/null || true
    sleep 1
    kill -9 "$qemu_pid" 2> /dev/null || true
) > /dev/null 2>&1 &
watchdog_pid=$!

wait "$qemu_pid"
rc=$?
qemu_pid=""

# Reap the watchdog immediately so it cannot linger.
kill "$watchdog_pid" 2> /dev/null || true
wait "$watchdog_pid" 2> /dev/null || true
watchdog_pid=""
set -e

# Timeout: the watchdog creates the sentinel before killing QEMU.
# This distinguishes a watchdog kill from an external signal.
timed_out=0
if [ -f "$timeout_flag" ]; then
    timed_out=1
fi
rm -f "$timeout_flag"
timeout_flag=""

if [ $timed_out -eq 1 ]; then
    if [ $semihosting -eq 1 ]; then
        echo "FAIL: QEMU timed out after ${QEMU_TIMEOUT}s (semihosting exit never reached)" >&2
        echo "  QEMU    : $("$QEMU" --version 2>&1 | head -1)" >&2
        echo "  ELF     : $ELF" >&2
        echo "  flags   : $*" >&2
        echo "  timeout : internal watchdog ${QEMU_TIMEOUT}s" >&2
        echo "  Hint: verify -semihosting-config is in flags above, TM_TEST_CYCLES=1 in binary," >&2
        echo "        and QEMU supports ARM semihosting (qemu-system-arm >= 4.0)." >&2
        exit 1
    fi
    # UART mode: timeout is expected (tests loop forever).
    exit 0
fi

exit $rc
