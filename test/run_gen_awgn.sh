#!/usr/bin/env bash
# Offline WAV lab: gen_packets + Gaussian mix at a target SNR, then
# Pico inject F1/F0 and atest F1/F0 on one device. Not the air path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_gen_awgn.sh [options]

  --snr LIST      target in-band dB (default 3 3.25 3.5 3.75 4 5 6 7 8)
  --count N       trials per SNR (default 100)
  --ui-file F     default test/corpus/air/ui.txt (written if missing or short)
  --ui-len N      bytes per line when writing ui.txt (default 32)
  --seed N        base seed (default 1)
  --out DIR       default test/corpus/awgn_pn
  --logs DIR      default test/logs/awgn_pn
  -d, --device PATH   Pico for scoring (omit with --gen-only)
      --sanity NAME   none or ASCII (default none)
      --gen-only
      --score-only
      --pico-only / --atest-only
      --gtfeed --gtcfg PATH
EOF
}

UI_FILE="${ROOT}/test/corpus/air/ui.txt"
UI_LEN=32
COUNT=100
SNR_LIST="3 3.25 3.5 3.75 4 5 6 7 8"
SEED=1
OUT="${ROOT}/test/corpus/awgn_pn"
LOGDIR="${ROOT}/test/logs/awgn_pn"
DEV=""
SANITY="none"
DO_GEN=1
DO_SCORE=1
DO_PICO=1
DO_ATEST=1
GTFEED="${ROOT}/host/bin/gtfeed"
GTCFG="${ROOT}/host/bin/gtcfg"

while [[ $# -gt 0 ]]; do
    case "$1" in
    --ui-file) UI_FILE="$2"; shift 2 ;;
    --ui-len) UI_LEN="$2"; shift 2 ;;
    --count) COUNT="$2"; shift 2 ;;
    --snr) SNR_LIST="$2"; shift 2 ;;
    --seed) SEED="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --logs) LOGDIR="$2"; shift 2 ;;
    -d|--device) DEV="$2"; shift 2 ;;
    --sanity) SANITY="$2"; shift 2 ;;
    --gen-only) DO_SCORE=0; shift ;;
    --score-only) DO_GEN=0; shift ;;
    --pico-only) DO_ATEST=0; shift ;;
    --atest-only) DO_PICO=0; shift ;;
    --gtfeed) GTFEED="$2"; shift 2 ;;
    --gtcfg) GTCFG="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "FAIL: unknown $1" >&2; usage >&2; exit 1 ;;
    esac
done

snr_tag() {
    awk -v s="$1" 'BEGIN{printf "%.2f", s+0}'
}

if [[ ! -x "${GTFEED}" && -x "${ROOT}/build/host/gtfeed" ]]; then
    GTFEED="${ROOT}/build/host/gtfeed"
fi
if [[ ! -x "${GTCFG}" && -x "${ROOT}/build/host/gtcfg" ]]; then
    GTCFG="${ROOT}/build/host/gtcfg"
fi
case "${SANITY}" in
none|ASCII) ;;
*) echo "FAIL: --sanity" >&2; exit 1 ;;
esac

