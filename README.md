# HyperVane

**Intelligent, high-performance emulation frontend, scraper, and library manager.**

HyperVane is a decoupled, multi-language platform that unifies ROM library management, automated gameplay preview scraping, full-text search, and multi-emulator process control under a single Qt6 desktop interface — connected over a typed Protobuf/Unix-socket IPC bus.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Frontend UI (C++ / Qt6)                  │
│  Library grid · Preview player · Search bar · Launch button │
└────────────┬────────────────────────────────┬───────────────┘
             │ Protobuf / Unix socket IPC      │
     ┌───────┴───────┐                 ┌───────┴────────┐
     │   ROM Search  │                 │   Emulator     │
     │   DB (Rust)   │                 │ Connectors (Go)│
     │ SQLite + FTS5 │                 │ DuckStation    │
     │ SHA-1 / CRC32 │                 │ RetroArch      │
     └───────────────┘                 │ Dolphin        │
                                       └────────────────┘
             │
     ┌───────┴───────────────┐
     │  Scraper & Previewer  │
     │  (Python / asyncio)   │
     │  yt-dlp · FFmpeg      │
     └───────────────────────┘
```

| Subsystem | Language | Key Libraries | Rationale |
|:---|:---|:---|:---|
| Frontend UI | C++ / Qt6 | `QtMultimedia`, `QtWidgets` | Native hardware acceleration, zero-GC, deterministic layout lifecycle |
| ROM Search DB | Rust | `rusqlite`, `prost`, `tokio` | Memory safety, WAL-mode SQLite, FTS5 full-text search, SHA-1 hashing |
| Emulator Connectors | Go | `protobuf`, `os/exec` | Goroutine-per-process model, clean crash isolation, trivial cross-compile |
| Scraper & Previewer | Python | `yt-dlp`, `ffmpeg`, `asyncio` | Native yt-dlp ecosystem, fast iteration on scraping pipelines |

All subsystems exchange messages using a shared `proto/hypervane.proto` schema — length-prefixed (4-byte big-endian) Protobuf frames over Unix domain sockets.

---

## Features

- **Boot intro** — animated HyperVane splash on launch (rotating blue vane blades, fade-in title); click or press any key to skip
- **Library grid** — cover-art cards with platform badge; single-click to select, double-click to launch
- **Live search** — keystroke FTS5 query against the Rust DB engine, filtered by platform
- **Preview player** — 30-second silent `.mp4` loop auto-plays on ROM selection
- **Multi-emulator launch** — DuckStation, RetroArch, Dolphin via the Go connector layer
- **Gameplay scraper** — background yt-dlp pipeline encodes preview clips with ffmpeg
- **Auto-reconnect** — UI reconnects to backend sockets after a 3-second grace period

---

## Getting Started

### Prerequisites (NixOS / Nix flake)

The dev shell provides everything: Qt6, Rust 1.75, Go, protoc, ffmpeg, dejavu fonts.

```bash
nix develop --extra-experimental-features "nix-command flakes"
```

Add to `/etc/nixos/configuration.nix` to make flakes permanent:

```nix
nix.settings.experimental-features = [ "nix-command" "flakes" ];
```

### First-time setup

```bash
bash scripts/dev_setup.sh   # install Python deps into .venv
bash scripts/build_all.sh   # build Rust DB, Go connectors, C++/Qt6 UI
```

### Run (development)

```bash
bash scripts/run_dev.sh
```

Starts the mock IPC server (16 fake ROM entries) and launches the UI. The HyperVane intro plays on startup.

### Manual build steps

```bash
# Rust DB engine
cd src/db_engine && cargo build --release

# Go connectors
cd src/connectors && go build -o hypervane-connectors ./...

# C++/Qt6 UI
cd src/ui && mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

---

## Project Structure

```
HyperVane/
├── proto/                  # Shared Protobuf IPC schema
│   └── hypervane.proto
├── assets/
│   └── platform_intros/    # Boot/intro videos per platform
├── src/
│   ├── ui/                 # C++/Qt6 frontend
│   │   ├── include/
│   │   └── src/
│   ├── db_engine/          # Rust SQLite + FTS5 daemon
│   ├── connectors/         # Go emulator process manager
│   └── scraper/            # Python yt-dlp + ffmpeg pipeline
├── scripts/
│   ├── build_all.sh
│   ├── dev_setup.sh
│   ├── run_dev.sh
│   └── gen_test_video.sh   # Regenerate HyperVane intro .mp4
└── flake.nix               # Reproducible NixOS dev environment
```

---

## IPC Protocol

All messages use the `Envelope` oneof pattern from `proto/hypervane.proto`:

| Message | Direction | Purpose |
|:---|:---|:---|
| `SearchRequest` | UI → DB | FTS5 query with optional platform filter |
| `SearchResponse` | DB → UI | Paginated `RomEntry` list with `total_hits` |
| `LaunchRequest` | UI → Connectors | ROM path + emulator ID + overrides |
| `KillRequest` | UI → Connectors | Terminate emulator by PID |
| `ProcessEvent` | Connectors → UI | Started / stopped / crashed notification |
| `ScrapeProgress` | Scraper → UI | Download progress per ROM |
| `PreviewReady` | Scraper → UI | Path to encoded `.mp4` preview |

Frames are length-prefixed: 4-byte big-endian uint32 length, then the serialized proto bytes. Maximum frame size is 64 MiB.

---

## License

MIT — see `LICENSE` for details.
