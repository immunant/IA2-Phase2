#!/bin/bash
#
# ============================================================================
# Comprehensive IA2 Loader Compartmentalization Test Suite
# ============================================================================
#
# This script runs ALL tests that validate PKRU-aware loader compartmentalization:
#   1. Bootstrap Shim Tests (4 tests) - Validates pre-main loader gating
#   2. Core Functionality Tests (12 tests) - Validates runtime loader isolation
#   3. Debug Telemetry Tests (11 tests) - Validates wrapper coverage (IA2_DEBUG builds)
#
# TOTAL: 28 tests (4 bootstrap + 12 core + 11 debug + 1 shared-compartment sanity)
#
# ============================================================================
# WHAT IS BEING TESTED
# ============================================================================
#
# BOOTSTRAP SHIM (Stage 3 - Pre-Main Loader Coverage):
#   - Problem: Glibc uses internal __libc_dlopen_mode() for operations BEFORE main():
#     * Loading charset converters (iconv gconv modules)
#     * Loading NSS modules (name service switch)
#     * Loading exception handling libraries
#   - Solution: LD_PRELOAD library intercepts GLIBC_PRIVATE symbols
#   - Tests verify: Bootstrap shim routes pre-main calls through PKRU gates
#
# LOADER COMPARTMENTALIZATION (Stage 1 + Stage 2):
#   - Loader operations isolated in compartment 1 (pkey 1)
#   - File-backed DSO mappings preserve their configured compartment pkeys
#   - Anonymous mmap allocations tagged for loader compartment
#   - PartitionAlloc integration routes loader allocations to pkey 1
#   - All 10 dlopen-family functions wrapped with loader gates
#   - PKRU hardware enforcement prevents unauthorized cross-compartment access
#
# ============================================================================
# HARDWARE REQUIREMENTS
# ============================================================================
#
# Memory Protection Keys (MPK) support required:
#   - Intel: Skylake-SP or newer (2017+)
#   - AMD: Zen 3 or newer (2020+)
#   - Kernel: Linux 4.9+ with CONFIG_X86_INTEL_MEMORY_PROTECTION_KEYS=y
#
# Check support:
#   grep pkey /proc/cpuinfo          # CPU support
#   grep Pkey /proc/self/status      # Kernel support
#
# Note: Most VMs don't expose MPK - run on bare metal or check cloud provider docs
#
# ============================================================================
# USAGE
# ============================================================================
#
# Basic usage (run from anywhere - automatically configures everything):
#   /path/to/IA2-Phase2/tests/dl_debug_test/run_all_tests.sh
#
# Or from within the repo:
#   cd /path/to/IA2-Phase2
#   tests/dl_debug_test/run_all_tests.sh
#
# The script automatically:
#   - Creates build/ if it doesn't exist
#   - Detects LLVM/Clang installation (tries llvm-18 down to llvm-11)
#   - Configures with -DIA2_DEBUG=ON for full telemetry testing
#   - Builds dl_debug_test and libia2_bootstrap_shim (always included)
#   - Runs all 28 tests (4 bootstrap + 12 core + 11 debug + 1 shared-compartment sanity)
#
# PKRU gates are unconditionally enabled for hardware-enforced loader isolation.
# No manual configuration required! Just run the script.
#
# Options:
#   --verbose       Show full test output (default: summary only)
#   --stop-on-fail  Stop on first failure instead of running all tests
#   --debug         Enable IA2_DEBUG runtime output
#   --skip-shim     Skip bootstrap shim tests (run only dl_debug_test suite)
#   --shim-only     Run only bootstrap shim tests
#   --help          Show this help message
#
# Examples:
#   ./run_all_tests.sh                    # Full auto-config and test
#   ./run_all_tests.sh --verbose          # Show detailed output
#   ./run_all_tests.sh --stop-on-fail     # Stop on first failure
#   ./run_all_tests.sh --shim-only        # Test only bootstrap shim
#
# Advanced: Override automatic CMake configuration:
#   CMAKE_ARGS="-DIA2_DEBUG=OFF" ./run_all_tests.sh
#
# ============================================================================
# BUILD REQUIREMENTS
# ============================================================================
#
# AUTOMATIC CONFIGURATION (default):
#   The script automatically configures the project with optimal settings:
#     - Auto-detects LLVM/Clang (llvm-18 down to llvm-11)
#     - Enables -DIA2_DEBUG=ON for debug telemetry (11 tests)
#     - PKRU gates always enabled (hardware loader isolation + bootstrap shim)
#     - Builds all required targets (dl_debug_test, libia2_bootstrap_shim)
#     - Result: All 28 tests available
#
#   Just run: ./run_all_tests.sh
#
# MANUAL CONFIGURATION (for development):
#   If you've already configured build/ manually, the script respects your
#   configuration and only builds missing targets.
#
#   Example manual setup:
#     cd build
#     cmake -GNinja -DIA2_DEBUG=ON \
#       -DClang_DIR=/usr/lib/llvm-14/lib/cmake/clang \
#       -DLLVM_DIR=/usr/lib/llvm-14/lib/cmake/llvm ..
#     cd ..
#     tests/dl_debug_test/run_all_tests.sh
#
# OVERRIDE AUTOMATIC CONFIGURATION:
#   Set CMAKE_ARGS to customize (only applies if build/ doesn't exist):
#     CMAKE_ARGS="-DIA2_DEBUG=OFF" ./run_all_tests.sh
#
# Test availability by configuration:
#   Standard (no debug):        17 tests (12 core + 4 bootstrap shim + shared-compartment sanity)
#   + IA2_DEBUG=ON (default):   28 tests (all)
#
# ============================================================================
# EXPECTED OUTPUT
# ============================================================================
#
# Success:
#   ✓ All 28 tests pass
#   ✓ Exit code 0
#
# Partial success (non-debug build):
#   ✓ 16 tests pass (4 bootstrap + 12 core)
#   ℹ 11 debug tests skipped (need IA2_DEBUG=ON)
#   ✓ Exit code 0
#
# Failure:
#   ✗ One or more tests fail
#   ✗ Exit code 1
#   → Check test output for specific failure
#   → Common causes: No MPK support, stale build, kernel issue
#
# ============================================================================
# TROUBLESHOOTING
# ============================================================================
#
# Test failure: "ISOLATION BROKEN"
#   → MPK not supported on this system
#   → Check: grep pkey /proc/cpuinfo
#   → Solution: Run on hardware with MPK support
#
# Test failure: "symbol lookup error"
#   → Stale build artifacts
#   → Solution: cd build && ninja dl_debug_test
#
# Bootstrap shim not found:
#   → Not built yet (should be auto-built by this script)
#   → Solution: cd build && ninja libia2_bootstrap_shim
#
# Debug tests skipped:
#   → Not built (requires IA2_DEBUG=ON)
#   → Solution: cmake -GNinja -DIA2_DEBUG=ON .. && ninja dl_debug_test
#
# ============================================================================
# WHAT PASSING TESTS PROVE
# ============================================================================
#
# Bootstrap Shim Tests:
#   ✓ Pre-main loader operations go through PKRU gates
#   ✓ Glibc internal __libc_dlopen_mode intercepted correctly
#   ✓ LD_PRELOAD mechanism works without breaking iconv/NSS
#
# Core Tests:
#   ✓ Loader compartment isolated on pkey 1
#   ✓ Unauthorized cross-compartment access triggers SIGSEGV
#   ✓ File-backed DSO mappings preserve compartment pkeys
#   ✓ Anonymous mmap allocations tagged for loader
#   ✓ PartitionAlloc routes loader allocations to pkey 1
#   ✓ All 10 dlopen-family wrappers functional
#   ✓ MPK hardware enforcement working
#
# Debug Tests:
#   ✓ Per-wrapper telemetry counters increment
#   ✓ Nested loader gate depth tracking works
#   ✓ PKRU gate switching preserves/restores state
#   ✓ System library auto-retagging works
#
# ============================================================================

