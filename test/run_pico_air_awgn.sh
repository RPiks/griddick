#!/usr/bin/env bash
# Phase 2 replay: mixed air corpus vs atest. F1 then F0.
# Corpus: test/corpus/air/awgn/<snr>/000.wav …
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_air_awgn.sh --device PATH [options]

  -d, --device PATH
      --corpus DIR    default test/corpus/air/awgn
      --snr LIST      default 3.00 3.25 3.50 3.75 4.00 5.00 6.00 7.00 8.00 9.00 10.00
      --sanity NAME   none or ASCII (default ASCII)
      --gtfeed --gtcfg PATH
      --logs DIR      default test/logs/air_awgn
      --pico-only / --atest-only
EOF
}

DEV=""
CORPUS="${ROOT}/test/corpus/air/awgn"
SNR="3.00 3.25 3.50 3.75 4.00 5.00 6.00 7.00 8.00 9.00 10.00"
SANITY="ASCII"
LOGDIR="${ROOT}/test/logs/air_awgn"
TNCFEED="${ROOT}/host/bin/gtfeed"
GTCFG="${ROOT}/host/bin/gtcfg"
DO_PICO=1
DO_ATEST=1

while [[ $# -gt 0 ]]; do
    case "$1" in
    -d|--device) DEV="$2"; shift 2 ;;
    --corpus) CORPUS="$2"; shift 2 ;;
    --snr) SNR="$2"; shift 2 ;;
    --sanity) SANITY="$2"; shift 2 ;;
    --gtfeed) TNCFEED="$2"; shift 2 ;;
    --gtcfg) GTCFG="$2"; shift 2 ;;
    --logs) LOGDIR="$2"; shift 2 ;;
    --pico-only) DO_ATEST=0; shift ;;
    --atest-only) DO_PICO=0; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "FAIL: unknown $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ ! -x "${TNCFEED}" && -x "${ROOT}/build/host/gtfeed" ]]; then
    TNCFEED="${ROOT}/build/host/gtfeed"
fi
if [[ ! -x "${GTCFG}" && -x "${ROOT}/build/host/gtcfg" ]]; then
    GTCFG="${ROOT}/build/host/gtcfg"
fi
case "${SANITY}" in
none|ASCII) ;;
*) echo "FAIL: --sanity" >&2; exit 1 ;;
esac
if [[ "${DO_ATEST}" -eq 1 ]] && ! command -v atest >/dev/null; then
    echo "FAIL: atest not in PATH" >&2
    exit 1
fi
if [[ "${DO_PICO}" -eq 1 ]]; then
    if [[ -z "${DEV}" || ! -e "${DEV}" ]]; then
        echo "FAIL: --device" >&2
        exit 1
    fi
    if [[ ! -x "${TNCFEED}" || ! -x "${GTCFG}" ]]; then
        echo "FAIL: gtfeed/gtcfg" >&2
        exit 1
    fi
fi

strip_ansi() { sed $'s/\x1b\\[[0-9;:]*[a-zA-Z]//g'; }

pico_hit() {
    local log="$1" n
    n="$(awk '
    /\[AX25\]/ && /fcs=1/ {
        split($2, t, /[:.]/)
        ms = ((t[1] * 3600) + (t[2] * 60) + t[3]) * 1000 + t[4]
        if (n == 0 || ms - prev > 25) n++
        prev = ms
    }
    END { print n + 0 }
    ' "${log}")"
    if [[ "${n}" -gt 0 ]]; then echo 1; else echo 0; fi
}

atest_hit() {
    local wav="$1" f="$2" line
    line="$(atest -B 1200 -F "${f}" "${wav}" 2>&1 | strip_ansi |
        awk '/packets decoded/{ print $1; exit }')"
    if [[ -n "${line}" && "${line}" -gt 0 ]]; then echo 1; else echo 0; fi
}

list_wavs() {
    local snr="$1"
    local dir="${CORPUS}/${snr}"
    if [[ ! -d "${dir}" ]]; then
        return
    fi
    find "${dir}" -maxdepth 1 -type f -name '*.wav' | sort
}

now_ts() { date '+%F %T.%3N'; }
script_log() { printf '%s [script] %s\n' "$(now_ts)" "$*"; }

