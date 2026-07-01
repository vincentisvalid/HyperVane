#!/usr/bin/env python3
"""
DAT file downloader for Redump and No-Intro preservation groups.

Redump   — publicly downloadable ZIPs; no account required.
No-Intro — requires a datomatic.no-intro.org account (cookie-based auth).

Usage examples:
    # List all available Redump systems
    python3 download_dats.py --source redump --list

    # Download a few Redump systems
    python3 download_dats.py --source redump --systems ps ps2 psp --output ./dats/

    # Download everything from Redump
    python3 download_dats.py --source redump --all --output ./dats/

    # No-Intro (needs your login session cookie from a browser)
    python3 download_dats.py --source nointro --cookie "session=..." --systems gba gb --output ./dats/
"""
from __future__ import annotations

import argparse
import io
import json
import sys
import time
import urllib.error
import urllib.request
import zipfile
from pathlib import Path


# ---------------------------------------------------------------------------
# Redump system catalogue
# Slugs taken from http://redump.org/downloads/
# Each entry: (slug, display_name)
# ---------------------------------------------------------------------------
REDUMP_SYSTEMS: list[tuple[str, str]] = [
    ("ps",        "Sony - PlayStation"),
    ("ps2",       "Sony - PlayStation 2"),
    ("psp",       "Sony - PlayStation Portable"),
    ("ps3",       "Sony - PlayStation 3"),
    ("ps4",       "Sony - PlayStation 4"),
    ("dc",        "Sega - Dreamcast"),
    ("ss",        "Sega - Saturn"),
    ("gc",        "Nintendo - GameCube"),
    ("wii",       "Nintendo - Wii"),
    ("wiiu",      "Nintendo - Wii U"),
    ("xbox",      "Microsoft - Xbox"),
    ("xbox360",   "Microsoft - Xbox 360"),
    ("pc",        "IBM - PC compatible"),
    ("3do",       "Panasonic - 3DO Interactive Multiplayer"),
    ("jaguar",    "Atari - Jaguar"),
    ("neocd",     "SNK - Neo Geo CD"),
    ("pcfx",      "NEC - PC-FX"),
    ("mcd",       "Sega - Mega CD"),
]

REDUMP_DAT_URL = "http://redump.org/datfile/{slug}/"

# ---------------------------------------------------------------------------
# No-Intro system catalogue (datomatic export IDs)
# Each entry: (export_id, display_name)
# ---------------------------------------------------------------------------
NOINTRO_SYSTEMS: list[tuple[str, str]] = [
    ("nintendo_gb",      "Nintendo - Game Boy"),
    ("nintendo_gbc",     "Nintendo - Game Boy Color"),
    ("nintendo_gba",     "Nintendo - Game Boy Advance"),
    ("nintendo_nes",     "Nintendo - Nintendo Entertainment System"),
    ("nintendo_snes",    "Nintendo - Super Nintendo Entertainment System"),
    ("nintendo_n64",     "Nintendo - Nintendo 64"),
    ("nintendo_gc",      "Nintendo - GameCube"),
    ("nintendo_ds",      "Nintendo - Nintendo DS"),
    ("nintendo_3ds",     "Nintendo - Nintendo 3DS"),
    ("sega_md",          "Sega - Mega Drive - Genesis"),
    ("sega_gg",          "Sega - Game Gear"),
    ("sega_sms",         "Sega - Master System - Mark III"),
    ("atari_2600",       "Atari - 2600"),
    ("atari_7800",       "Atari - 7800"),
    ("snk_ngp",          "SNK - Neo Geo Pocket"),
]

NOINTRO_DAT_URL = "https://datomatic.no-intro.org/index.php?page=export&s={id}&v=dat&type=standard"


HEADERS = {
    "User-Agent": (
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
    ),
    "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
}


def http_get(url: str, extra_headers: dict | None = None) -> bytes:
    headers = {**HEADERS, **(extra_headers or {})}
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def download_redump_system(slug: str, name: str, output_dir: Path, dry_run: bool = False) -> bool:
    url = REDUMP_DAT_URL.format(slug=slug)
    print(f"  [{slug}] {name}")
    print(f"    → {url}")

    if dry_run:
        return True

    try:
        data = http_get(url)
    except urllib.error.HTTPError as e:
        print(f"    HTTP {e.code}: {e.reason}", file=sys.stderr)
        return False
    except urllib.error.URLError as e:
        print(f"    Network error: {e.reason}", file=sys.stderr)
        return False

    # Redump serves a ZIP; extract the contained DAT file
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as zf:
            dat_files = [n for n in zf.namelist() if n.lower().endswith((".dat", ".xml"))]
            if not dat_files:
                print(f"    No DAT file found in ZIP (files: {zf.namelist()})", file=sys.stderr)
                return False

            for dat_name in dat_files:
                dest = output_dir / dat_name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(zf.read(dat_name))
                print(f"    Extracted: {dest}")
        return True

    except zipfile.BadZipFile:
        # Some systems serve the DAT directly (not zipped)
        if data.lstrip()[:5] in (b"<?xml", b"<data"):
            dest = output_dir / f"Redump - {name}.dat"
            dest.write_bytes(data)
            print(f"    Saved (raw DAT): {dest}")
            return True

        print("    Response is not a ZIP or XML DAT file", file=sys.stderr)
        # Save raw response for inspection
        debug_path = output_dir / f"_debug_{slug}.bin"
        debug_path.write_bytes(data[:4096])
        print(f"    First 4 KB saved to {debug_path} for inspection", file=sys.stderr)
        return False


