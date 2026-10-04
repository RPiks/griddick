#!/usr/bin/env bash
# TX Pico PWM AWGN → air → RX dump, then F1/F0 Pico inject + atest on dumps.
# SNR is PWM-domain (debug.txAwgnSNRdB). Measure air SNR on the dumps later.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_tx_awgn.sh --tx PATH --rx PATH [options]

  --tx PATH         TX Pico (gets debug.txAwgn*)
  --rx PATH         RX Pico (gtwav dump, then gtfeed score)
      --snr LIST    default: 3 3.5 4 5 6 7 8
      --count N     dumps per SNR (default 10)
      --ui-file F   default test/corpus/air/ui.txt (reuse first N lines)
      --src / --dst default TEST-0
      --sanity NAME none or ASCII (default none)
      --pre S       after DUMP_ON before TX (default 0.4)
      --post S      after Tx done before DUMP_OFF (default 0.4)
      --gap-min/--gap-max  after each dump (default 2 3)
      --out DIR     wavs test/corpus/tx_awgn/<snr>/00.wav
      --logs DIR    default test/logs/tx_awgn
      --capture-only
      --score-only
      --pico-only / --atest-only
      --gttx --gtwav --gtfeed --gtcfg PATH
EOF
}

TX=""
RX=""
SNR_LIST="3 3.5 4 5 6 7 8"
COUNT=10
UI_FILE="${ROOT}/test/corpus/air/ui.txt"
SRC="TEST-0"
DST="TEST-0"
SANITY="none"
PRE=0.4
POST=0.4
GAP_MIN=2
GAP_MAX=3
OUT="${ROOT}/test/corpus/tx_awgn"
LOGDIR="${ROOT}/test/logs/tx_awgn"
DO_CAP=1
DO_SCORE=1
DO_PICO=1
DO_ATEST=1
GTTX="${ROOT}/host/bin/gttx"
GTWAV="${ROOT}/host/bin/gtwav"
GTFEED="${ROOT}/host/bin/gtfeed"
GTCFG="${ROOT}/host/bin/gtcfg"

while [[ $# -gt 0 ]]; do
    case "$1" in
    --tx) TX="$2"; shift 2 ;;
    --rx) RX="$2"; shift 2 ;;
    --snr) SNR_LIST="$2"; shift 2 ;;
    --count) COUNT="$2"; shift 2 ;;
    --ui-file) UI_FILE="$2"; shift 2 ;;
    --src) SRC="$2"; shift 2 ;;
    --dst) DST="$2"; shift 2 ;;
    --sanity) SANITY="$2"; shift 2 ;;
    --pre) PRE="$2"; shift 2 ;;
    --post) POST="$2"; shift 2 ;;
    --gap-min) GAP_MIN="$2"; shift 2 ;;
    --gap-max) GAP_MAX="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --logs) LOGDIR="$2"; shift 2 ;;
    --capture-only) DO_SCORE=0; shift ;;
    --score-only) DO_CAP=0; shift ;;
    --pico-only) DO_ATEST=0; shift ;;
    --atest-only) DO_PICO=0; shift ;;
    --gttx) GTTX="$2"; shift 2 ;;
    --gtwav) GTWAV="$2"; shift 2 ;;
    --gtfeed) GTFEED="$2"; shift 2 ;;
    --gtcfg) GTCFG="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "FAIL: unknown $1" >&2; usage >&2; exit 1 ;;
    esac
done

snr_dir() {
    awk -v s="$1" 'BEGIN{
        if (s ~ /\./) printf "%.2f", s+0;
        else printf "%d.00", s+0;
    }'
}

now_ts() { date '+%F %T.%3N'; }
script_log() { printf '%s [script] %s\n' "$(now_ts)" "$*"; }

if [[ -z "${TX}" || -z "${RX}" || "${TX}" == "${RX}" ]]; then
    echo "FAIL: need distinct --tx and --rx" >&2
    exit 1
fi
if [[ ! -e "${TX}" || ! -e "${RX}" ]]; then
    echo "FAIL: device missing" >&2
    exit 1
fi
case "${SANITY}" in
none|ASCII) ;;
*) echo "FAIL: --sanity" >&2; exit 1 ;;
esac
if [[ ! -x "${GTTX}" || ! -x "${GTWAV}" || ! -x "${GTFEED}" || ! -x "${GTCFG}" ]]; then
    echo "FAIL: need gttx gtwav gtfeed gtcfg" >&2
    exit 1
fi
if [[ ! -f "${UI_FILE}" ]]; then
    echo "FAIL: no ${UI_FILE}" >&2
    exit 1
