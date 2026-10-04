#!/usr/bin/env bash
# Build modem unit tests. No command-line arguments.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${ROOT}/build"

echo "==> modem tests..."
cmake -S "${ROOT}" -B "${BUILD}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMODEM_BUILD_TESTS=ON
cmake --build "${BUILD}" -j"$(nproc)"

echo "==> ctest..."
ctest --test-dir "${BUILD}" --output-on-failure

echo "==> modem/build.sh done."
