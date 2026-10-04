#!/usr/bin/env bash
# Phase 0 air harness: Pico A TX, Pico B RX, one host.
# Stored random ASCII UI, random 2–3 s quiet after TX PTT off.
#
# Usage:
#   test/run_pico_air.sh --tx /dev/ttyACM0 --rx /dev/ttyACM1
#   test/run_pico_air.sh --tx /dev/ttyACM0 --rx /dev/ttyACM1 --count 100 --fix 1
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_air.sh --tx PATH --rx PATH [options]

  --tx PATH         TX Pico CDC
  --rx PATH         RX Pico CDC
      --count N     trials (default 10; 100 for a full pass)
      --ui-len N    info length, bytes (default 32)
      --ui-file F   payload list, one UI per line
                    (default test/corpus/air/ui.txt)
      --write-ui    (re)write --ui-file from --seed / --count / --ui-len
      --seed N      PRNG seed for --write-ui (default 1)
      --src CALL    gttx source (default TEST-0)
      --dst CALL    gttx dest (default TEST-0)
      --fix 0|1|both  RX bitFixCnt; both = F1 then F0 (default 0)
      --sanity NAME   none or ASCII (default none)
      --gttx PATH
      --gtdump PATH
      --gtcfg PATH
      --logs DIR    default test/logs/air
      --gap-min S   quiet after TX PTT off (default 2)
      --gap-max S   (default 3)
  -h, --help

Phase 0: desk, open squelch. Scores stored UI strings in the gtdump log.
EOF
}

TX=""
RX=""
COUNT=10
UI_LEN=32
UI_FILE="${ROOT}/test/corpus/air/ui.txt"
WRITE_UI=0
SEED=1
SRC="TEST-0"
DST="TEST-0"
FIX=0
SANITY="none"
GTTX="${ROOT}/host/bin/gttx"
GTDUMP="${ROOT}/host/bin/gtdump"
GTCFG="${ROOT}/host/bin/gtcfg"
LOGDIR="${ROOT}/test/logs/air"
GAP_MIN=2
GAP_MAX=3

while [[ $# -gt 0 ]]; do
    case "$1" in
    --tx)
        TX="$2"
        shift 2
        ;;
    --rx)
        RX="$2"
        shift 2
        ;;
    --count)
        COUNT="$2"
        shift 2
        ;;
    --ui-len)
        UI_LEN="$2"
        shift 2
        ;;
    --ui-file)
        UI_FILE="$2"
        shift 2
        ;;
    --write-ui)
        WRITE_UI=1
        shift
        ;;
    --seed)
        SEED="$2"
        shift 2
        ;;
    --src)
        SRC="$2"
        shift 2
        ;;
    --dst)
        DST="$2"
        shift 2
        ;;
    --fix)
        FIX="$2"
        shift 2
        ;;
    --sanity)
        SANITY="$2"
        shift 2
        ;;
    --gttx)
        GTTX="$2"
        shift 2
        ;;
    --gtdump)
        GTDUMP="$2"
        shift 2
        ;;
    --gtcfg)
        GTCFG="$2"
        shift 2
        ;;
    --logs)
        LOGDIR="$2"
        shift 2
        ;;
    --gap-min)
        GAP_MIN="$2"
        shift 2
        ;;
    --gap-max)
        GAP_MAX="$2"
        shift 2
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

if [[ ! -x "${GTTX}" && -x "${ROOT}/build/host/gttx" ]]; then
    GTTX="${ROOT}/build/host/gttx"
fi
if [[ ! -x "${GTDUMP}" && -x "${ROOT}/build/host/gtdump" ]]; then
    GTDUMP="${ROOT}/build/host/gtdump"
fi
if [[ ! -x "${GTCFG}" && -x "${ROOT}/build/host/gtcfg" ]]; then
    GTCFG="${ROOT}/build/host/gtcfg"
fi

if [[ -z "${TX}" || -z "${RX}" ]]; then
    echo "FAIL: need --tx and --rx" >&2
    usage >&2
    exit 1
fi
if [[ "${TX}" == "${RX}" ]]; then
    echo "FAIL: --tx and --rx must differ" >&2
    exit 1
fi
case "${FIX}" in
0|1|both) ;;
*)
    echo "FAIL: --fix must be 0, 1, or both" >&2
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
if [[ ! -x "${GTTX}" || ! -x "${GTDUMP}" || ! -x "${GTCFG}" ]]; then
    echo "FAIL: need gttx/gtdump/gtcfg" >&2
    exit 1
fi
if [[ ! -e "${TX}" ]]; then
    echo "FAIL: no TX device ${TX}" >&2
    exit 1
fi
if [[ ! -e "${RX}" ]]; then
    echo "FAIL: no RX device ${RX}" >&2
    exit 1
fi

