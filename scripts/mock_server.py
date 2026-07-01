#!/usr/bin/env python3
"""
Mock IPC daemon for testing the HyperVane Qt6 UI.

Serves fake ROM data on connect, forwards launch requests to the scraper
daemon, and routes preview_ready messages back to the UI.

Usage:
  python3 scripts/mock_server.py [--scraper-socket /tmp/hypervane-scraper.sock]

Start this first, then launch the UI binary.
"""

import argparse
import asyncio
import hashlib
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
_SCRAPER_SOCKET = "/tmp/hypervane-scraper.sock"
_FRAME = struct.Struct(">I")

# ── ROM database with standard IDs ──────────────────────────────────────

try:
    from scraper.rom_id import RomIDGenerator, RomHasher
except ImportError:
    # Minimal inline fallback for environments where the module isn't available
    import hashlib, re
    class RomIDGenerator:
        @staticmethod
        def from_metadata(platform, region, title, sha1_hash="", counter=0):
            s = platform.upper().strip()
            r = region.upper().strip()[:4]
            t = re.sub(r"[^A-Z0-9]", "", title.upper())
            sfx = sha1_hash[:5].upper() if sha1_hash else hashlib.sha1(title.encode()).hexdigest()[:5].upper()
            return f"{s}-{r}-{t}-{sfx}"
    class RomHasher:
        @staticmethod
        def compute_all(path):
            return {"crc32": "00000000", "sha1": "0"*40, "file_size": 0}

_RAW_ROMS: list[tuple[str, str, str]] = [
    ("Super Mario World",                          "SNES",     "USA"),
    ("The Legend of Zelda: A Link to the Past",    "SNES",     "USA"),
    ("Donkey Kong Country",                        "SNES",     "USA"),
    ("Sonic the Hedgehog 2",                       "Genesis",  "USA"),
    ("Streets of Rage 2",                          "Genesis",  "USA"),
    ("Final Fantasy VII",                          "PS1",      "USA"),
    ("Metal Gear Solid",                           "PS1",      "USA"),
    ("Castlevania: Symphony of the Night",         "PS1",      "USA"),
    ("Super Mario 64",                             "N64",      "USA"),
    ("The Legend of Zelda: Ocarina of Time",       "N64",      "USA"),
    ("GoldenEye 007",                              "N64",      "USA"),
    ("Pok\u00e9mon FireRed",                       "GBA",      "USA"),
    ("Metroid Fusion",                             "GBA",      "USA"),
    ("Castlevania: Aria of Sorrow",                "GBA",      "USA"),
    ("Shenmue",                                    "Dreamcast","JPN"),
    ("Jet Set Radio",                              "Dreamcast","USA"),
]

# Build ROM entries with standard IDs and deterministic fake hashes
FAKE_ROMS: list[dict] = []
for title, platform, region in _RAW_ROMS:
    hashes = RomHasher.compute_all(f"/dev/null")  # placeholder hashes
    fake_sha1 = hashlib.sha1(title.encode()).hexdigest()
    rom_id = RomIDGenerator.from_metadata(platform, region, title, fake_sha1)
    FAKE_ROMS.append({
        "id": rom_id,
        "title": title,
        "platform": platform,
        "region": region,
        "crc32": hashlib.sha1(title.encode()).hexdigest()[:8],
        "sha1": fake_sha1,
    })

_ROM_MAP = {r["id"]: r for r in FAKE_ROMS}


def _populate_rom_entry(entry: pb.RomEntry, rom: dict) -> None:
    entry.id        = rom["id"]
    entry.title     = rom["title"]
    entry.platform  = rom["platform"]
    entry.region    = rom["region"]
    entry.file_path = f"/roms/{rom['platform'].lower().replace(' ', '_')}/{rom['title']}.rom"
    entry.file_size = 4 * 1024 * 1024
    entry.crc32     = rom["crc32"]
    entry.sha1      = rom["sha1"]
    entry.verified  = True


def _pack(envelope: pb.Envelope) -> bytes:
    data = envelope.SerializeToString()
    return _FRAME.pack(len(data)) + data


async def _send(writer: asyncio.StreamWriter, envelope: pb.Envelope) -> None:
    writer.write(_pack(envelope))
    await writer.drain()


UI_WRITERS: set[asyncio.StreamWriter] = set()


async def forward_to_scraper(envelope: pb.Envelope) -> None:
    try:
        _, writer = await asyncio.open_unix_connection(_SCRAPER_SOCKET)
        writer.write(_pack(envelope))
        await writer.drain()
        writer.close()
    except (OSError, ConnectionRefusedError):
        print("[mock] scraper daemon not available — preview will not be fetched")