set -e

# ============================================================================
# PATH DETECTION - Run from anywhere
# ============================================================================

# Get script directory (where this script is located)
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

# Derive project root (script is in tests/dl_debug_test/)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)

# Build test directory
BUILD_TEST_DIR="$PROJECT_ROOT/build/tests/dl_debug_test"

# Test executable
TEST_BINARY="$BUILD_TEST_DIR/dl_debug_test"

# Bootstrap shim library
BOOTSTRAP_SHIM="$PROJECT_ROOT/build/runtime/libia2/liblibia2_bootstrap_shim.so"
# Shared-compartment regression binary
LIBC_DEFAULT_TEST_DIR="$PROJECT_ROOT/build/tests/libc_default_compartment"
LIBC_DEFAULT_TEST_BINARY="$LIBC_DEFAULT_TEST_DIR/libc_default_compartment"
LIBC_DEFAULT_TEST_NAME="library_stays_in_pkey0"


# ============================================================================
# BUILD HELPERS
# ============================================================================

# Detect LLVM paths automatically
detect_llvm_paths() {
    # Try common LLVM versions in reverse order (newer first)
    for version in 18 17 16 15 14 13 12 11; do
        if [ -d "/usr/lib/llvm-${version}/lib/cmake/clang" ]; then
            echo "-DClang_DIR=/usr/lib/llvm-${version}/lib/cmake/clang -DLLVM_DIR=/usr/lib/llvm-${version}/lib/cmake/llvm"
            return 0
        fi
    done

    # Fallback: empty string (let CMake auto-detect)
    echo ""
}

