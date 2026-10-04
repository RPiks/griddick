#!/usr/bin/env bash
# Remove modem/ build artifacts. No command-line arguments.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf "${ROOT}/build"
echo "Cleared ${ROOT}/build"
