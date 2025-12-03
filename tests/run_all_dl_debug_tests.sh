#!/bin/bash
#
# Minimal orchestration script that keeps the historical
# `run_all_dl_debug_tests.sh` entry point alive while adding the
# `libc_default_compartment` regression to the sweep.
#
# By default it:
#   1. Ensures the build tree is configured with IA2_DEBUG=ON and
#      IA2_LIBC_COMPARTMENT=ON (matching CI defaults).
#   2. Runs the existing dl_debug_test suite via
#      tests/dl_debug_test/run_all_tests.sh.
#   3. Builds and runs the new libc_default_compartment scenario to
#      prove shared-compartment libraries (pkey 0) can still call libc.
#
# Options:
#   --verbose      Forward verbose output to the dl_debug runner.
#   --libc-only    Skip dl_debug_test and only exercise libc_default_compartment.
#   --skip-libc    Run dl_debug_test but skip the libc_default_compartment check.
#   --help         Show this message.
#
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
BUILD_DIR="$PROJECT_ROOT/build"

DL_DEBUG_RUNNER="$PROJECT_ROOT/tests/dl_debug_test/run_all_tests.sh"
LIBC_TARGET="libc_default_compartment"
LIBC_BUILD_DIR="$BUILD_DIR/tests/$LIBC_TARGET"
LIBC_BINARY="$LIBC_BUILD_DIR/$LIBC_TARGET"
LIBC_TEST_NAME="library_stays_in_pkey0"

VERBOSE=0
RUN_DL_DEBUG=1
RUN_LIBC=1

usage() {
    cat <<EOF
Usage: $0 [--verbose] [--libc-only] [--skip-libc]

Runs the dl_debug_test suite plus libc_default_compartment regression.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --verbose) VERBOSE=1 ;;
        --libc-only) RUN_DL_DEBUG=0 ;;
        --skip-libc) RUN_LIBC=0 ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage
            exit 1
            ;;
    esac
    shift
done

detect_llvm_args() {
    for version in 18 17 16 15 14 13 12 11; do
        local clang_dir="/usr/lib/llvm-${version}/lib/cmake/clang"
        if [[ -d "$clang_dir" ]]; then
            echo "-DClang_DIR=${clang_dir} -DLLVM_DIR=/usr/lib/llvm-${version}/lib/cmake/llvm"
            return 0
        fi
    done
    echo ""
}

ensure_configured() {
    mkdir -p "$BUILD_DIR"

    local cache="$BUILD_DIR/CMakeCache.txt"
    local needs_reconfigure=0

    if [[ ! -f "$cache" ]] || [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
        needs_reconfigure=1
    else
        grep -q "IA2_DEBUG:BOOL=ON" "$cache" || needs_reconfigure=1
        grep -q "IA2_LIBC_COMPARTMENT:BOOL=ON" "$cache" || needs_reconfigure=1
    fi

    if [[ $needs_reconfigure -eq 0 ]]; then
        return
    fi

    echo "[configure] Generating build directory (IA2_DEBUG=ON, IA2_LIBC_COMPARTMENT=ON)"

    local cmake_cmd=(cmake -GNinja)
    if [[ -n "${CMAKE_ARGS:-}" ]]; then
        # shellcheck disable=SC2206
        cmake_cmd+=(${CMAKE_ARGS})
    else
        local detected
        detected=$(detect_llvm_args)
        if [[ -n "$detected" ]]; then
            # shellcheck disable=SC2206
            cmake_cmd+=($detected)
        fi
    fi
    cmake_cmd+=(-DIA2_DEBUG=ON -DIA2_LIBC_COMPARTMENT=ON ..)

    (
        cd "$BUILD_DIR"
        "${cmake_cmd[@]}"
    )
}

build_target() {
    ensure_configured
    echo "[build] ninja $1"
    (cd "$BUILD_DIR" && ninja "$1")
}

run_dl_debug_suite() {
    if [[ $RUN_DL_DEBUG -eq 0 ]]; then
        echo "[skip] dl_debug_test suite"
        return
    fi

    ensure_configured

    if [[ ! -x "$DL_DEBUG_RUNNER" ]]; then
        echo "Missing runner: $DL_DEBUG_RUNNER" >&2
        exit 1
    fi

    echo "======================================================================"
    echo "Running dl_debug_test suite"
    echo "======================================================================"

    local args=()
    if [[ $VERBOSE -eq 1 ]]; then
        args+=(--verbose)
    fi

    "$DL_DEBUG_RUNNER" "${args[@]}"
}

run_libc_default_compartment() {
    if [[ $RUN_LIBC -eq 0 ]]; then
        echo "[skip] libc_default_compartment"
        return
    fi

    echo "======================================================================"
    echo "Running libc_default_compartment regression"
    echo "======================================================================"

    build_target "$LIBC_TARGET"

    if [[ ! -x "$LIBC_BINARY" ]]; then
        echo "Missing binary: $LIBC_BINARY" >&2
        exit 1
    fi

    (
        cd "$LIBC_BUILD_DIR"
        IA2_TEST_NAME="$LIBC_TEST_NAME" "./$LIBC_TARGET"
    )
    echo "[pass] libc_default_compartment"
}

run_dl_debug_suite
run_libc_default_compartment

echo "All requested dl_debug scenarios completed successfully."
