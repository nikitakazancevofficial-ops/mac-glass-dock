#!/usr/bin/env bash
# Release by default; pass --tests to build the regression suite.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${MACGLASS_BUILD_DIR:-$project_dir/build}"
testing=OFF
case "${1:-}" in
    --tests) testing=ON ;;
    --help|-h) printf 'Usage: %s [--tests]\nEnvironment: MACGLASS_BUILD_DIR, CMAKE_BUILD_PARALLEL_LEVEL (default 1)\n' "$0"; exit 0 ;;
    '') ;;
    *) printf 'Unknown option: %s\n' "$1" >&2; exit 2 ;;
esac
cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING="$testing"
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