async def handle_client(
    reader: asyncio.StreamReader,
    writer: asyncio.StreamWriter,
) -> None:
    addr = writer.get_extra_info("peername", "<unix>")
    print(f"[mock] client connected ({addr})")

    # ── Read first message to distinguish scraper from UI ────────────────────
    # Scrapers push preview_ready immediately after connecting.  UIs wait for
    # the ROM library to arrive before the user starts interacting, so a short
    # timeout is safe.
    first = None
    try:
        header = await asyncio.wait_for(reader.readexactly(_FRAME.size), timeout=0.3)
        (length,) = _FRAME.unpack(header)
        payload = await asyncio.wait_for(reader.readexactly(length), timeout=0.3)
        first = pb.Envelope()
        first.ParseFromString(payload)
    except asyncio.TimeoutError:
        pass  # UI client — hasn't sent anything yet

    kind = first.WhichOneof("payload") if first is not None else None

    if kind == "preview_ready":
        # ── Scraper client ─────────────────────────────────────────────────
        req = first.preview_ready
        print(f"         rom_id={req.rom_id!r}  file_path={req.file_path!r}")
        for w in list(UI_WRITERS):
            try:
                await _send(w, first)
            except Exception:
                UI_WRITERS.discard(w)
        writer.close()
        print("[mock] scraper done")
        return

    # ── UI client — push ROM library ──────────────────────────────────────────
    # Send initial scrape progress to show the download bar works
    progress = pb.Envelope()
    progress.scrape_progress.rom_id = ""
    progress.scrape_progress.percent = 30.0
    progress.scrape_progress.stage = "fetch"
    progress.scrape_progress.message = "Fetching ROM library..."
    await _send(writer, progress)

    env = pb.Envelope()
    for rom in FAKE_ROMS:
        _populate_rom_entry(env.search_response.results.add(), rom)
    env.search_response.total_hits = len(FAKE_ROMS)
    await _send(writer, env)
    print(f"[mock] → sent {len(FAKE_ROMS)} ROM entries")

    # Mark library fetch as done
    done = pb.Envelope()
    done.scrape_progress.rom_id = ""
    done.scrape_progress.percent = 100.0
    done.scrape_progress.stage = "done"
    done.scrape_progress.message = "Library ready"
    await _send(writer, done)

    UI_WRITERS.add(writer)
    try:
        # If the UI sent a message before the ROM list finished, process it now.
        if kind is not None:
            print(f"[mock] ← queued: {kind}")
            if kind == "launch_request":
                req = first.launch_request
                print(f"         rom_id={req.rom_id!r}  emulator={req.emulator_id!r}")
                print("[mock] → forwarding to scraper daemon")
                await forward_to_scraper(first)
            elif kind == "search_request":
                req = first.search_request
                query = req.query.lower()
                print(f"         query={query!r}  platform={req.platform!r}")
                resp = pb.Envelope()
                for rom in FAKE_ROMS:
                    if query in rom["title"].lower() or not query:
                        _populate_rom_entry(resp.search_response.results.add(), rom)
                resp.search_response.total_hits = len(resp.search_response.results)
                await _send(writer, resp)
                print(f"[mock] → sent {resp.search_response.total_hits} search results")

        while True:
            header = await reader.readexactly(_FRAME.size)
            (length,) = _FRAME.unpack(header)
            payload   = await reader.readexactly(length)

            incoming = pb.Envelope()
            incoming.ParseFromString(payload)
            kind = incoming.WhichOneof("payload")
            print(f"[mock] ← received: {kind}")

            if kind == "preview_ready":
                req = incoming.preview_ready
                print(f"         rom_id={req.rom_id!r}  file_path={req.file_path!r}")
                for w in list(UI_WRITERS):
                    try:
                        await _send(w, incoming)
                    except Exception:
                        UI_WRITERS.discard(w)

            elif kind == "launch_request":
                req = incoming.launch_request
                print(f"         rom_id={req.rom_id!r}  emulator={req.emulator_id!r}")
                print("[mock] → forwarding to scraper daemon")

                # Simulate fast scrape progress (new audio-aware pipeline)
                prog = pb.Envelope()
                prog.scrape_progress.rom_id = req.rom_id
                prog.scrape_progress.percent = 10.0
                prog.scrape_progress.stage = "fetch"
                prog.scrape_progress.message = "Downloading gameplay with audio..."
                await _send(writer, prog)

                await forward_to_scraper(incoming)

                prog2 = pb.Envelope()
                prog2.scrape_progress.rom_id = req.rom_id
                prog2.scrape_progress.percent = 50.0
                prog2.scrape_progress.stage = "encode"
                prog2.scrape_progress.message = "Encoding with audio..."
                await _send(writer, prog2)

            elif kind == "search_request":
                req = incoming.search_request
                query = req.query.lower()
                print(f"         query={query!r}  platform={req.platform!r}")
                resp = pb.Envelope()
                for rom in FAKE_ROMS:
                    if query in rom["title"].lower() or not query:
                        _populate_rom_entry(resp.search_response.results.add(), rom)
                resp.search_response.total_hits = len(resp.search_response.results)
                await _send(writer, resp)
                print(f"[mock] → sent {resp.search_response.total_hits} search results")

    except asyncio.IncompleteReadError:
        pass
    finally:
        UI_WRITERS.discard(writer)
        writer.close()
        print("[mock] client disconnected")


async def main() -> None:
    Path(SOCKET_PATH).unlink(missing_ok=True)
    server = await asyncio.start_unix_server(
        handle_client,
        path=SOCKET_PATH,
    )
    print(f"[mock] HyperVane mock server listening on {SOCKET_PATH}")
    print("[mock] start the UI now (./src/ui/build/hypervane-ui)")

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
