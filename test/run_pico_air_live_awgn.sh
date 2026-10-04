#!/usr/bin/env bash
# Live air B: TX Pico PWM AWGN, RX Pico F1 then F0 (gtdump UI hit).
# Does not dump WAV. Capture/score dumps: test/run_pico_tx_awgn.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    cat <<'EOF'
Usage: test/run_pico_air_live_awgn.sh --tx PATH --rx PATH [options]

  --tx PATH         TX Pico (debug.txAwgn*)
  --rx PATH         RX Pico (bitFix F1 then F0)
      --snr LIST    PWM-domain dB (default 3 3.5 4 5 6 7 8)
      --count N     trials per SNR per F (default 20)
      --ui-file F   default test/corpus/air/ui.txt
      --sanity NAME none or ASCII (default none)
      --gap-min/--gap-max  default 2 3
      --gtcfg PATH
      --air PATH    run_pico_air.sh (default test/run_pico_air.sh)
EOF
}

TX=""
RX=""
SNR_LIST="3 3.5 4 5 6 7 8"
COUNT=20
UI_FILE="${ROOT}/test/corpus/air/ui.txt"
SANITY="none"
GAP_MIN=2
GAP_MAX=3
GTCFG="${ROOT}/host/bin/gtcfg"
AIR="${ROOT}/test/run_pico_air.sh"

while [[ $# -gt 0 ]]; do
    case "$1" in
    --tx) TX="$2"; shift 2 ;;
    --rx) RX="$2"; shift 2 ;;
    --snr) SNR_LIST="$2"; shift 2 ;;
    --count) COUNT="$2"; shift 2 ;;
    --ui-file) UI_FILE="$2"; shift 2 ;;
    --sanity) SANITY="$2"; shift 2 ;;
    --gap-min) GAP_MIN="$2"; shift 2 ;;
    --gap-max) GAP_MAX="$2"; shift 2 ;;
    --gtcfg) GTCFG="$2"; shift 2 ;;
    --air) AIR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "FAIL: unknown $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ -z "${TX}" || -z "${RX}" || "${TX}" == "${RX}" ]]; then
    echo "FAIL: need distinct --tx and --rx" >&2
    exit 1
fi
if [[ ! -e "${TX}" || ! -e "${RX}" ]]; then
    echo "FAIL: device missing" >&2
    exit 1
fi
if [[ ! -x "${GTCFG}" ]]; then
    echo "FAIL: gtcfg" >&2
    exit 1
fi
if [[ ! -x "${AIR}" ]]; then
    echo "FAIL: ${AIR}" >&2
    exit 1
fi
case "${SANITY}" in
none|ASCII) ;;
*) echo "FAIL: --sanity" >&2; exit 1 ;;
esac

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
        echo "FAIL: gtcfg TX" >&2
        exit 1
    fi
}

snr_tag() {
    awk -v s="$1" 'BEGIN{
        if (s ~ /\./) printf "%.2f", s+0;
        else printf "%d.00", s+0;
    }'
}

SUMDIR="${ROOT}/test/logs/air_live_awgn"
mkdir -p "${SUMDIR}"
SUM="${SUMDIR}/summary.txt"
: >"${SUM}"

echo "==== air live TX-AWGN ===="
echo "tx ${TX}"
echo "rx ${RX}"
echo "tx ${TX}" >>"${SUM}"
echo "rx ${RX}" >>"${SUM}"
echo "n ${COUNT}" >>"${SUM}"
echo "sanity ${SANITY}" >>"${SUM}"

printf '%-6s  %-10s  %-10s\n' "snr" "pico-F1" "pico-F0" | tee -a "${SUM}"

for snr in ${SNR_LIST}; do
    tag="$(snr_tag "${snr}")"
    echo "---- PWM ${snr} dB  n=${COUNT} ----"
    tx_awgn 1 "${snr}"
    out="${SUMDIR}/${tag}.run.log"
    set +e
    "${AIR}" --tx "${TX}" --rx "${RX}" \
        --count "${COUNT}" --fix both --sanity "${SANITY}" \
        --ui-file "${UI_FILE}" \
        --gap-min "${GAP_MIN}" --gap-max "${GAP_MAX}" \
        --logs "${SUMDIR}/${tag}" | tee "${out}"
    set -e
    f1="$(awk '/^pico-F1 /{print $2}' "${out}" | tail -1)"
    f0="$(awk '/^pico-F0 /{print $2}' "${out}" | tail -1)"
    printf '%-6s  %-10s  %-10s\n' "${tag}" "${f1:--}" "${f0:--}" | tee -a "${SUM}"
done
tx_awgn 0 3
echo "TX inject off"
echo "table ${SUM}"
exit 0
