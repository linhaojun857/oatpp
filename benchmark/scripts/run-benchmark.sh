#!/bin/bash
# =============================================================================
# oatpp Benchmark
# Usage:
#   ./run-benchmark.sh -m sync               # run all scenarios (sync mode)
#   ./run-benchmark.sh -m async              # run all scenarios (async mode)
#   ./run-benchmark.sh -m async -s "Hello World"
#   ./run-benchmark.sh -m async -s 0 1 2
#
# Scenario selection (-s) supports:
#   - Exact name   : -s "Hello World"
#   - Partial match: -s json        (matches "JSON (small)", "JSON (large)")
#   - Index        : -s 0           (0-based index, see --list)
#   - Lua filename : -s hello.lua
# =============================================================================
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../.."

export BUILD_DIR="${BUILD_DIR:-build-benchmark}"
export PORT="${PORT:-8000}"
export DURATION="${DURATION:-2s}"
export CONNECTIONS="${CONNECTIONS:-1000}"
export THREADS="${THREADS:-10}"

export PYTHONDONTWRITEBYTECODE=1
exec python3 -B "$SCRIPT_DIR/run-benchmark.py" "$@"
