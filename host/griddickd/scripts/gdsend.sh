#!/usr/bin/env bash
# One UDP datagram to a griddickd peer. Smoke check, not a chat.
#
#   echo ping | host/griddickd/scripts/gdsend.sh 10.64.0.2
#   host/griddickd/scripts/gdsend.sh 10.64.0.2 'hello'
#
# Env: IFACE (default gt0), PORT (default 9000), LOCAL (default addr of IFACE)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IFACE="${IFACE:-gt0}"
PORT="${PORT:-9000}"

usage() {
    echo "Usage: gdsend.sh PEER_IP [TEXT]" >&2
    echo "  TEXT default is a single line from stdin." >&2
    exit 1
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" || $# -lt 1 || $# -gt 2 ]]; then
    usage
fi
PEER="$1"

if [[ -z "${LOCAL:-}" ]]; then
    LOCAL="$(ip -4 -o addr show dev "$IFACE" 2>/dev/null | awk '{print $4}' | head -1 | cut -d/ -f1 || true)"
fi
if [[ -z "${LOCAL:-}" ]]; then
    echo "gdsend: no IPv4 on $IFACE (is griddickd running?)" >&2
    exit 1
fi

NC="$("$ROOT/gdnc.sh")"
if [[ $# -eq 2 ]]; then
    TEXT="$2"
else
    TEXT="$(cat)"
fi

# -w1: return after the send on OpenBSD nc and ncat. UDP has no reply wait.
printf '%s\n' "$TEXT" | "$NC" -u -w1 -s "$LOCAL" -p "$PORT" "$PEER" "$PORT"
echo "gdsend $LOCAL:$PORT -> $PEER:$PORT  ${#TEXT} bytes" >&2
