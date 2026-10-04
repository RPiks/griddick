#!/usr/bin/env bash
# Remove build trees and copied artifacts. Keeps third_party/ and tools/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

rm -rf \
    "${ROOT}/build" \
    "${ROOT}/dsp/build" \
    "${ROOT}/modem/build" \
    "${ROOT}/host/build"

rm -f \
    "${ROOT}/griddick_tnc.uf2" \
    "${ROOT}/compile_commands.json"

rm -rf "${ROOT}/host/bin"
rm -f "${ROOT}/host/lib"/libgtnc.so* "${ROOT}/host/lib"/libgtnc.a

echo "Cleaned build artifacts under ${ROOT}"
