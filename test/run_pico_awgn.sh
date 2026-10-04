#!/usr/bin/env bash
# AWGN lab: Pico bitFix vs atest. Full table, F1 then F0.
#
# Corpus (copy here, names from the host generator):
#   test/corpus/awgn/dw/s03_000.wav … s06_049.wav
#   test/corpus/awgn/ours/s03_000.wav …          (optional)
#
# Usage:
#   test/run_pico_awgn.sh --device /dev/ttyACM1 --family dw
#
# Order: Pico bitFixCnt=1, Pico bitFixCnt=0 (device left at 0),
#        then atest -F 1, atest -F 0.
# Table columns: pico-F1  pico-F0  atest-F1  atest-F0
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_awgn.sh [options]

  -d, --device PATH   Pico CDC node (default /dev/ttyACM0)
      --corpus DIR    WAV root (default test/corpus/awgn)
      --family NAME   dw, ours, or both (default both)
      --snr LIST      SNRs, space-separated (default: 3 4 5 6)
      --gtfeed PATH   gtfeed binary
      --gtcfg PATH    gtcfg binary
      --sanity NAME   none or ASCII (default ASCII)
      --logs DIR      feed logs (default test/logs/awgn)
      --pico-only     skip atest
      --atest-only    skip Pico / gtcfg
  -h, --help

Full comparison: Pico F1, Pico F0, atest F1, atest F0.
Hit = at least one good FCS (Pico AX25 clustered 25 ms; atest packets>0).
EOF
}

DEV="/dev/ttyACM0"
CORPUS="${ROOT}/test/corpus/awgn"
FAMILY="both"
SNR="3 4 5 6"
SANITY="ASCII"
LOGDIR="${ROOT}/test/logs/awgn"
TNCFEED="${ROOT}/host/bin/gtfeed"
GTCFG="${ROOT}/host/bin/gtcfg"
DO_PICO=1
DO_ATEST=1

while [[ $# -gt 0 ]]; do
    case "$1" in
    -d|--device)
        DEV="$2"
        shift 2
        ;;
    --corpus)
        CORPUS="$2"
        shift 2
        ;;
    --family)
        FAMILY="$2"
        shift 2
        ;;
    --snr)
        SNR="$2"
        shift 2
        ;;
    --gtfeed)
        TNCFEED="$2"
        shift 2
        ;;
    --gtcfg)
        GTCFG="$2"
        shift 2
        ;;
    --sanity)
        SANITY="$2"
        shift 2
        ;;
    --logs)
        LOGDIR="$2"
        shift 2
        ;;
    --pico-only)
        DO_ATEST=0
        shift
        ;;
    --atest-only)
        DO_PICO=0
        shift
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        echo "FAIL: unknown option $1" >&2
        usage >&2
        exit 1
        ;;
    esac
done

if [[ ! -x "${TNCFEED}" && -x "${ROOT}/build/host/gtfeed" ]]; then
    TNCFEED="${ROOT}/build/host/gtfeed"
fi
if [[ ! -x "${GTCFG}" && -x "${ROOT}/build/host/gtcfg" ]]; then
    GTCFG="${ROOT}/build/host/gtcfg"
fi

case "${FAMILY}" in
both) families=(dw ours) ;;
dw|ours) families=("${FAMILY}") ;;
*)
    echo "FAIL: --family must be dw, ours, or both" >&2
    exit 1
    ;;
esac
case "${SANITY}" in
none|ASCII) ;;
*)
    echo "FAIL: --sanity must be none or ASCII" >&2
    exit 1
    ;;
esac

if [[ "${DO_ATEST}" -eq 1 ]] && ! command -v atest >/dev/null; then
    echo "FAIL: atest not in PATH" >&2
    exit 1
