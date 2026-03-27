#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-$repo_root/build/tls_one_page_repro_sweep}"
log_file="${2:-$repo_root/tests/tls_one_page_repro/attempt_log.md}"

main_sizes=(8192 16384 32768 65536)
shared_sizes=(4096 8192 12288 16384 24576)
lib2_sizes=(1 4096)

append_log() {
  printf -- "- %s | %s\n" "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" "$*" >> "$log_file"
}

if [ ! -f "$log_file" ]; then
  cat > "$log_file" <<'LOGHDR'
# TLS One-Page Repro Attempt Log

Append-only run log. Each entry includes exact knobs and result.
LOGHDR
fi

append_log "sweep-start build_dir=$build_dir"

for main_b in "${main_sizes[@]}"; do
  for shared_b in "${shared_sizes[@]}"; do
    for lib2_b in "${lib2_sizes[@]}"; do
      append_log "configure main=$main_b shared=$shared_b lib2=$lib2_b"
      cmake -S "$repo_root" -B "$build_dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
        -DTLS_REPRO_MAIN_TLS_BYTES="$main_b" \
        -DTLS_REPRO_SHARED_TLS_BYTES="$shared_b" \
        -DTLS_REPRO_LIB2_TLS_BYTES="$lib2_b" >/tmp/tls_one_page_repro_configure.log 2>&1

      if ! ninja -C "$build_dir" tls_one_page_repro >/tmp/tls_one_page_repro_build.log 2>&1; then
        append_log "build-fail main=$main_b shared=$shared_b lib2=$lib2_b"
        append_log "build-log /tmp/tls_one_page_repro_build.log"
        exit 2
      fi

      set +e
      run_out="$("$build_dir/tests/tls_one_page_repro/tls_one_page_repro" 2>&1)"
      run_rc=$?
      set -e

      diag_lines="$(printf '%s\n' "$run_out" | grep -E 'config_|distance_from_tp|pkey\(' || true)"
      if [ "$run_rc" -eq 0 ]; then
        append_log "run-pass main=$main_b shared=$shared_b lib2=$lib2_b"
      else
        append_log "run-fail main=$main_b shared=$shared_b lib2=$lib2_b rc=$run_rc"
      fi
      if [ -n "$diag_lines" ]; then
        while IFS= read -r line; do
          append_log "diag $line"
        done <<< "$diag_lines"
      fi

      if [ "$run_rc" -ne 0 ]; then
        append_log "failure-stop main=$main_b shared=$shared_b lib2=$lib2_b"
        exit 1
      fi
    done
  done
done

append_log "sweep-complete no-failure"