if [[ "${DO_GEN}" -eq 1 ]]; then
    if ! command -v gen_packets >/dev/null; then
        echo "FAIL: gen_packets not in PATH" >&2
        exit 1
    fi
    if ! command -v gcc >/dev/null; then
        echo "FAIL: gcc not in PATH" >&2
        exit 1
    fi
    write_ui() {
        local n="$1" len="$2" seed="$3" path="$4"
        local alphabet='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=@#%*'
        local alen=${#alphabet}
        local i j idx line
        mkdir -p "$(dirname "${path}")"
        RANDOM="${seed}"
        : >"${path}"
        for ((i = 0; i < n; i++)); do
            line=""
            for ((j = 0; j < len; j++)); do
                idx=$((RANDOM % alen))
                line+="${alphabet:idx:1}"
            done
            printf '%s\n' "${line}" >>"${path}"
        done
    }
    have=0
    if [[ -f "${UI_FILE}" ]]; then
        have="$(wc -l < "${UI_FILE}" | tr -d ' ')"
    fi
    if [[ "${have}" -lt "${COUNT}" ]]; then
        echo "write ${UI_FILE} lines=${COUNT} len=${UI_LEN}"
        write_ui "${COUNT}" "${UI_LEN}" "${SEED}" "${UI_FILE}"
    fi
    mapfile -t UIS < "${UI_FILE}"
    if [[ ${#UIS[@]} -lt ${COUNT} ]]; then
        echo "FAIL: ui file has ${#UIS[@]} lines, need ${COUNT}" >&2
        exit 1
    fi
    MIX="${ROOT}/test/awgn/mix_awgn"
    gcc -O2 -Wall -o "${MIX}" "${ROOT}/test/awgn/mix_awgn.c" -lm
    mkdir -p "${LOGDIR}"
    MEAS="${LOGDIR}/measure.txt"
    : >"${MEAS}"
    TMPD="$(mktemp -d)"
    trap 'rm -rf "${TMPD}"' EXIT
    for snr in ${SNR_LIST}; do
        tag="$(snr_tag "${snr}")"
        dest="${OUT}/${tag}"
        mkdir -p "${dest}"
        echo "==== gen ${tag} dB ===="
        for ((i = 0; i < COUNT; i++)); do
            ui="${UIS[i]}"
            frm="${TMPD}/f.txt"
            cln="${TMPD}/c.wav"
            printf 'TEST-0>TEST-0:%s\n' "${ui}" >"${frm}"
            gen_packets -r 48000 -B 1200 -a 100 -o "${cln}" "${frm}" >/dev/null
            s=$((SEED + $(snr_tag "${snr}" | awk '{printf "%d", $1*100}') * 1000 + i))
            wav="$(printf '%s/%03d.wav' "${dest}" "${i}")"
            "${MIX}" --snr "${snr}" --seed "${s}" "${cln}" "${wav}" | tee -a "${MEAS}"
        done
    done
    echo "corpus ${OUT}/<snr>/"
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

rx_fix() {
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
}

if [[ "${DO_SCORE}" -eq 1 ]]; then
    if [[ -z "${DEV}" || ! -e "${DEV}" ]]; then
        echo "FAIL: --device (or --gen-only)" >&2
        exit 1
    fi
    if [[ "${DO_PICO}" -eq 1 && ( ! -x "${GTFEED}" || ! -x "${GTCFG}" ) ]]; then
        echo "FAIL: gtfeed/gtcfg" >&2
        exit 1
    fi
    if [[ "${DO_ATEST}" -eq 1 ]] && ! command -v atest >/dev/null; then
        echo "FAIL: atest not in PATH" >&2
        exit 1
    fi
    declare -A P0N P1N A0N A1N P0H P1H A0H A1H
    echo "==== score ===="
    for snr in ${SNR_LIST}; do
        tag="$(snr_tag "${snr}")"
        P0N["${tag}"]=0; P1N["${tag}"]=0
        A0N["${tag}"]=0; A1N["${tag}"]=0
        P0H["${tag}"]=0; P1H["${tag}"]=0
        A0H["${tag}"]=0; A1H["${tag}"]=0
        shopt -s nullglob
        wavs=("${OUT}/${tag}"/*.wav)
        shopt -u nullglob
        if [[ ${#wavs[@]} -eq 0 ]]; then
            echo "SKIP ${tag}"
            continue
        fi
        if [[ "${DO_PICO}" -eq 1 ]]; then
            for cnt in 1 0; do
                rx_fix "${cnt}"
                mkdir -p "${LOGDIR}/${tag}"
                for wav in "${wavs[@]}"; do
                    base="${wav##*/}"
                    log="${LOGDIR}/${tag}/${base%.wav}.f${cnt}.feed.log"
                    set +e
                    "${GTFEED}" -d "${DEV}" -o "${log}" "${wav}"
                    set -e
                    ph="$(pico_hit "${log}")"
                    if [[ "${cnt}" -eq 0 ]]; then
                        P0N["${tag}"]=$((P0N["${tag}"] + 1))
                        P0H["${tag}"]=$((P0H["${tag}"] + ph))
                    else
                        P1N["${tag}"]=$((P1N["${tag}"] + 1))
                        P1H["${tag}"]=$((P1H["${tag}"] + ph))
                    fi
                done
            done
        fi
        if [[ "${DO_ATEST}" -eq 1 ]]; then
            for f in 1 0; do
                for wav in "${wavs[@]}"; do
                    ah="$(atest_hit "${wav}" "${f}")"
                    if [[ "${f}" -eq 0 ]]; then
                        A0N["${tag}"]=$((A0N["${tag}"] + 1))
                        A0H["${tag}"]=$((A0H["${tag}"] + ah))
                    else
                        A1N["${tag}"]=$((A1N["${tag}"] + 1))
                        A1H["${tag}"]=$((A1H["${tag}"] + ah))
                    fi
                done
            done
        fi
    done
    printf '\n========== host wav ==========\n'
    printf '%-6s  %-10s  %-10s  %-10s  %-10s\n' \
        "snr" "pico-F1" "pico-F0" "atest-F1" "atest-F0"
    for snr in ${SNR_LIST}; do
        tag="$(snr_tag "${snr}")"
        printf '%-6s  %3d/%-6d  %3d/%-6d  %3d/%-6d  %3d/%-6d\n' \
            "${tag}" \
            "${P1H[${tag}]:-0}" "${P1N[${tag}]:-0}" \
            "${P0H[${tag}]:-0}" "${P0N[${tag}]:-0}" \
            "${A1H[${tag}]:-0}" "${A1N[${tag}]:-0}" \
            "${A0H[${tag}]:-0}" "${A0N[${tag}]:-0}"
    done
    echo "order: Pico F1, Pico F0, atest -F 1, atest -F 0"
    echo "================================"
fi
exit 0
