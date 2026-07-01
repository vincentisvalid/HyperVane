#!/usr/bin/env python3
"""
Catalog builder: parses No-Intro / Redump DAT XML files into reference_catalog.db.

Usage:
    python3 build_catalog.py /path/to/dat/files/
    python3 build_catalog.py /path/to/dat/files/ --db /data/reference_catalog.db
    python3 build_catalog.py /path/to/dat/files/ --clear   # wipe and reimport
"""
import argparse
import re
import sqlite3
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

SCHEMA = """
CREATE TABLE IF NOT EXISTS reference_titles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    system_tag TEXT,
    serial_number TEXT,
    title TEXT,
    region TEXT,
    sha1_hash TEXT,
    crc32_hash TEXT
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_sha1
    ON reference_titles(sha1_hash) WHERE sha1_hash IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_hashes
    ON reference_titles(sha1_hash, crc32_hash);
CREATE INDEX IF NOT EXISTS idx_serial
    ON reference_titles(serial_number) WHERE serial_number IS NOT NULL;
"""

# Substring-match against the DAT header <name> field (lowercased)
SYSTEM_TAG_MAP: list[tuple[str, str]] = [
    ("game boy advance",                "GBA"),
    ("game boy color",                  "GBC"),
    ("game boy",                        "GB"),
    ("nintendo entertainment system",   "NES"),
    ("super nintendo",                  "SNES"),
    ("nintendo 64",                     "N64"),
    ("nintendo - gamecube",             "GC"),
    ("nintendo - wii",                  "WII"),
    ("mega drive",                      "MD"),
    ("genesis",                         "MD"),
    ("game gear",                       "GG"),
    ("master system",                   "SMS"),
    ("saturn",                          "SAT"),
    ("dreamcast",                       "DC"),
    ("playstation 3",                   "PS3"),
    ("playstation 2",                   "PS2"),
    ("playstation portable",            "PSP"),
    ("playstation",                     "PS1"),
    ("xbox 360",                        "X360"),
    ("xbox",                            "XBOX"),
    ("atari 2600",                      "2600"),
    ("atari 7800",                      "7800"),
    ("neo geo",                         "NEO"),
    ("turbografx",                      "PCE"),
    ("pc engine",                       "PCE"),
]

# Pattern for disc serial numbers embedded in game names: e.g. SLUS-00157, BLES01330
SERIAL_RE = re.compile(r'\b([A-Z]{4})[_-]?(\d{5})\b')


def infer_system_tag(header_name: str) -> str:
    lower = header_name.lower()
    for keyword, tag in SYSTEM_TAG_MAP:
        if keyword in lower:
            return tag
    # Generic fallback: last segment after " - ", uppercased, max 16 chars
    parts = header_name.split(" - ")
    return parts[-1].strip().upper().replace(" ", "_")[:16]


def extract_serial(name: str) -> str | None:
    m = SERIAL_RE.search(name.upper())
    if m:
        return f"{m.group(1)}-{m.group(2)}"
    return None


def init_db(db_path: str) -> sqlite3.Connection:
    conn = sqlite3.connect(db_path)
    conn.executescript(SCHEMA)
    conn.commit()
    return conn


def process_dat(conn: sqlite3.Connection, dat_path: Path) -> int:
    try:
        tree = ET.parse(dat_path)
    except ET.ParseError as e:
        print(f"  XML parse error in {dat_path.name}: {e}", file=sys.stderr)
        return 0

    root = tree.getroot()
    header = root.find("header")
    if header is None:
        print(f"  No <header> in {dat_path.name}, skipping", file=sys.stderr)
        return 0

    name_elem = header.find("name")
    system_name = name_elem.text.strip() if name_elem is not None and name_elem.text else "Unknown"
    system_tag = infer_system_tag(system_name)
    print(f"  [{system_tag}] {system_name}")

    rows: list[tuple] = []
    for game in root.findall("game"):
        title = game.get("name", "").strip()

        region = ""
        release = game.find("release")
        if release is not None:
            region = release.get("region", "")

        serial = extract_serial(title)

        for rom in game.findall("rom"):
            sha1 = (rom.get("sha1") or "").lower().strip() or None
            crc32 = (rom.get("crc") or "").lower().strip() or None

            # ROM-level serial override (some DATs embed it in the rom name)
            rom_name = rom.get("name", "")
            rom_serial = extract_serial(rom_name) or serial

            rows.append((system_tag, rom_serial, title, region, sha1, crc32))

    if rows:
        conn.executemany(
            """INSERT OR IGNORE INTO reference_titles
               (system_tag, serial_number, title, region, sha1_hash, crc32_hash)
               VALUES (?, ?, ?, ?, ?, ?)""",
            rows,
        )
        conn.commit()

    return len(rows)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Build reference_catalog.db from No-Intro/Redump DAT files"
    )
    parser.add_argument("dat_dir", help="Directory containing .dat or .xml files")
    parser.add_argument(
        "--db", default="reference_catalog.db", help="Output SQLite path (default: ./reference_catalog.db)"
    )
    parser.add_argument(
        "--clear", action="store_true", help="Delete all existing rows before importing"
    )
    args = parser.parse_args()

    dat_dir = Path(args.dat_dir)
    if not dat_dir.is_dir():
        sys.exit(f"Error: '{dat_dir}' is not a directory")

    conn = init_db(args.db)

    if args.clear:
        conn.execute("DELETE FROM reference_titles")
        conn.commit()
        print("Cleared reference_titles table")

    dat_files = sorted(dat_dir.glob("*.dat")) + sorted(dat_dir.glob("*.xml"))
    if not dat_files:
        sys.exit(f"No .dat or .xml files found in {dat_dir}")

    total_rows = 0
    for f in dat_files:
        print(f"Processing {f.name} …")
        n = process_dat(conn, f)
        print(f"  → {n} rows")
        total_rows += n

    row_count = conn.execute("SELECT COUNT(*) FROM reference_titles").fetchone()[0]
    print(f"\nDone — {total_rows} rows imported ({row_count} total in DB)")
    print(f"Database: {args.db}")
    conn.close()


if __name__ == "__main__":
    main()
