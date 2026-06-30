#!/usr/bin/env bash
# Start mock server + UI for local development.
# Run from the project root inside a graphical terminal.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$ROOT/src/ui/build/hypervane-ui"

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found at $BINARY"
    echo "Run: nix develop --extra-experimental-features 'nix-command flakes' --command bash -c 'cd src/ui/build && ninja'"
    exit 1
fi

# Activate virtual env if present (created by dev_setup.sh).
VENV="$ROOT/.venv"
if [[ -d "$VENV" ]]; then
    source "$VENV/bin/activate"
fi

echo "Starting mock server..."
python3 "$ROOT/scripts/mock_server.py" &
MOCK_PID=$!
trap 'kill $MOCK_PID 2>/dev/null; exit' INT TERM EXIT

# Give the mock server a moment to bind the socket.
sleep 0.5

echo "Launching HyperVane..."
"$BINARY"
