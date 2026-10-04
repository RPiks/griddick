#!/usr/bin/env bash
# Stop the unit if present. Remove /opt/griddick binaries and the unit.
# Leaves /opt/griddick/griddickd.conf and the griddick user.
set -euo pipefail

PREFIX="/opt/griddick"
UNIT_DST="/etc/systemd/system/griddickd.service"

if [[ "$(id -u)" -ne 0 ]]; then
    exec sudo -- "$0" "$@"
fi

if command -v systemctl >/dev/null; then
    systemctl stop griddickd 2>/dev/null || true
    systemctl disable griddickd 2>/dev/null || true
fi
rm -f "${UNIT_DST}" /etc/profile.d/griddick.sh
for b in gtlog gttime gttx gtwav gtfeed gtcfg gtdump gtcall griddickd gtstation; do
    if [[ -L "/usr/bin/${b}" ]]; then
        tgt="$(readlink "/usr/bin/${b}" || true)"
        case "${tgt}" in
            /opt/griddick/bin/*) rm -f "/usr/bin/${b}" ;;
        esac
    fi
done
if command -v systemctl >/dev/null; then
    systemctl daemon-reload
fi

rm -rf "${PREFIX}/bin" "${PREFIX}/lib"
rm -f "${PREFIX}/griddickd.conf.example"
echo "uninstall: binaries and unit removed"
if [[ -f "${PREFIX}/griddickd.conf" ]]; then
    echo "uninstall: kept ${PREFIX}/griddickd.conf"
fi
