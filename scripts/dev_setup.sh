#!/usr/bin/env bash
# One-shot dev environment bootstrap
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

check() { command -v "$1" &>/dev/null || { echo "Missing: $1"; exit 1; }; }

check cmake; check cargo; check go; check python3; check protoc

# Python venv — skip inside a Nix shell (packages already provided by flake.nix)
if [[ -z "${IN_NIX_SHELL:-}" ]]; then
    check pip3
    echo "==> Python virtualenv…"
    python3 -m venv "$ROOT/src/scraper/.venv"
    source "$ROOT/src/scraper/.venv/bin/activate"
    pip install -r "$ROOT/src/scraper/requirements.txt"
    pip install -e "$ROOT/src/scraper"  # yt-load, yt-gameplay-search entry points
else
    echo "==> Nix shell detected — registering entry points via pip…"
    pip install -e "$ROOT/src/scraper" --no-build-isolation
fi

echo "==> Generating Protobuf stubs…"

# Python
protoc --python_out="$ROOT/src/scraper" \
       --proto_path="$ROOT/proto" \
       "$ROOT/proto/hypervane.proto"

# Go
protoc --go_out="$ROOT/src/connectors" \
       --go_opt=paths=source_relative \
       --proto_path="$ROOT/proto" \
       "$ROOT/proto/hypervane.proto"

echo "==> Go module tidy…"
(cd "$ROOT/src/connectors" && go mod tidy)

echo "==> Dev environment ready."
