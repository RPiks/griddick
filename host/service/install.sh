#!/usr/bin/env bash
# Install host tools + griddickd under /opt/griddick.
# Copies griddickd.conf.example. Does not enable or start the service.
# Does not overwrite an existing /opt/griddick/griddickd.conf.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HOST="${ROOT}/host"
PREFIX="/opt/griddick"
UNIT_SRC="${HOST}/service/griddickd.service"
UNIT_DST="/etc/systemd/system/griddickd.service"
EXAMPLE_SRC="${HOST}/griddickd/scripts/griddickd.conf.example"

if [[ "$(id -u)" -ne 0 ]]; then
    exec sudo -- "$0" "$@"
fi

need() {
    if [[ ! -e "$1" ]]; then
        echo "install: missing $1 (build host first)" >&2
        exit 1
    fi
}

need "${HOST}/lib/libgtnc.so"
need "${HOST}/bin/griddickd"
need "${EXAMPLE_SRC}"
need "${UNIT_SRC}"

if ! getent group griddick >/dev/null; then
    groupadd --system griddick
fi
if ! getent passwd griddick >/dev/null; then
    useradd --system --gid griddick --home "${PREFIX}" \
        --shell /usr/sbin/nologin --comment "Griddick TNC" griddick
fi
if getent group dialout >/dev/null; then
    usermod -aG dialout griddick
fi

install -d -m 0755 -o root -g griddick "${PREFIX}" "${PREFIX}/bin" "${PREFIX}/lib"

install -m 0755 -o root -g root "${HOST}/lib/libgtnc.so" "${PREFIX}/lib/libgtnc.so"

fix_link() {
    local dest="$1"
    if ! command -v patchelf >/dev/null; then
        return 0
    fi
    while IFS= read -r need; do
        case "${need}" in
            */libgtnc.so)
                patchelf --replace-needed "${need}" libgtnc.so "${dest}"
                ;;
        esac
    done < <(patchelf --print-needed "${dest}" 2>/dev/null || true)
    patchelf --set-rpath '$ORIGIN/../lib' "${dest}"
}

for b in gtlog gttime gttx gtwav gtfeed gtcfg gtdump gtcall griddickd; do
    need "${HOST}/bin/${b}"
    install -m 0755 -o root -g root "${HOST}/bin/${b}" "${PREFIX}/bin/${b}"
    fix_link "${PREFIX}/bin/${b}"
done
if [[ -x "${HOST}/bin/gtstation" ]]; then
    install -m 0755 -o root -g root "${HOST}/bin/gtstation" "${PREFIX}/bin/gtstation"
    fix_link "${PREFIX}/bin/gtstation"
fi

install -m 0644 -o root -g griddick "${EXAMPLE_SRC}" \
    "${PREFIX}/griddickd.conf.example"
if [[ ! -f "${PREFIX}/griddickd.conf" ]]; then
    echo "install: no ${PREFIX}/griddickd.conf (copy the example and edit)"
fi

install -m 0644 -o root -g root "${UNIT_SRC}" "${UNIT_DST}"
install -d -m 0755 /etc/profile.d /usr/bin
install -m 0644 -o root -g root "${HOST}/service/griddick.sh" \
    /etc/profile.d/griddick.sh
for b in gtlog gttime gttx gtwav gtfeed gtcfg gtdump gtcall griddickd; do
    ln -sfn "${PREFIX}/bin/${b}" "/usr/bin/${b}"
done
if [[ -e "${PREFIX}/bin/gtstation" ]]; then
    ln -sfn "${PREFIX}/bin/gtstation" /usr/bin/gtstation
fi
if command -v systemctl >/dev/null; then
    systemctl daemon-reload
fi

echo "install: prefix ${PREFIX}"
echo "install: unit ${UNIT_DST} (disabled)"
echo "install: enable with: systemctl enable --now griddickd"
echo "install: after copying ${PREFIX}/griddickd.conf.example -> griddickd.conf"