fi
mapfile -t UIS < "${UI_FILE}"
if [[ ${#UIS[@]} -lt ${COUNT} ]]; then
    echo "FAIL: ${UI_FILE} has ${#UIS[@]} lines, need ${COUNT}" >&2
    exit 1
fi
if [[ "${DO_ATEST}" -eq 1 && "${DO_SCORE}" -eq 1 ]] && ! command -v atest >/dev/null; then
    echo "FAIL: atest not in PATH" >&2
    exit 1
fi

tx_awgn() {
    local on="$1" snr="$2" js rc
    js="$(mktemp)"
    if [[ "${on}" -eq 0 ]]; then
        printf '{"debug":{"txAwgnInject":false}}\n' >"${js}"
    else
        printf '{"debug":{"txAwgnInject":true,"txAwgnSNRdB":%s,"txAwgnSeed":1}}\n' \
            "${snr}" >"${js}"
    fi
    set +e
    "${GTCFG}" -d "${TX}" --put "${js}"
    rc=$?
    set -e
    rm -f "${js}"
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL: gtcfg TX put" >&2
        exit 1
    fi
}

rx_fix() {
    local cnt="$1" js rc
    js="$(mktemp)"
    printf '{"modem":{"bitFixCnt":%s,"bitFixAlgo":0,"bitFixSanity":"%s"}}\n' \
        "${cnt}" "${SANITY}" >"${js}"
    set +e
    "${GTCFG}" -d "${RX}" --put "${js}"
    rc=$?
    set -e
    rm -f "${js}"
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL: gtcfg RX put" >&2
        exit 1
    fi
}

WAV_PID=""
stop_wav() {
    if [[ -n "${WAV_PID}" ]] && kill -0 "${WAV_PID}" 2>/dev/null; then
        kill -INT "${WAV_PID}" 2>/dev/null || true
        wait "${WAV_PID}" 2>/dev/null || true
    fi
    WAV_PID=""
}
trap stop_wav EXIT INT TERM

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

strip_ansi() { sed $'s/\x1b\\[[0-9;:]*[a-zA-Z]//g'; }

atest_hit() {
    local wav="$1" f="$2" line
    line="$(atest -B 1200 -F "${f}" "${wav}" 2>&1 | strip_ansi |
        awk '/packets decoded/{ print $1; exit }')"
    if [[ -n "${line}" && "${line}" -gt 0 ]]; then echo 1; else echo 0; fi
}

if [[ "${DO_CAP}" -eq 1 ]]; then
    echo "==== capture TX-AWGN dumps ===="
    for snr in ${SNR_LIST}; do
        tag="$(snr_dir "${snr}")"
        mkdir -p "${OUT}/${tag}"
        script_log "TX inject ${snr} dB"
        tx_awgn 1 "${snr}"
        for ((i = 0; i < COUNT; i++)); do
            ui="${UIS[i]}"
            wav="$(printf '%s/%s/%02d.wav' "${OUT}" "${tag}" "${i}")"
            script_log "rec ${tag} $((i + 1))/${COUNT}"
            "${GTWAV}" -d "${RX}" -o "${wav}" &
            WAV_PID=$!
            sleep "${PRE}"
            if ! kill -0 "${WAV_PID}" 2>/dev/null; then
                echo "FAIL: gtwav died ${wav}" >&2
                exit 1
            fi
            set +e
            "${GTTX}" -d "${TX}" -s "${SRC}" -t "${DST}" --text "${ui}"
            set -e
            sleep "${POST}"
            stop_wav
            if [[ ! -s "${wav}" ]]; then
                echo "FAIL: empty ${wav}" >&2
                exit 1
            fi
            if [[ "${i}" -lt $((COUNT - 1)) ]]; then
                gap="$(awk -v a="${GAP_MIN}" -v b="${GAP_MAX}" -v r="${RANDOM}" \
                    'BEGIN{printf "%.3f", a + (b-a)*(r/32767)}')"
                sleep "${gap}"
            fi
        done
    done
    tx_awgn 0 3
    echo "dumps ${OUT}/<snr>/"
fi

declare -A P0N P1N A0N A1N P0H P1H A0H A1H

if [[ "${DO_SCORE}" -eq 1 ]]; then
    echo "==== score dumps ===="
    for snr in ${SNR_LIST}; do
        tag="$(snr_dir "${snr}")"
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
                    script_log "feed F${cnt} ${tag}/${base}"
                    set +e
                    "${GTFEED}" -d "${RX}" -o "${log}" "${wav}"
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

    printf '\n========== tx awgn dumps ==========\n'
    printf '%-6s  %-10s  %-10s  %-10s  %-10s\n' \
        "snr" "pico-F1" "pico-F0" "atest-F1" "atest-F0"
    for snr in ${SNR_LIST}; do
        tag="$(snr_dir "${snr}")"
        printf '%-6s  %3d/%-6d  %3d/%-6d  %3d/%-6d  %3d/%-6d\n' \
            "${tag}" \
            "${P1H[${tag}]:-0}" "${P1N[${tag}]:-0}" \
            "${P0H[${tag}]:-0}" "${P0N[${tag}]:-0}" \
            "${A1H[${tag}]:-0}" "${A1N[${tag}]:-0}" \
            "${A0H[${tag}]:-0}" "${A0N[${tag}]:-0}"
    done
    echo "order: Pico F1, Pico F0, atest -F 1, atest -F 0"
    echo "wav ${OUT}  log ${LOGDIR}"
    echo "==================================="
fi
exit 0
