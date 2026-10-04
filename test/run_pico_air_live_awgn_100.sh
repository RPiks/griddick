#!/usr/bin/env bash
# 100 live TX/RX per SNR, Pico F1 then F0 (gtdump UI).
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${ROOT}/test/run_pico_air_live_awgn.sh" \
    --snr "3 3.5 4 5 6 7 8" --count 100 "$@"
