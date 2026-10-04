#!/usr/bin/env bash
# 100 dumps per SNR, then Pico inject + atest F1/F0 on those WAVs.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec "${ROOT}/test/run_pico_tx_awgn.sh" \
    --snr "3 3.5 4 5 6 7 8" --count 100 "$@"
