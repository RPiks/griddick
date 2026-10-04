#!/usr/bin/env bash
# Short regression slice of the host AWGN corpus: 5 fixed WAVs per SNR.
# Same files every run (sorted names, first 5): sNN_000.wav … sNN_004.wav.
#
# Copy the corpus as for run_pico_awgn.sh:
#   test/corpus/awgn/dw/s03_000.wav … s06_049.wav
#   test/corpus/awgn/ours/s03_000.wav …          (optional)
#
# Usage: test/run_pico_awgn_brief.sh
# Env:
#   TNC_DEV   CDC path (default /dev/ttyACM0)
#   CORPUS    root with dw/ and ours/ (default test/corpus/awgn)
#   FAM       dw, ours, or both (default both)
#   SNR       space-separated list (default "3 4 5 6")
#   TNCFEED   path to gtfeed
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CORPUS="${CORPUS:-${ROOT}/test/corpus/awgn}"
LOGDIR="${ROOT}/test/logs/awgn_brief"
DEV="${TNC_DEV:-/dev/ttyACM0}"
FAM="${FAM:-both}"
SNR="${SNR:-3 4 5 6}"
PER_SNR=5
TNCFEED="${TNCFEED:-${ROOT}/host/bin/gtfeed}"
if [[ ! -x "${TNCFEED}" && -x "${ROOT}/build/host/gtfeed" ]]; then
    TNCFEED="${ROOT}/build/host/gtfeed"
fi

need() {
    if ! command -v "$1" >/dev/null; then
        echo "FAIL: $1 not in PATH" >&2
        exit 1
    fi
}

need atest
if [[ ! -x "${TNCFEED}" ]]; then
    echo "FAIL: gtfeed not found (${TNCFEED})" >&2
    exit 1
fi
if [[ ! -e "${DEV}" ]]; then
    echo "FAIL: no device ${DEV}" >&2
    exit 1
fi

strip_ansi() {
    sed $'s/\x1b\\[[0-9;:]*[a-zA-Z]//g'
}

families=()
case "${FAM}" in
both) families=(dw ours) ;;
dw|ours) families=("${FAM}") ;;
*)
    echo "FAIL: FAM must be dw, ours, or both" >&2
    exit 1
    ;;
esac

pico_hit() {
    local log="$1"
    local n
    n="$(awk '
    /\[AX25\]/ && /fcs=1/ {
        split($2, t, /[:.]/)
        ms = ((t[1] * 3600) + (t[2] * 60) + t[3]) * 1000 + t[4]
        if (n == 0 || ms - prev > 25) n++
        prev = ms
    }
    END { print n + 0 }
    ' "${log}")"
    if [[ "${n}" -gt 0 ]]; then
        echo 1
    else
        echo 0
    fi
}

atest_hit() {
    local wav="$1"
    local line
    line="$(atest -B 1200 -F 0 "${wav}" 2>&1 | strip_ansi |
        awk '/packets decoded/{ print $1; exit }')"
    if [[ -n "${line}" && "${line}" -gt 0 ]]; then
        echo 1
    else
        echo 0
    fi
}

list_wavs() {
    local fam="$1" snr="$2"
    local dir="${CORPUS}/${fam}"
    local pat
    printf -v pat 's%02d_*.wav' "${snr}"
    if [[ ! -d "${dir}" ]]; then
        return
    fi
    find "${dir}" -maxdepth 1 -type f -name "${pat}" | sort | head -n "${PER_SNR}"
}

n_total=0
declare -A PICO_N ATEST_N PICO_H ATEST_H

for fam in "${families[@]}"; do
    for s in ${SNR}; do
        key="${fam},${s}"
        PICO_N["${key}"]=0
        ATEST_N["${key}"]=0
        PICO_H["${key}"]=0
        ATEST_H["${key}"]=0
        mapfile -t WAVS < <(list_wavs "${fam}" "${s}")
        if [[ ${#WAVS[@]} -eq 0 ]]; then
            echo "SKIP ${fam} ${s} dB: no ${CORPUS}/${fam}/s$(printf '%02d' "${s}")_*.wav"
            continue
        fi
        if [[ ${#WAVS[@]} -lt ${PER_SNR} ]]; then
            echo "WARN ${fam} ${s} dB: ${#WAVS[@]} wavs (want ${PER_SNR})"
        fi
        mkdir -p "${LOGDIR}/${fam}"
        for wav in "${WAVS[@]}"; do
            base="${wav##*/}"
            log="${LOGDIR}/${fam}/${base%.wav}.feed.log"
            echo "FEED ${fam}/${base} -> ${DEV}"
            set +e
            "${TNCFEED}" -d "${DEV}" -o "${log}" "${wav}"
            rc=$?
            set -e
            ph="$(pico_hit "${log}")"
            ah="$(atest_hit "${wav}")"
            PICO_N["${key}"]=$((PICO_N["${key}"] + 1))
            ATEST_N["${key}"]=$((ATEST_N["${key}"] + 1))
            PICO_H["${key}"]=$((PICO_H["${key}"] + ph))
            ATEST_H["${key}"]=$((ATEST_H["${key}"] + ah))
            n_total=$((n_total + 1))
            echo "  hit pico=${ph} atest=${ah} rc=${rc}"
        done
    done
done

if [[ "${n_total}" -eq 0 ]]; then
    echo "FAIL: no WAVs under ${CORPUS}/{dw,ours}/sNN_XXX.wav" >&2
    echo "Copy the host corpus there and rerun." >&2
    exit 1
fi

echo
echo "========== summary =========="
printf '%-6s %3s  %-8s  %-8s\n' "family" "snr" "pico" "atest"
printf '%-6s %3s  %-8s  %-8s\n' "------" "---" "--------" "--------"
for fam in "${families[@]}"; do
    for s in ${SNR}; do
        key="${fam},${s}"
        pn="${PICO_N[${key}]}"
        an="${ATEST_N[${key}]}"
        if [[ "${pn}" -eq 0 && "${an}" -eq 0 ]]; then
            printf '%-6s %3s  %-8s  %-8s\n' "${fam}" "${s}" "-" "-"
            continue
        fi
        printf '%-6s %3s  %3d/%-4d  %3d/%-4d\n' \
            "${fam}" "${s}" \
            "${PICO_H[${key}]}" "${pn}" \
            "${ATEST_H[${key}]}" "${an}"
    done
done
echo "============================="
echo "brief: first ${PER_SNR} sorted wavs per family/SNR (sNN_000 …)"
echo "hit = ≥1 good FCS (Pico AX25 clustered 25 ms; atest packets>0)"
echo "logs ${LOGDIR}"
exit 0