now_ts() {
    date '+%F %T.%3N'
}

script_log() {
    printf '%s [script] %s\n' "$(now_ts)" "$*"
}

write_ui_file() {
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

if [[ "${WRITE_UI}" -eq 1 || ! -f "${UI_FILE}" ]]; then
    script_log "write ${UI_FILE} count=${COUNT} len=${UI_LEN} seed=${SEED}"
    write_ui_file "${COUNT}" "${UI_LEN}" "${SEED}" "${UI_FILE}"
fi

mapfile -t UIS < "${UI_FILE}"
if [[ ${#UIS[@]} -eq 0 ]]; then
    echo "FAIL: empty ${UI_FILE}" >&2
    exit 1
fi
if [[ ${#UIS[@]} -lt ${COUNT} ]]; then
    echo "FAIL: ${UI_FILE} has ${#UIS[@]} lines, need ${COUNT}" >&2
    exit 1
fi

set_bitfix() {
    local cnt="$1"
    local js rc
    js="$(mktemp)"
    printf '{"modem":{"bitFixCnt":%s,"bitFixAlgo":0,"bitFixSanity":"%s"}}\n' \
        "${cnt}" "${SANITY}" >"${js}"
    set +e
    "${GTCFG}" -d "${RX}" --put "${js}"
    rc=$?
    set -e
    rm -f "${js}"
    if [[ "${rc}" -ne 0 ]]; then
        echo "FAIL: gtcfg on ${RX}" >&2
        exit 1
    fi
}

DUMP_PID=""
cleanup() {
    if [[ -n "${DUMP_PID}" ]] && kill -0 "${DUMP_PID}" 2>/dev/null; then
        kill "${DUMP_PID}" 2>/dev/null || true
        wait "${DUMP_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

mkdir -p "${LOGDIR}"
HIT_F0="-"
HIT_F1="-"

air_pass() {
    local cnt="$1"
    local stamp dump_log run_log hit sent i ui rc gap
    stamp="$(date +%Y%m%d_%H%M%S)"
    dump_log="${LOGDIR}/rx_${stamp}_f${cnt}.dump.log"
    run_log="${LOGDIR}/run_${stamp}_f${cnt}.log"
    script_log "tx=${TX} rx=${RX} fix=${cnt} count=${COUNT}" | tee -a "${run_log}"
    set_bitfix "${cnt}"
    script_log "gtcfg bitFixCnt=${cnt} on ${RX}" | tee -a "${run_log}"
    "${GTDUMP}" -d "${RX}" --no-sync -o "${dump_log}" >/dev/null &
    DUMP_PID=$!
    sleep 0.4
    if ! kill -0 "${DUMP_PID}" 2>/dev/null; then
        echo "FAIL: gtdump died" >&2
        exit 1
    fi
    sent=0
    for ((i = 0; i < COUNT; i++)); do
        ui="${UIS[i]}"
        script_log "tx $((i + 1))/${COUNT} f${cnt}" | tee -a "${run_log}"
        set +e
        "${GTTX}" -d "${TX}" -s "${SRC}" -t "${DST}" --text "${ui}" >>"${run_log}" 2>&1
        rc=$?
        set -e
        if [[ "${rc}" -ne 0 ]]; then
            script_log "gttx rc=${rc}" | tee -a "${run_log}"
        fi
        sent=$((sent + 1))
        if [[ "${i}" -lt $((COUNT - 1)) ]]; then
            gap="$(awk -v a="${GAP_MIN}" -v b="${GAP_MAX}" -v r="${RANDOM}" \
                'BEGIN{printf "%.3f", a + (b-a)*(r/32767)}')"
            sleep "${gap}"
        fi
    done
    sleep 2
    cleanup
    DUMP_PID=""
    hit=0
    for ((i = 0; i < COUNT; i++)); do
        ui="${UIS[i]}"
        if grep -F -q -- "${ui}" "${dump_log}"; then
            hit=$((hit + 1))
        fi
    done
    if [[ "${cnt}" -eq 1 ]]; then
        HIT_F1="${hit}/${COUNT}"
    else
        HIT_F0="${hit}/${COUNT}"
    fi
    script_log "pass f${cnt} sent=${sent} hit=${hit}/${COUNT} dump=${dump_log}" \
        | tee -a "${run_log}"
}

if [[ "${FIX}" == "both" ]]; then
    air_pass 1
    air_pass 0
else
    air_pass "${FIX}"
fi

echo
echo "========== air live =========="
printf 'tx %s\nrx %s\n' "${TX}" "${RX}"
printf 'pico-F1 %s\n' "${HIT_F1}"
printf 'pico-F0 %s\n' "${HIT_F0}"
printf 'ui %s\n' "${UI_FILE}"
echo "=============================="
echo "hit = stored UI string seen in gtdump (KISS DATA)"
exit 0
