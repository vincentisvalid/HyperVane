#!/usr/bin/env python3
"""Mock HTTP ROM server using standard ROM IDs (SYSTEM-REGION-TITLE-XXXXX).

Run alongside mock_server.py.

Usage:
  python3 scripts/mock_rom_server.py [--port 8080]
"""

from __future__ import annotations

import argparse
import hashlib
import http.server
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "src" / "scraper"))

try:
    from scraper.rom_id import RomIDGenerator, RomHasher
except ImportError:
    # Fallback for minimal environments
    import re as _re
    class RomIDGenerator:
        @staticmethod
        def from_metadata(platform, region, title, sha1_hash="", counter=0):
            s = platform.upper().strip()
            r = region.upper().strip()[:4]
            t = _re.sub(r"[^A-Z0-9]", "", title.upper())
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
    ("Pokemon FireRed",                            "GBA",      "USA"),
    ("Metroid Fusion",                             "GBA",      "USA"),
    ("Castlevania: Aria of Sorrow",                "GBA",      "USA"),
    ("Shenmue",                                    "Dreamcast","JPN"),
    ("Jet Set Radio",                              "Dreamcast","USA"),
    ("Grand Theft Auto: Vice City",                "PS2",      "USA"),
    ("Shadow of the Colossus",                     "PS2",      "USA"),
    ("The Last of Us",                             "PS3",      "USA"),
    ("God of War",                                 "PS4",      "USA"),
    ("Super Mario Galaxy",                         "Wii",      "USA"),
    ("Super Smash Bros. Melee",                    "GameCube", "USA"),
    ("Pokemon Sun",                                "3DS",      "USA"),
    ("New Super Mario Bros.",                      "NDS",      "USA"),
]

# Build ROM entries with standard IDs
FAKE_ROMS: list[dict] = []
for title, platform, region in _RAW_ROMS:
    fake_sha1 = hashlib.sha1(title.encode()).hexdigest()
    rom_id = RomIDGenerator.from_metadata(platform, region, title, fake_sha1)
    FAKE_ROMS.append({
        "id": rom_id,
        "title": title,
        "platform": platform,
        "region": region,
        "verified": True,
        "crc32": fake_sha1[:8],
        "sha1": fake_sha1,
        "file_size": 4 * 1024 * 1024,
    })

_ROM_BY_ID = {r["id"]: r for r in FAKE_ROMS}

# Generate fake binary data for downloads (deterministic)
_ROM_DATA_CACHE: dict[str, bytes] = {}


def _fake_rom_data(rom_id: str) -> bytes:
    if rom_id not in _ROM_DATA_CACHE:
        n = int(hashlib.sha1(rom_id.encode()).hexdigest(), 16) % 3 + 1
        size = n * 1024 * 1024
        seed = hashlib.sha256(rom_id.encode()).digest()
        data = bytearray(size)
        for i in range(0, size, len(seed)):
            data[i:i+len(seed)] = seed[:min(len(seed), size-i)]
        _ROM_DATA_CACHE[rom_id] = bytes(data)
    return _ROM_DATA_CACHE[rom_id]


class ROMHTTPHandler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = self.path.split("?")
        path = parsed[0].rstrip("/")
        params = {}
        if len(parsed) > 1:
            for kv in parsed[1].split("&"):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    params[k] = v

        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

        if path == "/api/roms/search":
            self._handle_search(params)
        elif path.startswith("/api/roms/download/"):
            parts = path.split("/")
            if len(parts) >= 6:
                platform = parts[4]
                rom_id = parts[5]
                self._handle_download(platform, rom_id)
            else:
                self._send_json({"error": "invalid download path"}, 400)
        else:
            self._send_json({"error": "not found"}, 404)

    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def _handle_search(self, params):
        query = params.get("q", "").lower()
        platform = params.get("platform", "").lower()

        results = []
        for rom in FAKE_ROMS:
            if query and query not in rom["title"].lower():
                continue
            if platform and platform not in rom["platform"].lower():
                continue
            r = dict(rom)
            r["file_path"] = f"/roms/{rom['platform'].lower().replace(' ', '_')}/{rom['title']}.rom"
            results.append(r)

        self._send_json({"total_hits": len(results), "results": results})

    def _handle_download(self, platform, rom_id):
        if rom_id not in _ROM_BY_ID:
            self._send_json({"error": "ROM not found"}, 404)
            return

        rom = _ROM_BY_ID[rom_id]
        data = _fake_rom_data(rom_id)

        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Disposition",
                         f'attachment; filename="{rom["title"]}.rom"')
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def _send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode("utf-8"))

    def log_message(self, fmt, *args):
        print(f"[mock-rom] {args[0]} {args[1]} {args[2]}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8080)
    args = ap.parse_args()

    server = http.server.HTTPServer(("127.0.0.1", args.port), ROMHTTPHandler)
    print(f"[mock-rom] HTTP ROM server listening on http://127.0.0.1:{args.port}")
    print("[mock-rom] endpoints:")
    print("  GET /api/roms/search?q=<query>&platform=<platform>")
    print("  GET /api/roms/download/<platform>/<id>")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[mock-rom] shutting down")
        server.server_close()


if __name__ == "__main__":
    main()