def download_nointro_system(
    system_id: str, name: str, output_dir: Path, cookie: str, dry_run: bool = False
) -> bool:
    url = NOINTRO_DAT_URL.format(id=system_id)
    print(f"  [{system_id}] {name}")
    print(f"    → {url}")

    if dry_run:
        return True

    try:
        data = http_get(url, extra_headers={"Cookie": cookie})
    except urllib.error.HTTPError as e:
        if e.code == 403:
            print("    403 Forbidden — check your --cookie value", file=sys.stderr)
        else:
            print(f"    HTTP {e.code}: {e.reason}", file=sys.stderr)
        return False
    except urllib.error.URLError as e:
        print(f"    Network error: {e.reason}", file=sys.stderr)
        return False

    try:
        with zipfile.ZipFile(io.BytesIO(data)) as zf:
            dat_files = [n for n in zf.namelist() if n.lower().endswith((".dat", ".xml"))]
            for dat_name in dat_files:
                dest = output_dir / dat_name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(zf.read(dat_name))
                print(f"    Extracted: {dest}")
        return bool(dat_files)
    except zipfile.BadZipFile:
        if b"<html" in data[:200].lower():
            print("    Got an HTML page — cookie may be expired or invalid", file=sys.stderr)
        else:
            print("    Response is not a ZIP file", file=sys.stderr)
        return False


def cmd_list(source: str) -> None:
    systems = REDUMP_SYSTEMS if source == "redump" else NOINTRO_SYSTEMS
    print(f"\nAvailable {source} systems ({len(systems)}):\n")
    col = max(len(s) for s, _ in systems) + 2
    for slug, name in systems:
        print(f"  {slug:<{col}} {name}")
    print()


def cmd_download(args: argparse.Namespace) -> None:
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    if args.source == "redump":
        systems_map = dict(REDUMP_SYSTEMS)
        if args.all:
            selected = list(systems_map.items())
        else:
            selected = []
            for slug in (args.systems or []):
                if slug not in systems_map:
                    print(f"Unknown system '{slug}'. Run --list to see options.", file=sys.stderr)
                    sys.exit(1)
                selected.append((slug, systems_map[slug]))

        if not selected:
            sys.exit("No systems selected. Use --all or --systems <slug...>.")

        ok = 0
        for i, (slug, name) in enumerate(selected):
            if i > 0:
                time.sleep(1)  # be polite
            if download_redump_system(slug, name, output_dir, dry_run=args.dry_run):
                ok += 1

    elif args.source == "nointro":
        if not args.cookie:
            sys.exit(
                "No-Intro requires a --cookie value.\n"
                "Get it from your browser's DevTools after logging into datomatic.no-intro.org:\n"
                "  Network tab → any request → copy the 'Cookie' request header value."
            )
        systems_map = dict(NOINTRO_SYSTEMS)
        if args.all:
            selected = list(systems_map.items())
        else:
            selected = []
            for sid in (args.systems or []):
                if sid not in systems_map:
                    print(f"Unknown system '{sid}'. Run --list to see options.", file=sys.stderr)
                    sys.exit(1)
                selected.append((sid, systems_map[sid]))

        if not selected:
            sys.exit("No systems selected. Use --all or --systems <id...>.")

        ok = 0
        for i, (sid, name) in enumerate(selected):
            if i > 0:
                time.sleep(2)
            if download_nointro_system(sid, name, output_dir, args.cookie, dry_run=args.dry_run):
                ok += 1

    print(f"\n{ok}/{len(selected)} systems downloaded to {output_dir}")
    if ok > 0 and not args.dry_run:
        print(f"\nNext step:")
        print(f"  python3 build_catalog.py {output_dir} --db reference_catalog.db")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Download No-Intro / Redump DAT files",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--source",
        choices=["redump", "nointro"],
        default="redump",
        help="Which preservation group to download from (default: redump)",
    )
    parser.add_argument("--list", action="store_true", help="List available systems and exit")
    parser.add_argument(
        "--systems",
        nargs="+",
        metavar="SLUG",
        help="System slugs to download (e.g. ps ps2 psp)",
    )
    parser.add_argument("--all", action="store_true", help="Download all available systems")
    parser.add_argument(
        "--output",
        default="./dats",
        metavar="DIR",
        help="Directory to save extracted DAT files (default: ./dats)",
    )
    parser.add_argument(
        "--cookie",
        default="",
        metavar="HEADER",
        help="Cookie header value for No-Intro authentication",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be downloaded without actually downloading",
    )

    args = parser.parse_args()

    if args.list:
        cmd_list(args.source)
        return

    if not args.systems and not args.all:
        parser.print_help()
        print("\nError: specify --systems or --all", file=sys.stderr)
        sys.exit(1)

    cmd_download(args)


if __name__ == "__main__":
    main()
