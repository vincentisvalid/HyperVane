# HyperVane

**A Multi-Generational PlayStation Gaming Frontend and Emulator.**

HyperVane is a premium, "Netflix-style" living-room frontend that unifies the entire **Sony PlayStation lineage — PS1, PS2, PS3, and PS4 —** under a single, cinematic Qt6 desktop interface. It manages your ROM/game library, scrapes and plays gameplay previews, indexes everything for instant full-text search, and orchestrates the best-in-class emulator for each console generation — each launched behind an authentic, per-generation boot sequence that masks emulator load times and recreates the feeling of powering on a real console.

Rather than bolting a skin onto a single emulator, HyperVane is a **decoupled, multi-language platform**: a native C++/Qt6 viewport, a Rust search daemon, a Go emulator-process supervisor, and a Python scraping pipeline, all communicating over a typed Protobuf/Unix-socket IPC bus. This separation lets each PlayStation generation — from the 1994 original hardware to the PS4 era — be driven by a purpose-built connector while the frontend stays fast, native, and console-agnostic.

---

## The PlayStation Generations

HyperVane maps each generation of Sony hardware to the industry-standard emulator best suited to drive it, and abstracts every one behind a common Go connector interface.

| Generation | Console | Emulator Core | Why it's the pick | Launch profile |
|:---|:---|:---|:---|:---|
| 5th gen (1994) | **PlayStation 1** | [DuckStation](https://github.com/stenzek/duckstation) | PGXP geometry correction, pristine 4K upscaling, flawless headless CLI | Instant — launch & forget |
| 6th gen (2000) | **PlayStation 2** | [PCSX2](https://pcsx2.net/) | Modern Vulkan backend, clean Qt UI, excellent CLI argument handling | Instant — launch & forget |
| 7th gen (2006) | **PlayStation 3** | [RPCS3](https://rpcs3.net/) | Runs the vast majority of the PS3 library; thrives on high core counts / AVX-512 | **Variable** — PPU/SPU shader compilation, needs a dynamic ready-check |
| 8th gen (2013) | **PlayStation 4** | [shadPS4](https://github.com/shadps4-emu/shadPS4) | Fastest-evolving core in the scene — *Bloodborne* at 4K/60, ShadNet online, Neo/Pro features | **Variable** — Vulkan pipeline warm-up |

> HyperVane also ships an experimental in-tree PS1-class core, **BytesPSX5**, versioned in its own repository.

Because DuckStation and PCSX2 load games almost instantly, their connectors use a simple *launch-and-forget* execution thread. RPCS3 and shadPS4 compile shader/PPU pipelines on boot (anywhere from a few seconds to a couple of minutes), so their connectors run a **dynamic state-checking loop**, watching the emulator process for active frame generation before signalling the frontend to swap away from the boot screen. See [`EMULATION.md`](EMULATION.md) and [`SHADPS4.md`](SHADPS4.md) for the full per-core breakdown.

---

## Cinematic Boot Sequences

The signature HyperVane experience: when you hit **Play**, the frontend goes fullscreen and plays a hardware-accelerated, native **boot intro for that console generation** — the PS1 diamond, the PS2 tower monoliths, the PS3 orchestral ribbon wave — while the emulator spins up hidden in the background. The moment the emulator is ready to push frames, HyperVane crossfades seamlessly into the live game.

This turns an annoying emulator quirk (shader compilation stutter) into a polished, cinematic tension-builder:

- **Native playback, not BIOS boots** — intros render in the Qt6 transition layer via `QMediaPlayer` on the OS multimedia server (PipeWire/VA-API on Linux, Media Foundation/DirectX on Windows), avoiding the window-resizing and audio pops of unconfigurable emulator BIOS boots.
- **Aspect-ratio aware** — 4:3 intros (PS1) are pillar-boxed with an optional CRT scanline shader; 16:9 intros (PS3/PS4) play full-frame.
- **Fixed vs. variable duration** — early consoles use an exact-length video (e.g. PS1 ≈ 7.2s, PS2 ≈ 8.5s); heavier systems like the PS3 use a **two-phase dynamic sequence** — an ambient orchestral "warm-up" wave that loops seamlessly during shader compilation, then a chime + logo climax fired the instant the Go connector reports `EMULATOR_WINDOW_READY`.
- **Skip Intro** — mapping `Start` / gamepad `A` (or any key) triggers an instant alpha-fade to `0.0` and an IPC `INTRO_VIDEO_SKIPPED` signal to drop straight into the action.

Boot assets and per-platform timing live in an asset manifest; see [`CUSTOM_PLATFORM_INTROS.md`](CUSTOM_PLATFORM_INTROS.md) for the intro state machine and [`PS3_BOOTUP_SEQUENCE.md`](PS3_BOOTUP_SEQUENCE.md) for the dynamic orchestral-crossfade design.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Frontend UI (C++ / Qt6)                    │
│  Library grid · Cinematic boot player · Search · Launch      │
└────────────┬────────────────────────────────┬───────────────┘
             │ Protobuf / Unix socket IPC      │
     ┌───────┴───────┐                 ┌───────┴─────────────────┐
     │   ROM Search  │                 │   Emulator Connectors   │
     │   DB (Rust)   │                 │        (Go)             │
     │ SQLite + FTS5 │                 │ DuckStation · PCSX2     │
     │ SHA-1 / CRC32 │                 │ RPCS3 · shadPS4         │
     └───────────────┘                 └─────────────────────────┘
             │
     ┌───────┴───────────────┐
     │  Scraper & Previewer  │
     │  (Python / asyncio)   │
     │  yt-dlp · FFmpeg      │
     └───────────────────────┘
```

| Subsystem | Language | Key Libraries | Rationale |
|:---|:---|:---|:---|
| Frontend UI | C++ / Qt6 | `QtMultimedia`, `QtWidgets` | Native hardware acceleration, zero-GC, deterministic layout & boot-video lifecycle |
| ROM Search DB | Rust | `rusqlite`, `prost`, `tokio` | Memory safety, WAL-mode SQLite, FTS5 full-text search, SHA-1 hashing |
| Emulator Connectors | Go | `protobuf`, `os/exec` | Goroutine-per-process model, clean crash isolation, per-generation launch profiles, trivial cross-compile |
| Scraper & Previewer | Python | `yt-dlp`, `ffmpeg`, `asyncio` | Native yt-dlp ecosystem, fast iteration on scraping pipelines |

All subsystems exchange messages using a shared `proto/hypervane.proto` schema — length-prefixed (4-byte big-endian) Protobuf frames over Unix domain sockets.

---

## Features

- **Multi-generational library** — one grid for PS1 → PS4, each entry tagged with its console badge
- **Per-generation cinematic boot intros** — authentic console startup sequences with dynamic, load-masking transitions
- **Instant search** — keystroke FTS5 queries against the Rust DB engine, filterable by PlayStation generation
- **Gameplay preview player** — a silent preview clip auto-loops on game selection
- **Best-in-class emulator per console** — DuckStation, PCSX2, RPCS3, and shadPS4 via the Go connector layer
- **Variable-load handshake** — for RPCS3/shadPS4, the connector watches for active frame generation before swapping the boot screen for the game
- **Gameplay scraper** — background yt-dlp pipeline encodes preview clips with FFmpeg (H.264/AV1, `-14 LUFS` loudness-normalized)
- **ROM identification & cataloging** — SHA-1/CRC32 hashing matched against DAT files (ROMULUS catalog)
- **Auto-reconnect** — the UI reconnects to backend sockets after a grace period

---

## Getting Started

### Prerequisites (NixOS / Nix flake)

The dev shell provides everything: Qt6, Rust, Go, protoc, FFmpeg, and fonts.

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

Starts the mock IPC server (fake ROM entries) and launches the UI. The HyperVane intro plays on startup.

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

> **Note:** HyperVane is a *frontend and orchestrator* — it does not distribute copyrighted BIOS files or games. You must supply your own legally obtained ROMs/disc images and console firmware for each emulator.

---

## Project Structure

```
HyperVane/
├── proto/                  # Shared Protobuf IPC schema
│   └── hypervane.proto
├── assets/
│   └── platform_intros/    # Per-generation boot/intro videos + manifest.json
├── src/
│   ├── ui/                 # C++/Qt6 frontend + cinematic boot player
│   │   ├── include/
│   │   └── src/
│   ├── db_engine/          # Rust SQLite + FTS5 daemon
│   ├── connectors/         # Go emulator process manager (per-generation profiles)
│   └── scraper/            # Python yt-dlp + FFmpeg preview pipeline
├── ROMULUS/                # ROM identification & catalog tooling (DAT matching)
├── BytesPSX5/              # Experimental in-tree PS1-class core (own repo)
├── scripts/
│   ├── build_all.sh
│   ├── dev_setup.sh
│   ├── run_dev.sh
│   └── gen_test_video.sh   # Regenerate the HyperVane intro .mp4
├── EMULATION.md            # Per-generation emulator breakdown
├── CUSTOM_PLATFORM_INTROS.md  # Cinematic intro state machine spec
├── PS3_BOOTUP_SEQUENCE.md  # Dynamic orchestral crossfade design
├── SHADPS4.md              # PS4 / shadPS4 state of the art
└── flake.nix               # Reproducible NixOS dev environment
```

---

## IPC Protocol

All messages use the `Envelope` oneof pattern from `proto/hypervane.proto`:

| Message | Direction | Purpose |
|:---|:---|:---|
| `SearchRequest` | UI → DB | FTS5 query with optional generation/platform filter |
| `SearchResponse` | DB → UI | Paginated `RomEntry` list with `total_hits` |
| `LaunchRequest` | UI → Connectors | Game path + emulator ID + overrides |
| `KillRequest` | UI → Connectors | Terminate emulator by PID |
| `ProcessEvent` | Connectors → UI | Started / stopped / crashed notification |
| `TransitionSignal` | UI ↔ Connectors | Boot-intro state machine (`LAUNCH_INITIATED`, `INTRO_VIDEO_COMPLETE`, `INTRO_VIDEO_SKIPPED`, `EMULATOR_WINDOW_READY`) |
| `ScrapeProgress` | Scraper → UI | Download progress per game |
| `PreviewReady` | Scraper → UI | Path to encoded `.mp4` preview |

Frames are length-prefixed: 4-byte big-endian uint32 length, then the serialized proto bytes. Maximum frame size is 64 MiB.

---

## Roadmap

- **PS5 / next-gen** connector slot as emulation matures
- **RPCS3 online** via RPCN and **shadPS4 online** via ShadNet integration surfaced in-frontend
- Holiday/easter-egg boot variants driven by the scraper's manifest date checks
- Controller-first navigation and in-game overlay menu

---

## License

MIT — see `LICENSE` for details.

*PlayStation, PS1, PS2, PS3, PS4, and PS5 are trademarks of Sony Interactive Entertainment. HyperVane is an independent frontend and is not affiliated with or endorsed by Sony.*
