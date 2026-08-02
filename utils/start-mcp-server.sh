#!/usr/bin/env bash
# Startet andy-mcp-server in einer libvirt-VM und leitet STDIO über socat an TCP weiter

set -euo pipefail

HOST=${1:-0.0.0.0}
PORT=${2:-31234}

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_ROOT=$(cd "${SCRIPT_DIR}/.." && pwd)

echo "Launching andy-mcp-server from ${PROJECT_ROOT} and forwarding to ${HOST}:${PORT}"
command -v socat >/dev/null || { echo "Error: socat required" >&2; exit 1; }

cd "${PROJECT_ROOT}"
./build/andy-mcp-server | socat -d -d - "TCP-LISTEN:${PORT},reuseaddr,fork,bind=${HOST}"