set_bitfix() {
    local cnt="$1" js rc
    js="$(mktemp)"
    printf '{"modem":{"bitFixCnt":%s,"bitFixAlgo":0,"bitFixSanity":"%s"}}\n' \
        "${cnt}" "${SANITY}" >"${js}"
    set +e
    "${GTCFG}" -d "${DEV}" --put "${js}"
    rc=$?
    set -e
    rm -f "${js}"
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL: gtcfg" >&2
        exit 1
    fi
    set +e
    "${GTCFG}" -d "${DEV}" -g | tee /dev/stderr | grep -E 'bitFixCnt' || true
    set -e
}

frac() {
    local h="$1" n="$2"
    if [[ "${n}" -eq 0 ]]; then
        printf '%s' "-"
    else
        printf '%3d/%-6d' "${h}" "${n}"
    fi
}

declare -A PICO0_N PICO1_N ATEST0_N ATEST1_N
declare -A PICO0_H PICO1_H ATEST0_H ATEST1_H
for s in ${SNR}; do
    PICO0_N["${s}"]=0; PICO1_N["${s}"]=0
    ATEST0_N["${s}"]=0; ATEST1_N["${s}"]=0
    PICO0_H["${s}"]=0; PICO1_H["${s}"]=0
    ATEST0_H["${s}"]=0; ATEST1_H["${s}"]=0
done

pico_sweep() {
    local cnt="$1"
    local tag="f${cnt}"
    echo "==== Pico bitFixCnt=${cnt} ===="
    set_bitfix "${cnt}"
    for s in ${SNR}; do
        mapfile -t WAVS < <(list_wavs "${s}")
        if [[ ${#WAVS[@]} -eq 0 ]]; then
            echo "SKIP SNR ${s}"
            continue
        fi
        mkdir -p "${LOGDIR}/${s}"
        for wav in "${WAVS[@]}"; do
            local base="${wav##*/}"
            local log="${LOGDIR}/${s}/${base%.wav}.${tag}.feed.log"
            script_log "feed ${tag} ${s}/${base}"
            set +e
            "${TNCFEED}" -d "${DEV}" -o "${log}" "${wav}"
            local rc=$?
            set -e
            local ph
            ph="$(pico_hit "${log}")"
            if [[ "${cnt}" -eq 0 ]]; then
                PICO0_N["${s}"]=$((PICO0_N["${s}"] + 1))
                PICO0_H["${s}"]=$((PICO0_H["${s}"] + ph))
            else
                PICO1_N["${s}"]=$((PICO1_N["${s}"] + 1))
                PICO1_H["${s}"]=$((PICO1_H["${s}"] + ph))
            fi
            script_log "hit pico-F${cnt}=${ph} rc=${rc}"
        done
    done
}

atest_sweep() {
    local f="$1"
    echo "==== atest -F ${f} ===="
    for s in ${SNR}; do
        mapfile -t WAVS < <(list_wavs "${s}")
        for wav in "${WAVS[@]}"; do
            local ah
            ah="$(atest_hit "${wav}" "${f}")"
            if [[ "${f}" -eq 0 ]]; then
                ATEST0_N["${s}"]=$((ATEST0_N["${s}"] + 1))
                ATEST0_H["${s}"]=$((ATEST0_H["${s}"] + ah))
            else
                ATEST1_N["${s}"]=$((ATEST1_N["${s}"] + 1))
                ATEST1_H["${s}"]=$((ATEST1_H["${s}"] + ah))
            fi
        done
    done
}

if [[ "${DO_PICO}" -eq 1 ]]; then
    pico_sweep 1
    pico_sweep 0
fi
if [[ "${DO_ATEST}" -eq 1 ]]; then
    atest_sweep 1
    atest_sweep 0
fi

echo
echo "========== air awgn =========="
printf '%-6s  %-10s  %-10s  %-10s  %-10s\n' \
    "snr" "pico-F1" "pico-F0" "atest-F1" "atest-F0"
for s in ${SNR}; do
    printf '%-6s  %-10s  %-10s  %-10s  %-10s\n' \
        "${s}" \
        "$(frac "${PICO1_H[${s}]}" "${PICO1_N[${s}]}")" \
        "$(frac "${PICO0_H[${s}]}" "${PICO0_N[${s}]}")" \
        "$(frac "${ATEST1_H[${s}]}" "${ATEST1_N[${s}]}")" \
        "$(frac "${ATEST0_H[${s}]}" "${ATEST0_N[${s}]}")"
done
echo "=============================="
exit 0