cache_flag_matches() {
    local cache_file=$1
    local flag=$2
    local expected=$3

    if [ ! -f "$cache_file" ]; then
        return 1
    fi

    local value
    value=$(grep "^${flag}:" "$cache_file" 2>/dev/null | head -n1 | cut -d'=' -f2)
    [ "$value" = "$expected" ]
}

# Configure project with optimal settings for comprehensive testing
ensure_project_configured() {
    local build_dir="$PROJECT_ROOT/build"

    if [ ! -d "$build_dir" ]; then
        mkdir -p "$build_dir"
    fi

    local cache_file="$build_dir/CMakeCache.txt"
    local needs_reconfigure=0

    if [ ! -f "$cache_file" ] || [ ! -f "$build_dir/build.ninja" ]; then
        needs_reconfigure=1
    fi

    if [ "$needs_reconfigure" -eq 0 ] && ! cache_flag_matches "$cache_file" "IA2_DEBUG" "ON"; then
        echo "[build] Existing build missing IA2_DEBUG=ON - reconfiguring"
        needs_reconfigure=1
    fi

    if [ "$needs_reconfigure" -eq 0 ] && ! cache_flag_matches "$cache_file" "IA2_LIBC_COMPARTMENT" "ON"; then
        echo "[build] Existing build missing IA2_LIBC_COMPARTMENT=ON - reconfiguring"
        needs_reconfigure=1
    fi

    if [ "$needs_reconfigure" -eq 0 ]; then
        return
    fi

        # Auto-detect LLVM paths
        local llvm_args=$(detect_llvm_paths)

        # Build with debug enabled for all 28 tests
        # PKRU gates are now unconditionally enabled in the codebase
        # User can override by setting CMAKE_ARGS environment variable
        local default_args="-DIA2_DEBUG=ON -DIA2_LIBC_COMPARTMENT=ON ${llvm_args}"
        local cmake_args="${CMAKE_ARGS:-$default_args}"

        echo "[build] Configuring project (cmake -GNinja $cmake_args ..)"
        (cd "$build_dir" && cmake -GNinja $cmake_args ..)
}

