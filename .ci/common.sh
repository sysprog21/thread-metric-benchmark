# Bash strict mode (enabled only when executed directly, not sourced)
if ! (return 0 2> /dev/null); then
    set -euo pipefail
fi

# Expect host is Linux/x86_64, Linux/aarch64, macOS/arm64

MACHINE_TYPE=$(uname -m)
OS_TYPE=$(uname -s)

check_platform()
{
    case "${MACHINE_TYPE}/${OS_TYPE}" in
        x86_64/Linux | aarch64/Linux | arm64/Darwin) ;;

        *)
            echo "Unsupported platform: ${MACHINE_TYPE}/${OS_TYPE}"
            exit 1
            ;;
    esac
}

if [ "${OS_TYPE}" = "Linux" ]; then
    PARALLEL=-j$(nproc)
else
    PARALLEL=-j$(sysctl -n hw.logicalcpu)
fi

# Color output helpers
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_success()
{
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error()
{
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

print_warning()
{
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Cleanup function registry
CLEANUP_FUNCS=()

register_cleanup()
{
    CLEANUP_FUNCS+=("$1")
}

cleanup()
{
    local func
    for func in "${CLEANUP_FUNCS[@]-}"; do
        [ -n "${func}" ] || continue
        eval "${func}" || true
    done
}

trap cleanup EXIT
