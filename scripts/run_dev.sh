#!/usr/bin/env bash
# Start mock server + UI for local development.
# Run from the project root inside a graphical terminal.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BINARY="$ROOT/src/ui/build/hypervane-ui"

# ── Runtime environment fixes ──────────────────────────────────────────────
# Force Wayland — the flake shellHook defaults QT_QPA_PLATFORM=xcb but your
# session is Wayland-only (no X11 display available).
export QT_QPA_PLATFORM=wayland

# Pipewire runtime — Qt Multimedia needs libpipewire-0.3.so for video playback.
# Use the Nix store path (not /run/current-system/sw/lib) to avoid pulling
# in the system libstdc++ which clashes with the Nix shell's ABI.
PIPEWIRE_LIB="/nix/store/6y2b9qs1npwsvcsl9m66hjarzj0yagb4-pipewire-1.6.5/lib"
if [[ -d "$PIPEWIRE_LIB" ]]; then
    export LD_LIBRARY_PATH="${PIPEWIRE_LIB}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

# CUDA stub — FFmpeg tries to dlopen libcuda.so.1 for HW video decode.
# This stub satisfies the dlopen (returns success / 0 devices) so FFmpeg
# falls through to software decoding without spamming "Cannot load libcuda.so.1".
CUDA_STUB_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="${CUDA_STUB_DIR}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# Qt Multimedia backends — the FFmpeg and GStreamer plugins ship inside the
# qtmultimedia package but the dev shell doesn't run wrapQtAppsHook, so we
# must point the plugin loader at them explicitly.
QTMM_PLUGINS="/nix/store/k93w7nvgkrkzqc2a9q4f04c7dg82bri5-qtmultimedia-6.11.1/lib/qt-6/plugins"
QTBASE_PLUGINS="/nix/store/b1zm7dq5gf2593fmk575wgr0nvr2ngld-qtbase-6.11.1/lib/qt-6/plugins"
export QT_PLUGIN_PATH="${QTMM_PLUGINS}${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_PLUGIN_PATH="${QTBASE_PLUGINS}${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"

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

# Ensure yt-load / yt-gameplay-search modules are importable.
export PYTHONPATH="${ROOT}/src/scraper${PYTHONPATH:+:$PYTHONPATH}"

echo "Starting mock server..."
python3 "$ROOT/scripts/mock_server.py" &
MOCK_PID=$!

echo "Starting scraper daemon..."
python3 "$ROOT/src/scraper/main.py" &
SCRAPER_PID=$!

echo "Starting Socolite social server..."
python3 "$ROOT/scripts/mock_social_server.py" &
SOCIAL_PID=$!

echo "Starting ROM download server..."
python3 "$ROOT/scripts/mock_rom_server.py" &
ROM_PID=$!

trap 'kill $MOCK_PID $SCRAPER_PID $SOCIAL_PID $ROM_PID 2>/dev/null; exit' INT TERM EXIT

# Give servers a moment to bind their sockets.
sleep 0.5

echo "Launching HyperVane..."
"$BINARY"
