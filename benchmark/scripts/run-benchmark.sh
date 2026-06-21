#!/bin/bash
# =============================================================================
# oatpp Benchmark Suite
# Usage: ./run-benchmark.sh [sync|async]
# =============================================================================
set -uo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../.."

export BUILD_DIR="${BUILD_DIR:-build}"
export PORT="${PORT:-8000}"
export DURATION="${DURATION:-10s}"
export CONNECTIONS="${CONNECTIONS:-100}"
export THREADS="${THREADS:-4}"

export PYTHONDONTWRITEBYTECODE=1
exec python3 -B "$SCRIPT_DIR/run-benchmark.py" "${1:-sync}"