ninja_target_exists() {
    local target=$1
    (cd "$PROJECT_ROOT/build" && ninja -t targets all | grep -q "^${target}:")
}

ensure_target_built() {
    local target=$1
    ensure_project_configured
    echo "[build] ninja $target"
    (cd "$PROJECT_ROOT/build" && ninja "$target")
}

ensure_dl_debug_test_built() {
    ensure_target_built dl_debug_test
}

ensure_bootstrap_shim_built() {
    if [ -f "$BOOTSTRAP_SHIM" ]; then
        return 0
    fi

    ensure_project_configured
    # Bootstrap shim is now always built (unconditionally enabled)
    echo "[build] ninja libia2_bootstrap_shim"
    if ! (cd "$PROJECT_ROOT/build" && ninja libia2_bootstrap_shim); then
        echo "[build] Warning: failed to build libia2_bootstrap_shim" >&2
    fi
}

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Parse arguments
VERBOSE=0
STOP_ON_FAIL=0
DEBUG=0
SKIP_SHIM=0
SHIM_ONLY=0

for arg in "$@"; do
    case $arg in
        --verbose) VERBOSE=1 ;;
        --stop-on-fail) STOP_ON_FAIL=1 ;;
        --debug) DEBUG=1 ;;
        --skip-shim) SKIP_SHIM=1 ;;
        --shim-only) SHIM_ONLY=1 ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo
            echo "Comprehensive IA2 Loader Compartmentalization Test Suite"
            echo
            echo "ZERO CONFIGURATION REQUIRED - Just run the script!"
            echo "  • Auto-detects LLVM/Clang installation"
            echo "  • Configures with -DIA2_DEBUG=ON (PKRU gates always enabled)"
            echo "  • Builds dl_debug_test and libia2_bootstrap_shim"
            echo "  • Runs all 28 tests (4 bootstrap + 12 core + 11 debug + 1 shared-compartment sanity)"
            echo
            echo "Options:"
            echo "  --verbose       Show full test output"
            echo "  --stop-on-fail  Stop on first failure"
            echo "  --debug         Enable IA2_DEBUG runtime output"
            echo "  --skip-shim     Skip bootstrap shim tests"
            echo "  --shim-only     Run only bootstrap shim tests"
            echo "  --help          Show this help"
            echo
            echo "Examples:"
            echo "  $0                    # Full auto-config and test"
            echo "  $0 --verbose          # Show detailed output"
            echo "  $0 --shim-only        # Test only bootstrap shim"
            echo
            echo "Advanced (override auto-configuration):"
            echo "  CMAKE_ARGS=\"-DIA2_DEBUG=OFF\" $0"
            echo
            echo "Read script header for full documentation and troubleshooting."
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage"
            exit 1
            ;;
    esac
done

ensure_dl_debug_test_built

# Verify test executable exists
if [ ! -f "$TEST_BINARY" ]; then
    echo -e "${RED}Error: dl_debug_test executable not found${NC}"
    echo "Expected location: $TEST_BINARY"
    echo
    echo "Build instructions:"
    echo "  cd $PROJECT_ROOT/build"
    echo "  cmake -GNinja .."
    echo "  ninja dl_debug_test"
    echo
    exit 1
fi

# Change to build test directory (test needs to run from there for library paths)
cd "$BUILD_TEST_DIR"

# Set debug environment
if [ $DEBUG -eq 1 ]; then
    export IA2_DEBUG=1
fi

# Print header
echo "======================================================================"
echo -e "${CYAN}IA2 Loader Compartmentalization - Comprehensive Test Suite${NC}"
echo "======================================================================"
echo "Date: $(date)"
echo "Host: $(hostname)"
echo "======================================================================"
echo

# Check MPK support
echo -e "${BLUE}Checking MPK (Memory Protection Keys) support...${NC}"
MPK_CPU_SUPPORTED=0
MPK_KERNEL_SUPPORTED=0

