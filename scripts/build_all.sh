#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

echo "==> Building DB engine (Rust)…"
cargo build --release --manifest-path "$ROOT/src/db_engine/Cargo.toml"

echo "==> Building connectors (Go)…"
(cd "$ROOT/src/connectors" && go build -o bin/hypervane-connector ./...)

echo "==> Building UI (C++ / Qt6)…"
cmake -S "$ROOT/src/ui" -B "$ROOT/src/ui/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/src/ui/build" --parallel "$(nproc)"

echo "==> All subsystems built successfully."
