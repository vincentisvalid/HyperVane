#!/usr/bin/env python3
"""
Mock IPC daemon for testing the HyperVane Qt6 UI.

Usage:
  python3 scripts/mock_server.py [--video /path/to/clip.mp4]

Start this first, then launch the UI binary. On each connection it immediately
pushes fake ROM data so the library grid populates, and (optionally) sends a
preview_ready message so the video player fires.
"""

import argparse
import asyncio
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "src" / "scraper"))

try:
    import hypervane_pb2 as pb
except ModuleNotFoundError:
    sys.exit(
        "hypervane_pb2.py not found.\n"
        "Run 'bash scripts/dev_setup.sh' inside the Nix shell first."
    )

SOCKET_PATH = "/tmp/hypervane.sock"
_FRAME = struct.Struct(">I")   # 4-byte big-endian length prefix

FAKE_ROMS = [
    ("snes-001", "Super Mario World",              "SNES",    "USA"),
    ("snes-002", "The Legend of Zelda: A Link to the Past", "SNES", "USA"),
    ("snes-003", "Donkey Kong Country",            "SNES",    "USA"),
    ("gen-001",  "Sonic the Hedgehog 2",           "Genesis", "USA"),
    ("gen-002",  "Streets of Rage 2",              "Genesis", "USA"),
    ("ps1-001",  "Final Fantasy VII",              "PS1",     "USA"),
    ("ps1-002",  "Metal Gear Solid",               "PS1",     "USA"),
    ("ps1-003",  "Castlevania: Symphony of the Night", "PS1", "USA"),
    ("n64-001",  "Super Mario 64",                 "N64",     "USA"),
    ("n64-002",  "The Legend of Zelda: Ocarina of Time", "N64", "USA"),
    ("n64-003",  "GoldenEye 007",                  "N64",     "USA"),
    ("gba-001",  "Pokémon FireRed",                "GBA",     "USA"),
    ("gba-002",  "Metroid Fusion",                 "GBA",     "USA"),
    ("gba-003",  "Castlevania: Aria of Sorrow",    "GBA",     "USA"),
    ("dc-001",   "Shenmue",                        "Dreamcast","JPN"),
    ("dc-002",   "Jet Set Radio",                  "Dreamcast","USA"),
]


def _pack(envelope: pb.Envelope) -> bytes:
    data = envelope.SerializeToString()
    return _FRAME.pack(len(data)) + data


async def _send(writer: asyncio.StreamWriter, envelope: pb.Envelope) -> None:
    writer.write(_pack(envelope))
    await writer.drain()


async def handle_client(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
    video_path: str | None,
) -> None:
    addr = writer.get_extra_info("peername", "<unix>")
    print(f"[mock] client connected ({addr})")

    # ── Push ROM library immediately ──────────────────────────────────────────
    env = pb.Envelope()
    for rom_id, title, platform, region in FAKE_ROMS:
        e = env.search_response.results.add()
        e.id        = rom_id
        e.title     = title
        e.platform  = platform
        e.region    = region
        e.file_path = f"/roms/{platform.lower().replace(' ', '_')}/{title}.rom"
        e.verified  = True
    env.search_response.total_hits = len(FAKE_ROMS)
    await _send(writer, env)
    print(f"[mock] → sent {len(FAKE_ROMS)} ROM entries")

    # ── Push preview_ready after 1 s (so the video widget has time to appear) ─
    if video_path:
        await asyncio.sleep(1.0)
        env2 = pb.Envelope()
        env2.preview_ready.rom_id    = FAKE_ROMS[0][0]
        env2.preview_ready.file_path = video_path
        await _send(writer, env2)
        print(f"[mock] → sent preview_ready: {video_path}")

    # ── Drain incoming messages from the UI ───────────────────────────────────
    try:
        while True:
            header = await reader.readexactly(_FRAME.size)
            (length,) = _FRAME.unpack(header)
            payload   = await reader.readexactly(length)

            incoming = pb.Envelope()
            incoming.ParseFromString(payload)
            kind = incoming.WhichOneof("payload")
            print(f"[mock] ← received: {kind}")

            if kind == "launch_request":
                req = incoming.launch_request
                print(f"         rom_id={req.rom_id!r}  emulator={req.emulator_id!r}")
            elif kind == "search_request":
                req = incoming.search_request
                print(f"         query={req.query!r}  platform={req.platform!r}")

    except asyncio.IncompleteReadError:
        pass
    finally:
        writer.close()
        print("[mock] client disconnected")


async def main(video_path: str | None) -> None:
    Path(SOCKET_PATH).unlink(missing_ok=True)
    server = await asyncio.start_unix_server(
        lambda r, w: handle_client(r, w, video_path),
        path=SOCKET_PATH,
    )
    print(f"[mock] HyperVane mock server listening on {SOCKET_PATH}")
    if video_path:
        print(f"[mock] preview video : {video_path}")
    else:
        print("[mock] no --video given; preview player will not be tested")
    print("[mock] start the UI now (./src/ui/build/hypervane-ui)")

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="HyperVane mock IPC server")
    parser.add_argument(
        "--video", metavar="PATH",
        help="Path to an .mp4 to send as a gameplay preview",
    )
    asyncio.run(main(parser.parse_args().video))
