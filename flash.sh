#!/usr/bin/env bash
# Build griddick_tnc.uf2 and flash a Raspberry Pi Pico over USB (picotool -f).
# Optional env:
#   SERIAL_DEV   preferred CDC node after reboot (default: auto, e.g. /dev/ttyACM0)
#   PICOTOOL     path to picotool (default: tools/bin/picotool or $PATH)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="${ROOT}/third_party/pico-sdk"
BUILD_DIR="${ROOT}/build/firmware"
export PICO_SDK_PATH="${PICO_SDK_PATH:-${SDK_DIR}}"

if [[ -n "${PICOTOOL:-}" ]]; then
    :
elif [[ -x "${ROOT}/tools/bin/picotool" ]]; then
    PICOTOOL="${ROOT}/tools/bin/picotool"
elif command -v picotool >/dev/null; then
    PICOTOOL="$(command -v picotool)"
else
    echo "picotool not found. Run: ${ROOT}/setup_env.sh" >&2
    exit 1
fi

"${ROOT}/build.sh"

UF2="${ROOT}/griddick_tnc.uf2"
if [[ ! -f "${UF2}" ]]; then
    FOUND="$(find "${BUILD_DIR}" -name 'griddick_tnc.uf2' -print -quit || true)"
    if [[ -n "${FOUND}" ]]; then
        UF2="${FOUND}"
    fi
fi
if [[ ! -f "${UF2}" ]]; then
    echo "Build did not produce griddick_tnc.uf2" >&2
    exit 1
fi
echo "    using UF2 ${UF2}"

pico_in_bootsel() {
    lsusb 2>/dev/null | grep -qi '2e8a:0003'
}

echo "==> Flashing via picotool (${PICOTOOL})..."
if pico_in_bootsel; then
    echo "    BOOTSEL device detected; loading without force..."
    if ! "${PICOTOOL}" load -x "${UF2}"; then
        echo "Flash failed while Pico was in BOOTSEL." >&2
        exit 1
    fi
else
    echo "    attempting forced reset-to-BOOTSEL (picotool -f)..."
    if ! "${PICOTOOL}" load -f -x "${UF2}"; then
        cat <<'EOT' >&2

Flash failed. For a blank Pico:
  1. Unplug USB
  2. Hold BOOTSEL, plug USB, release BOOTSEL
  3. Re-run ./flash.sh

EOT
        exit 1
    fi
fi

echo "==> Waiting for CDC serial device..."
SERIAL_DEV="${SERIAL_DEV:-}"
sleep 1
for _ in $(seq 1 80); do
    if [[ -n "${SERIAL_DEV}" && -e "${SERIAL_DEV}" ]]; then
        break
    fi
    SERIAL_DEV=""
    for candidate in /dev/serial/by-id/usb-Raspberry_Pi_Pico_*-if00 /dev/ttyACM*; do
        if [[ -e "${candidate}" ]]; then
            SERIAL_DEV="${candidate}"
            break
        fi
    done
    [[ -n "${SERIAL_DEV}" ]] && break
    sleep 0.25
done

if [[ -z "${SERIAL_DEV}" || ! -e "${SERIAL_DEV}" ]]; then
    echo "No Pico CDC serial device appeared after flash." >&2
    exit 1
fi
echo "    using ${SERIAL_DEV}"
echo "==> Flash complete. KISS CDC should be on ${SERIAL_DEV}"