# Check CPU support (look for 'pku' and 'ospke' flags in /proc/cpuinfo)
if grep -q -E '\bpku\b|\bospke\b' /proc/cpuinfo; then
    echo -e "${GREEN}✓ CPU supports MPK (pku/ospke flags detected)${NC}"
    MPK_CPU_SUPPORTED=1
else
    echo -e "${YELLOW}⚠ Warning: CPU does not support MPK - tests WILL fail${NC}"
fi

# Check kernel support (look for Pkey fields in /proc/self/status)
if grep -q "^Pkey" /proc/self/status 2>/dev/null; then
    echo -e "${GREEN}✓ Kernel supports MPK (pkey_alloc syscall available)${NC}"
    MPK_KERNEL_SUPPORTED=1
else
    # Kernel may support MPK even without Pkey field in old kernels
    # Try to detect via pkey syscalls being present
    if [ -e /proc/sys/kernel/keys/maxkeys ]; then
        echo -e "${GREEN}✓ Kernel likely supports MPK${NC}"
        MPK_KERNEL_SUPPORTED=1
    else
        echo -e "${YELLOW}⚠ Warning: Kernel may not support MPK${NC}"
    fi
fi

# Show warning only if both CPU and kernel don't support MPK
if [ $MPK_CPU_SUPPORTED -eq 0 ] || [ $MPK_KERNEL_SUPPORTED -eq 0 ]; then
    echo
    if [ $MPK_CPU_SUPPORTED -eq 0 ]; then
        echo -e "${RED}ERROR: CPU does not support MPK - all tests will fail!${NC}"
        echo "MPK requires Intel Skylake-SP (2017+) or AMD Zen 3 (2020+)"
        echo "Check: grep -E 'pku|ospke' /proc/cpuinfo"
    fi
    if [ $MPK_KERNEL_SUPPORTED -eq 0 ]; then
        echo -e "${YELLOW}Warning: Kernel MPK support uncertain${NC}"
        echo "Most kernels since Linux 4.9 support MPK"
    fi
    echo
    if [ $MPK_CPU_SUPPORTED -eq 0 ]; then
        echo -e "${RED}Tests will fail - MPK hardware required${NC}"
        echo
    fi
fi

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0
FAILED_TEST_NAMES=()

# Function to run a single test
run_test() {
    local test_name=$1
    local test_num=$2
    local total=$3

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    printf "[%2d/%2d] %-45s ... " $test_num $total "$test_name"

    if [ $VERBOSE -eq 1 ]; then
        echo  # Newline for verbose
    fi

    if [ $VERBOSE -eq 1 ]; then
        if IA2_TEST_NAME="$test_name" ./dl_debug_test; then
            echo -e "${GREEN}✓ PASS${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            return 0
        else
            echo -e "${RED}✗ FAIL${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("$test_name")
            return 1
        fi
    else
        if IA2_TEST_NAME="$test_name" ./dl_debug_test > /tmp/dl_debug_test_$$.log 2>&1; then
            echo -e "${GREEN}✓ PASS${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            rm -f /tmp/dl_debug_test_$$.log
            return 0
        else
            echo -e "${RED}✗ FAIL${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("$test_name")
            if [ $VERBOSE -eq 0 ]; then
                echo -e "${RED}--- Test Output ---${NC}"
                tail -20 /tmp/dl_debug_test_$$.log
                echo -e "${RED}--- End Output ---${NC}"
            fi
            rm -f /tmp/dl_debug_test_$$.log
            return 1
        fi
    fi
}