fi
if [[ "${DO_PICO}" -eq 1 ]]; then
    if [[ ! -x "${TNCFEED}" ]]; then
        echo "FAIL: gtfeed not found (${TNCFEED})" >&2
        exit 1
    fi
    if [[ ! -x "${GTCFG}" ]]; then
        echo "FAIL: gtcfg not found (${GTCFG})" >&2
        exit 1
    fi
    if [[ ! -e "${DEV}" ]]; then
        echo "FAIL: no device ${DEV}" >&2
        exit 1
    fi
fi

strip_ansi() {
    sed $'s/\x1b\\[[0-9;:]*[a-zA-Z]//g'
}

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
    local f="$2"
    local line
    line="$(atest -B 1200 -F "${f}" "${wav}" 2>&1 | strip_ansi |
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
    find "${dir}" -maxdepth 1 -type f -name "${pat}" | sort
}

now_ts() {
    date '+%F %T.%3N'
}

script_log() {
    printf '%s [script] %s\n' "$(now_ts)" "$*"
}

set_bitfix() {
    local cnt="$1"
    local js t0 t1 rc dt
    js="$(mktemp)"
    printf '{"modem":{"bitFixCnt":%s,"bitFixAlgo":0,"bitFixSanity":"%s"}}\n' \
        "${cnt}" "${SANITY}" >"${js}"
    script_log "gtcfg put bitFixCnt=${cnt} begin"
    t0=$(date +%s.%N)
    set +e
    "${GTCFG}" -d "${DEV}" --put "${js}"
    rc=$?
    set -e
    t1=$(date +%s.%N)
    dt="$(awk -v a="${t0}" -v b="${t1}" 'BEGIN{printf "%.3f", b-a}')"
    rm -f "${js}"
    script_log "gtcfg put bitFixCnt=${cnt} rc=${rc} sec=${dt}"
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL: gtcfg bitFixCnt=${cnt}" >&2
        exit 1
    fi
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

for fam in "${families[@]}"; do
    for s in ${SNR}; do
        key="${fam},${s}"
        PICO0_N["${key}"]=0
        PICO1_N["${key}"]=0
        ATEST0_N["${key}"]=0
        ATEST1_N["${key}"]=0
        PICO0_H["${key}"]=0
        PICO1_H["${key}"]=0
        ATEST0_H["${key}"]=0
        ATEST1_H["${key}"]=0
    done
done

n_wav=0

pico_sweep() {
    local cnt="$1"
    local tag="f${cnt}"
    echo "==== Pico bitFixCnt=${cnt} sanity=${SANITY} ===="
    set_bitfix "${cnt}"
    for fam in "${families[@]}"; do
        for s in ${SNR}; do
            local key="${fam},${s}"
            mapfile -t WAVS < <(list_wavs "${fam}" "${s}")
            if [[ ${#WAVS[@]} -eq 0 ]]; then
                echo "SKIP ${fam} ${s} dB: no wavs"
                continue
            fi
            mkdir -p "${LOGDIR}/${fam}"
            for wav in "${WAVS[@]}"; do
                local base="${wav##*/}"
                local log="${LOGDIR}/${fam}/${base%.wav}.${tag}.feed.log"
                local t0 t1 rc ph dt
                script_log "feed ${tag} ${fam}/${base} spawn ${TNCFEED} ${DEV}"
                t0=$(date +%s.%N)
                set +e
                "${TNCFEED}" -d "${DEV}" -o "${log}" "${wav}"
                rc=$?
                set -e
                t1=$(date +%s.%N)
                dt="$(awk -v a="${t0}" -v b="${t1}" 'BEGIN{printf "%.3f", b-a}')"
                script_log "feed ${tag} ${fam}/${base} gtfeed_rc=${rc} sec=${dt}"
                if [[ ! -f "${log}" ]]; then
                    script_log "feed ${tag} ${fam}/${base} no log file"
                else
                    script_log "feed ${tag} ${fam}/${base} log_tail=$(tail -n 1 "${log}" | tr '\r' ' ')"
                fi
                ph="$(pico_hit "${log}")"
                if [[ "${cnt}" -eq 0 ]]; then
                    PICO0_N["${key}"]=$((PICO0_N["${key}"] + 1))
                    PICO0_H["${key}"]=$((PICO0_H["${key}"] + ph))
                else
                    PICO1_N["${key}"]=$((PICO1_N["${key}"] + 1))
                    PICO1_H["${key}"]=$((PICO1_H["${key}"] + ph))
                fi
                n_wav=$((n_wav + 1))
                script_log "feed ${tag} ${fam}/${base} hit=${ph}"
            done
        done
    done
}

atest_sweep() {
    local f="$1"
    echo "==== atest -B 1200 -F ${f} ===="
    for fam in "${families[@]}"; do
        for s in ${SNR}; do
            local key="${fam},${s}"
            mapfile -t WAVS < <(list_wavs "${fam}" "${s}")
            if [[ ${#WAVS[@]} -eq 0 ]]; then
                continue
            fi
            for wav in "${WAVS[@]}"; do
                local base="${wav##*/}"
                local ah
                ah="$(atest_hit "${wav}" "${f}")"
                if [[ "${f}" -eq 0 ]]; then
                    ATEST0_N["${key}"]=$((ATEST0_N["${key}"] + 1))
                    ATEST0_H["${key}"]=$((ATEST0_H["${key}"] + ah))
                else
                    ATEST1_N["${key}"]=$((ATEST1_N["${key}"] + 1))
                    ATEST1_H["${key}"]=$((ATEST1_H["${key}"] + ah))
                fi
                echo "  atest-F${f} ${fam}/${base} hit=${ah}"
            done
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

if [[ "${n_wav}" -eq 0 && "${DO_PICO}" -eq 1 ]]; then
    echo "FAIL: no WAVs under ${CORPUS}/{dw,ours}/sNN_XXX.wav" >&2
    exit 1
fi
if [[ "${DO_PICO}" -eq 0 && "${DO_ATEST}" -eq 1 ]]; then
    n_chk=0
    for fam in "${families[@]}"; do
        for s in ${SNR}; do
            n_chk=$((n_chk + ATEST1_N["${fam},${s}"] + ATEST0_N["${fam},${s}"]))
        done
    done
    if [[ "${n_chk}" -eq 0 ]]; then
        echo "FAIL: no WAVs under ${CORPUS}/{dw,ours}/sNN_XXX.wav" >&2
        exit 1
    fi
fi

echo
echo "========== summary =========="
printf '%-6s %3s  %-10s  %-10s  %-10s  %-10s\n' \
    "family" "snr" "pico-F1" "pico-F0" "atest-F1" "atest-F0"
printf '%-6s %3s  %-10s  %-10s  %-10s  %-10s\n' \
    "------" "---" "----------" "----------" "----------" "----------"
for fam in "${families[@]}"; do
    for s in ${SNR}; do
        key="${fam},${s}"
        printf '%-6s %3s  %-10s  %-10s  %-10s  %-10s\n' \
            "${fam}" "${s}" \
            "$(frac "${PICO1_H[${key}]}" "${PICO1_N[${key}]}")" \
            "$(frac "${PICO0_H[${key}]}" "${PICO0_N[${key}]}")" \
            "$(frac "${ATEST1_H[${key}]}" "${ATEST1_N[${key}]}")" \
            "$(frac "${ATEST0_H[${key}]}" "${ATEST0_N[${key}]}")"
    done
done
echo "============================="
echo "order: Pico F1, Pico F0, atest -F 1, atest -F 0"
echo "hit = ≥1 good FCS (Pico AX25 clustered 25 ms; atest packets>0)"
echo "host kzii reference dw: 3≈1/50  4≈11/50  5≈32/50  6≈35/50"
echo "logs ${LOGDIR}  (*.f1.feed.log / *.f0.feed.log)"
exit 0
