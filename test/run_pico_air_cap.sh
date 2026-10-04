#!/usr/bin/env bash
# Phase 2 capture: one WAV per trial. start gtwav → gttx → stop gtwav.
# RX port is dump-only (no gtdump on that CDC).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_air_cap.sh --tx PATH --rx PATH [options]

  --tx PATH --rx PATH
      --count N     default 100
      --ui-file F   default test/corpus/air/ui.txt
      --src / --dst default TEST-0
      --pre S       settle after DUMP_ON before TX (default 0.4)
      --post S      keep recording after Tx done (default 0.4)
      --gap-min/--gap-max  pause after DUMP_OFF (default 2 3)
      --raw-dir DIR default test/corpus/air/raw
      --gttx --gtwav PATH

Writes raw/000.wav … one file per stored UI. No split step.
EOF
}

TX=""
RX=""
COUNT=100
UI_FILE="${ROOT}/test/corpus/air/ui.txt"
SRC="TEST-0"
DST="TEST-0"
PRE=0.4
POST=0.4
GAP_MIN=2
GAP_MAX=3
RAW_DIR="${ROOT}/test/corpus/air/raw"
GTTX="${ROOT}/host/bin/gttx"
GTWAV="${ROOT}/host/bin/gtwav"

while [[ $# -gt 0 ]]; do
    case "$1" in
    --tx) TX="$2"; shift 2 ;;
    --rx) RX="$2"; shift 2 ;;
    --count) COUNT="$2"; shift 2 ;;
    --ui-file) UI_FILE="$2"; shift 2 ;;
    --src) SRC="$2"; shift 2 ;;
    --dst) DST="$2"; shift 2 ;;
    --pre) PRE="$2"; shift 2 ;;
    --post) POST="$2"; shift 2 ;;
    --gap-min) GAP_MIN="$2"; shift 2 ;;
    --gap-max) GAP_MAX="$2"; shift 2 ;;
    --raw-dir) RAW_DIR="$2"; shift 2 ;;
    --gttx) GTTX="$2"; shift 2 ;;
    --gtwav) GTWAV="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "FAIL: unknown $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ ! -x "${GTTX}" && -x "${ROOT}/build/host/gttx" ]]; then
    GTTX="${ROOT}/build/host/gttx"
fi
if [[ ! -x "${GTWAV}" && -x "${ROOT}/build/host/gtwav" ]]; then
    GTWAV="${ROOT}/build/host/gtwav"
fi
if [[ -z "${TX}" || -z "${RX}" || "${TX}" == "${RX}" ]]; then
    echo "FAIL: need distinct --tx and --rx" >&2
    exit 1
fi
if [[ ! -e "${TX}" || ! -e "${RX}" ]]; then
    echo "FAIL: device missing" >&2
    exit 1
fi
if [[ ! -x "${GTTX}" || ! -x "${GTWAV}" ]]; then
    echo "FAIL: need gttx and gtwav" >&2
    exit 1
fi
if [[ ! -f "${UI_FILE}" ]]; then
    echo "FAIL: no ${UI_FILE}" >&2
    exit 1
fi

mapfile -t UIS < "${UI_FILE}"
if [[ ${#UIS[@]} -lt ${COUNT} ]]; then
    echo "FAIL: ui file short" >&2
    exit 1
fi

now_ts() { date '+%F %T.%3N'; }
script_log() { printf '%s [script] %s\n' "$(now_ts)" "$*"; }

WAV_PID=""
stop_wav() {
    if [[ -n "${WAV_PID}" ]] && kill -0 "${WAV_PID}" 2>/dev/null; then
        kill -INT "${WAV_PID}" 2>/dev/null || true
        wait "${WAV_PID}" 2>/dev/null || true
    fi
    WAV_PID=""
}
trap stop_wav EXIT INT TERM

mkdir -p "${RAW_DIR}"
ok=0
for ((i = 0; i < COUNT; i++)); do
    ui="${UIS[i]}"
    wav="$(printf '%s/%03d.wav' "${RAW_DIR}" "${i}")"
    script_log "rec $((i + 1))/${COUNT} ${wav}"
    "${GTWAV}" -d "${RX}" -o "${wav}" &
    WAV_PID=$!
    sleep "${PRE}"
    if ! kill -0 "${WAV_PID}" 2>/dev/null; then
        echo "FAIL: gtwav died on ${wav}" >&2
        exit 1
    fi
    set +e
    "${GTTX}" -d "${TX}" -s "${SRC}" -t "${DST}" --text "${ui}"
    rc=$?
    set -e
    if [[ "${rc}" -ne 0 ]]; then
        script_log "gttx rc=${rc}"
    fi
    sleep "${POST}"
    stop_wav
    if [[ ! -s "${wav}" ]]; then
        echo "FAIL: empty ${wav}" >&2
        exit 1
    fi
    ok=$((ok + 1))
    if [[ "${i}" -lt $((COUNT - 1)) ]]; then
        gap="$(awk -v a="${GAP_MIN}" -v b="${GAP_MAX}" -v r="${RANDOM}" \
            'BEGIN{printf "%.3f", a + (b-a)*(r/32767)}')"
        sleep "${gap}"
    fi
done

echo "raw ${ok} files in ${RAW_DIR}"
exit 0