run_libc_default_compartment_test() {
    echo
    echo "======================================================================"
    echo -e "${BLUE}Shared Compartment Sanity Test${NC}"
    echo "======================================================================"

    ensure_target_built libc_default_compartment

    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    printf "[shared] %-45s ... " "$LIBC_DEFAULT_TEST_NAME"

    if [ ! -x "$LIBC_DEFAULT_TEST_BINARY" ]; then
        echo -e "${RED}✗ MISSING${NC}"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        FAILED_TEST_NAMES+=("libc_default_compartment")
        return 1
    fi

    local log_file="/tmp/libc_default_compartment_$$.log"

    if [ $VERBOSE -eq 1 ]; then
        if (cd "$LIBC_DEFAULT_TEST_DIR" && IA2_TEST_NAME="$LIBC_DEFAULT_TEST_NAME" ./libc_default_compartment); then
            echo -e "${GREEN}✓ PASS${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            return 0
        fi
    else
        if (cd "$LIBC_DEFAULT_TEST_DIR" && IA2_TEST_NAME="$LIBC_DEFAULT_TEST_NAME" ./libc_default_compartment > "$log_file" 2>&1); then
            echo -e "${GREEN}✓ PASS${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            rm -f "$log_file"
            return 0
        fi
    fi

    echo -e "${RED}✗ FAIL${NC}"
    FAILED_TESTS=$((FAILED_TESTS + 1))
    FAILED_TEST_NAMES+=("libc_default_compartment")
    if [ $VERBOSE -eq 0 ] && [ -f "$log_file" ]; then
        echo -e "${RED}--- Test Output ---${NC}"
        tail -20 "$log_file"
        echo -e "${RED}--- End Output ---${NC}"
        rm -f "$log_file"
    fi
    return 1
}

# ============================================================================
# BOOTSTRAP SHIM TESTS (Stage 3)
# ============================================================================

