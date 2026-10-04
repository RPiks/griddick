#!/usr/bin/env bash
# Pick nc or ncat. Prints the command prefix on stdout.
# OpenBSD nc (Debian) and nmap ncat (Fedora) both accept:
#   -u -s LOCAL -p PORT PEER PORT
set -euo pipefail

if command -v ncat >/dev/null 2>&1; then
    echo ncat
    exit 0
fi
if command -v nc >/dev/null 2>&1; then
    echo nc
    exit 0
fi
echo "gdchat: install nc or ncat" >&2
exit 1
