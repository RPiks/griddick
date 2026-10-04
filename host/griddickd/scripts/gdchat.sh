#!/usr/bin/env bash
# UDP text chat over griddickd TUN.
#
# Both hosts: griddickd already running (this script does not start it).
#   A: griddickd -c host/griddickd/scripts/peer-a.conf
#   B: griddickd -c host/griddickd/scripts/peer-b.conf
#
#   A: host/griddickd/scripts/gdchat.sh 10.64.0.2
#   B: host/griddickd/scripts/gdchat.sh 10.64.0.1
#
# Keep a line under ~200 bytes (AX.25 Info cap).
# Env: IFACE (default gt0), PORT (default 9000), LOCAL (default addr of IFACE)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IFACE="${IFACE:-gt0}"
PORT="${PORT:-9000}"

usage() {
    echo "Usage: gdchat.sh PEER_IP" >&2
    echo "  IFACE=gt0 PORT=9000 LOCAL=10.64.0.1 $0 10.64.0.2" >&2
    exit 1
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -ne 1 ]]; then
    usage
fi
PEER="$1"

if [[ -z "${LOCAL:-}" ]]; then
    LOCAL="$(ip -4 -o addr show dev "$IFACE" 2>/dev/null | awk '{print $4}' | head -1 | cut -d/ -f1 || true)"
fi
if [[ -z "${LOCAL:-}" ]]; then
    echo "gdchat: no IPv4 on $IFACE (is griddickd running?)" >&2
    exit 1
fi

NC="$("$ROOT/gdnc.sh")"
echo "gdchat $LOCAL:$PORT <-> $PEER:$PORT  (Ctrl-C to quit)" >&2
exec "$NC" -u -s "$LOCAL" -p "$PORT" "$PEER" "$PORT"