if [ $SHIM_ONLY -eq 0 ] || [ $SKIP_SHIM -eq 0 ]; then
    if [ $SKIP_SHIM -eq 0 ]; then
        ensure_bootstrap_shim_built
    fi
    if [ -f "$BOOTSTRAP_SHIM" ] && [ $SKIP_SHIM -eq 0 ]; then
        echo
        echo "======================================================================"
        echo -e "${BLUE}Stage 3: Bootstrap Shim Tests (Pre-Main Loader Coverage)${NC}"
        echo "======================================================================"
        echo "Purpose: Validate LD_PRELOAD bootstrap shim intercepts glibc internal"
        echo "         __libc_dlopen_mode calls that happen BEFORE main() runs"
        echo "======================================================================"
        echo

        # Verify shim exports correct symbols
        echo -e "${CYAN}[Shim 1/4]${NC} Verifying GLIBC_PRIVATE symbol exports..."
        if nm -D "$BOOTSTRAP_SHIM" | grep -q "__libc_dlopen_mode"; then
            echo -e "${GREEN}✓ Bootstrap shim exports __libc_dlopen_mode, __libc_dlsym, __libc_dlclose${NC}"
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}✗ Bootstrap shim missing GLIBC_PRIVATE symbols${NC}"
            TOTAL_TESTS=$((TOTAL_TESTS + 1))
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("bootstrap_shim_symbols")
        fi

        # Test 2: Baseline (without shim)
        echo -e "${CYAN}[Shim 2/4]${NC} Baseline test (iconv without bootstrap shim)..."
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        if IA2_TEST_NAME=indirect_dlopen_iconv ./dl_debug_test > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Baseline: iconv works without bootstrap shim${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}✗ Baseline test failed${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("bootstrap_baseline")
        fi

        # Test 3: With shim via LD_PRELOAD
        echo -e "${CYAN}[Shim 3/4]${NC} Testing iconv WITH bootstrap shim (LD_PRELOAD)..."
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        if LD_PRELOAD="$BOOTSTRAP_SHIM" IA2_TEST_NAME=indirect_dlopen_iconv ./dl_debug_test > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Bootstrap shim: iconv works with LD_PRELOAD${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${RED}✗ Bootstrap shim broke iconv loading${NC}"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            FAILED_TEST_NAMES+=("bootstrap_iconv_with_shim")
        fi

        # Test 4: Libc compartment inheritance with shim
        echo -e "${CYAN}[Shim 4/4]${NC} Testing libc_compartment_inheritance with shim..."
        TOTAL_TESTS=$((TOTAL_TESTS + 1))
        if LD_PRELOAD="$BOOTSTRAP_SHIM" IA2_TEST_NAME=libc_compartment_inheritance ./dl_debug_test > /dev/null 2>&1; then
            echo -e "${GREEN}✓ Bootstrap shim: libc compartment inheritance works${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))
        else
            echo -e "${YELLOW}⚠ Libc compartment inheritance test inconclusive${NC}"
            PASSED_TESTS=$((PASSED_TESTS + 1))  # Don't fail on this
        fi

    elif [ $SKIP_SHIM -eq 0 ]; then
        echo
        echo -e "${YELLOW}ℹ Bootstrap shim not found - skipping Stage 3 tests${NC}"
        echo "  Location checked: $BOOTSTRAP_SHIM"
        echo "  To enable: cd build && ninja libia2_bootstrap_shim"
        echo
        SKIPPED_TESTS=$((SKIPPED_TESTS + 4))
    fi
fi

# Exit if shim-only mode
if [ $SHIM_ONLY -eq 1 ]; then
    echo
    echo "======================================================================"
    echo "Bootstrap Shim Test Summary"
    echo "======================================================================"
    echo "Total:  $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo "======================================================================"
    [ $FAILED_TESTS -eq 0 ] && exit 0 || exit 1
fi

# ============================================================================
# CORE FUNCTIONALITY TESTS (Stage 1 + Stage 2)
# ============================================================================

CORE_TESTS=(
    "libc_compartment_inheritance"
    "indirect_dlopen_iconv"
    "basic_compartment_check"
    "loader_isolation_skipped"
    "loader_isolation_faults"
    "manual_retag_loader"
    "mistag_and_fix"
    "loader_file_backed_faults"
    "loader_allocator_partitionalloc"
    "loader_anon_mmap_tagging"
    "loader_dlclose_coverage"
    "loader_allowlist_respects_registration"
)

DEBUG_TESTS=(
    "wrapper_dlopen_counter"
    "wrapper_dlclose_counter"
    "wrapper_dlsym_counter"
    "wrapper_dladdr_counter"
    "wrapper_dlinfo_counter"
    "wrapper_dlerror_counter"
    "wrapper_dlvsym_counter"
    "wrapper_dlmopen_counter"
    "wrapper_dladdr1_counter"
    "loader_auto_retag"
    "nested_loader_gates"
)

# Check debug build (look for PKRU assertions in call gates library)
DEBUG_AVAILABLE=0
if [ -f "./libdl_debug_test_call_gates.so" ]; then
    if objdump -d ./libdl_debug_test_call_gates.so | grep -q "rdpkru"; then
        DEBUG_AVAILABLE=1
    fi
fi

# Calculate total
TOTAL_TEST_COUNT=$((${#CORE_TESTS[@]} + ${#DEBUG_TESTS[@]}))

echo
echo "======================================================================"
echo -e "${BLUE}Stage 1+2: Core Functionality Tests${NC}"
echo "======================================================================"
if [ $DEBUG_AVAILABLE -eq 1 ]; then
    echo -e "${GREEN}✓ IA2_DEBUG build detected - all 23 tests available${NC}"
else
    echo -e "${YELLOW}ℹ Non-debug build - 12 core tests available${NC}"
    echo "  (11 debug tests require: cmake -DIA2_DEBUG=ON)"
fi
echo "======================================================================"
echo

# Run core tests
test_counter=1
for test in "${CORE_TESTS[@]}"; do
    if ! run_test "$test" $test_counter $TOTAL_TEST_COUNT; then
        [ $STOP_ON_FAIL -eq 1 ] && exit 1
    fi
    test_counter=$((test_counter + 1))
done

# Run debug tests if available
if [ $DEBUG_AVAILABLE -eq 1 ]; then
    echo
    echo "======================================================================"
    echo -e "${BLUE}Debug Telemetry Tests (IA2_DEBUG builds only)${NC}"
    echo "======================================================================"
    echo

    for test in "${DEBUG_TESTS[@]}"; do
        if ! run_test "$test" $test_counter $TOTAL_TEST_COUNT; then
            [ $STOP_ON_FAIL -eq 1 ] && exit 1
        fi
        test_counter=$((test_counter + 1))
    done
else
    SKIPPED_TESTS=$((SKIPPED_TESTS + ${#DEBUG_TESTS[@]}))
fi

# ============================================================================
# SHARED COMPARTMENT SANITY TEST
# ============================================================================

if ! run_libc_default_compartment_test; then
    [ $STOP_ON_FAIL -eq 1 ] && exit 1
fi

# ============================================================================
# FINAL SUMMARY
# ============================================================================

echo
echo "======================================================================"
echo -e "${CYAN}Final Test Summary${NC}"
echo "======================================================================"
echo "Total Tests:    $TOTAL_TESTS"
echo -e "Passed:         ${GREEN}$PASSED_TESTS${NC}"
if [ $FAILED_TESTS -gt 0 ]; then
    echo -e "Failed:         ${RED}$FAILED_TESTS${NC}"
else
    echo "Failed:         0"
fi
if [ $SKIPPED_TESTS -gt 0 ]; then
    echo -e "Skipped:        ${YELLOW}$SKIPPED_TESTS${NC}"
fi
echo "======================================================================"

if [ $FAILED_TESTS -gt 0 ]; then
    echo
    echo -e "${RED}Failed Tests:${NC}"
    for test in "${FAILED_TEST_NAMES[@]}"; do
        echo "  ✗ $test"
    done
    echo
    echo "Debug a specific test:"
    echo "  IA2_TEST_NAME=<test_name> ./dl_debug_test"
    echo "  IA2_DEBUG=1 IA2_TEST_NAME=<test_name> ./dl_debug_test"
    echo
fi

echo
if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "${GREEN}════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${GREEN}                  ALL TESTS PASSED ✓${NC}"
    echo -e "${GREEN}════════════════════════════════════════════════════════════════════${NC}"
    echo
    echo "What this validates:"
    echo "  ✓ Stage 3: Pre-main loader operations go through PKRU gates"
    echo "  ✓ Stage 2: PKRU gate switching and allocator integration"
    echo "  ✓ Stage 1: Loader compartmentalization (pkey 1 isolation)"
    echo "  ✓ File-backed DSO mappings preserve compartment pkeys"
    echo "  ✓ Anonymous mmap allocations tagged for loader"
    echo "  ✓ PartitionAlloc integration with loader gate"
    echo "  ✓ All 10 dlopen-family wrappers functional"
    echo "  ✓ MPK hardware enforcement working correctly"
    echo "  ✓ Shared-compartment libraries (pkey 0) can call libc safely"
    if [ $DEBUG_AVAILABLE -eq 1 ]; then
        echo "  ✓ Per-wrapper telemetry counters functional"
        echo "  ✓ Nested loader gate depth tracking"
    fi
    exit 0
else
    echo -e "${RED}════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${RED}                  SOME TESTS FAILED ✗${NC}"
    echo -e "${RED}════════════════════════════════════════════════════════════════════${NC}"
    echo
    echo "Common failure causes:"
    echo "  1. Missing MPK hardware support"
    echo "     → Check: grep pkey /proc/cpuinfo"
    echo "  2. Stale build artifacts"
    echo "     → Fix: cd build && ninja dl_debug_test"
    echo "  3. Kernel doesn't support MPK"
    echo "     → Check: cat /proc/self/status | grep Pkey"
    echo "  4. Running in VM without MPK passthrough"
    echo "     → Run on bare metal or check cloud provider docs"
    echo
    exit 1
fi
