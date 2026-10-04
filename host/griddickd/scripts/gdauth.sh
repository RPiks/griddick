#!/usr/bin/env bash
# Auth check using the existing chat/send scripts.
#
# match     both sides share s3cret — UDP should appear on B's gt0
# mismatch  B uses wrongpass — B must log auth mismatch and not write TUN
#
# Does not start griddickd. Restart the daemon after changing conf
# (password is reloadable via SIGHUP if only Peer lines change).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    echo "Usage: gdauth.sh match|mismatch" >&2
    echo "  match:     A peer-a-auth.conf, B peer-b-auth.conf" >&2
    echo "  mismatch:  A peer-a-auth.conf, B peer-b-badauth.conf" >&2
    echo "Then on A: $ROOT/gdsend.sh 10.64.0.2 'auth-probe'" >&2
    echo "Watch B: journalctl -f -t griddickd   or the daemon stderr" >&2
    exit 1
}

MODE="${1:-}"
if [[ "$MODE" != match && "$MODE" != mismatch ]]; then
    usage
fi

echo "A:  sudo griddickd -c $ROOT/peer-a-auth.conf"
if [[ "$MODE" == match ]]; then
    echo "B:  sudo griddickd -c $ROOT/peer-b-auth.conf"
    echo "Expect: B receives UDP. No 'auth mismatch' line."
else
    echo "B:  sudo griddickd -c $ROOT/peer-b-badauth.conf"
    echo "Expect: B logs 'auth mismatch from JBLADE-8' and no UDP on gt0."
fi
echo "A:  $ROOT/gdsend.sh 10.64.0.2 'auth-probe'"
echo "B:  $ROOT/gdchat.sh 10.64.0.1    # optional listener"
