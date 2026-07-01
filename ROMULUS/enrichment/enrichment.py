#!/usr/bin/env python3
"""
Enrichment worker: fetches artwork and metadata for matched library entries.

Supported sources:
  - ScreenScraper (https://www.screenscraper.fr) — primary, hash/serial lookup
  - SteamGridDB  (https://www.steamgriddb.com)  — fallback, title-based search

Usage:
    python3 enrichment.py --library user_library.db \
        --ss-user <user> --ss-pass <pass> \
        --sgdb-key <api_key> \
        --artwork-dir ./artwork
"""
from __future__ import annotations

import argparse
import sqlite3
import sys
import time
from pathlib import Path
from typing import Optional
from urllib.parse import urljoin

try:
    import requests
except ImportError:
    sys.exit("Install dependencies: pip install requests")


# ---------------------------------------------------------------------------
# ScreenScraper system ID table
# ---------------------------------------------------------------------------
SS_SYSTEM_IDS: dict[str, int] = {
    "PS1":  57,
    "PS2":  58,
    "PS3":  59,
    "PSP":  61,
    "GB":   9,
    "GBC":  10,
    "GBA":  12,
    "NES":  3,
    "SNES": 4,
    "N64":  14,
    "GC":   13,
    "WII":  16,
    "MD":   1,
    "GG":   21,
    "SMS":  2,
    "SAT":  22,
    "DC":   23,
    "X360": 12,
    "XBOX": 32,
    "2600": 26,
    "PCE":  31,
    "NEO":  142,
}

SS_BASE = "https://www.screenscraper.fr/api2/"
SGDB_BASE = "https://www.steamgriddb.com/api/v2/"

RATE_LIMIT_SLEEP = 1.0  # seconds between API requests


class ScreenScraperClient:
    def __init__(self, user: str, password: str, soft_name: str = "romulus"):
        self.session = requests.Session()
        self.common = {
            "devid": "romulus",
            "devpassword": "romulus_dev",
            "softname": soft_name,
            "ssid": user,
            "sspassword": password,
            "output": "json",
        }

    def game_info(
        self,
        system_tag: str,
        sha1: Optional[str] = None,
        crc32: Optional[str] = None,
        serial: Optional[str] = None,
        rom_name: Optional[str] = None,
    ) -> Optional[dict]:
        system_id = SS_SYSTEM_IDS.get(system_tag)
        if system_id is None:
            return None

        params = {**self.common, "systemeid": system_id, "romtype": "rom"}
        if sha1:
            params["sha1"] = sha1
        if crc32:
            params["crc"] = crc32
        if serial:
            params["serialnum"] = serial
        if rom_name:
            params["romnom"] = rom_name

        try:
            r = self.session.get(urljoin(SS_BASE, "jeuInfos.php"), params=params, timeout=15)
            r.raise_for_status()
            data = r.json()
            return data.get("response", {}).get("jeu")
        except requests.RequestException as e:
            print(f"  ScreenScraper error: {e}", file=sys.stderr)
            return None

    def find_cover_url(self, game_info: dict) -> Optional[str]:
        """Pick the best box-art URL from the medias array."""
        medias = game_info.get("medias", [])
        # Preference order: box-2D (world) → box-2D (any region) → sstitle
        for pref_region in ("wor", "us", "eu", "jp", None):
            for media in medias:
                if media.get("type") != "box-2D":
                    continue
                if pref_region is None or media.get("region") == pref_region:
                    return media.get("url")
        return None

    def find_screenshot_url(self, game_info: dict) -> Optional[str]:
        for media in game_info.get("medias", []):
            if media.get("type") in ("ss", "sstitle"):
                return media.get("url")
        return None

    def extract_metadata(self, game_info: dict) -> dict:
        def pick_text(items: list[dict], field: str = "text") -> Optional[str]:
            if not items:
                return None
            for lang in ("en", "wor"):
                for item in items:
                    if item.get("langue") == lang:
                        return item.get(field)
            return items[0].get(field)

        noms = game_info.get("noms", [])
        synopses = game_info.get("synopsis", [])
        devs = game_info.get("developpeur", {})
        pubs = game_info.get("editeur", {})
        dates = game_info.get("dates", [])

        return {
            "title": pick_text(noms),
            "description": pick_text(synopses),
            "developer": devs.get("text") if isinstance(devs, dict) else None,
            "publisher": pubs.get("text") if isinstance(pubs, dict) else None,
            "release_date": pick_text(dates, "text") if dates else None,
            "screenscraper_id": str(game_info.get("id", "")),
        }


class SteamGridDBClient:
    def __init__(self, api_key: str):
        self.session = requests.Session()
        self.session.headers["Authorization"] = f"Bearer {api_key}"

    def search_game(self, title: str) -> Optional[int]:
        try:
            r = self.session.get(
                urljoin(SGDB_BASE, "search/autocomplete/" + requests.utils.quote(title)),
                timeout=10,
            )
            r.raise_for_status()
            data = r.json()
            results = data.get("data", [])
            return results[0]["id"] if results else None
        except requests.RequestException as e:
            print(f"  SteamGridDB search error: {e}", file=sys.stderr)
            return None

    def get_cover_url(self, game_id: int) -> Optional[str]:
        try:
            r = self.session.get(
                urljoin(SGDB_BASE, f"grids/game/{game_id}"),
                params={"dimensions": "600x900"},
                timeout=10,
            )
            r.raise_for_status()
            data = r.json()
            items = data.get("data", [])
            return items[0]["url"] if items else None
        except requests.RequestException as e:
            print(f"  SteamGridDB cover error: {e}", file=sys.stderr)
            return None


def download_file(session: requests.Session, url: str, dest: Path) -> bool:
    try:
        r = session.get(url, stream=True, timeout=30)
        r.raise_for_status()
        dest.parent.mkdir(parents=True, exist_ok=True)
        with open(dest, "wb") as f:
            for chunk in r.iter_content(chunk_size=65536):
                f.write(chunk)
        return True
    except requests.RequestException as e:
        print(f"  Download error ({url}): {e}", file=sys.stderr)
        return False


def enrich_entry(
    row: dict,
    ss: Optional[ScreenScraperClient],
    sgdb: Optional[SteamGridDBClient],
    artwork_dir: Path,
    session: requests.Session,
) -> dict:
    updates: dict = {}
    game_id_str = str(row["id"])
    system_tag = row["system_tag"] or "UNKNOWN"
    title = row["title"] or row["filename"] or ""

    # --- ScreenScraper lookup ---
    if ss:
        game_info = ss.game_info(
            system_tag=system_tag,
            sha1=row.get("sha1_hash"),
            crc32=row.get("crc32_hash"),
            serial=row.get("serial_number"),
            rom_name=row.get("filename"),
        )

        if game_info:
            meta = ss.extract_metadata(game_info)
            updates.update({k: v for k, v in meta.items() if v})

            cover_url = ss.find_cover_url(game_info)
            if cover_url:
                ext = Path(cover_url.split("?")[0]).suffix or ".jpg"
                dest = artwork_dir / system_tag / f"{game_id_str}_cover{ext}"
                if download_file(session, cover_url, dest):
                    updates["cover_art_path"] = str(dest)

            bg_url = ss.find_screenshot_url(game_info)
            if bg_url:
                ext = Path(bg_url.split("?")[0]).suffix or ".jpg"
                dest = artwork_dir / system_tag / f"{game_id_str}_bg{ext}"
                if download_file(session, bg_url, dest):
                    updates["background_art_path"] = str(dest)

            time.sleep(RATE_LIMIT_SLEEP)

    # --- SteamGridDB fallback for cover art only ---
    if sgdb and not updates.get("cover_art_path") and title:
        sgdb_game_id = sgdb.search_game(title)
        if sgdb_game_id:
            cover_url = sgdb.get_cover_url(sgdb_game_id)
            if cover_url:
                ext = Path(cover_url.split("?")[0]).suffix or ".jpg"
                dest = artwork_dir / system_tag / f"{game_id_str}_cover_sgdb{ext}"
                if download_file(session, cover_url, dest):
                    updates["cover_art_path"] = str(dest)
        time.sleep(RATE_LIMIT_SLEEP)

    return updates


def run(args: argparse.Namespace) -> None:
    conn = sqlite3.connect(args.library)
    conn.row_factory = sqlite3.Row
    artwork_dir = Path(args.artwork_dir)

    ss = None
    if args.ss_user and args.ss_pass:
        ss = ScreenScraperClient(args.ss_user, args.ss_pass)
    else:
        print("No ScreenScraper credentials — skipping SS lookups", file=sys.stderr)

    sgdb = None
    if args.sgdb_key:
        sgdb = SteamGridDBClient(args.sgdb_key)
    else:
        print("No SteamGridDB API key — skipping SGDB lookups", file=sys.stderr)

    if not ss and not sgdb:
        sys.exit("No API credentials provided. Use --ss-user/--ss-pass or --sgdb-key.")

    session = requests.Session()

    # Fetch entries that are matched but still missing artwork
    rows = conn.execute(
        """SELECT id, system_tag, title, filename, sha1_hash, crc32_hash, serial_number
           FROM user_library
           WHERE is_matched = 1 AND cover_art_path IS NULL
           ORDER BY system_tag, title"""
    ).fetchall()

    print(f"Found {len(rows)} entries needing enrichment")

    enriched = 0
    for row in rows:
        row_dict = dict(row)
        label = row_dict.get("title") or row_dict.get("filename") or str(row_dict["id"])
        print(f"  [{row_dict['system_tag']}] {label}")

        updates = enrich_entry(row_dict, ss, sgdb, artwork_dir, session)

        if updates:
            set_clause = ", ".join(f"{k} = :{k}" for k in updates)
            updates["_id"] = row_dict["id"]
            conn.execute(
                f"UPDATE user_library SET {set_clause} WHERE id = :_id",
                updates,
            )
            conn.commit()
            enriched += 1
            print(f"    ✓ Updated {len(updates) - 1} fields")
        else:
            print("    — No data found")

    print(f"\nEnriched {enriched}/{len(rows)} entries")
    conn.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Fetch artwork and metadata for library entries")
    parser.add_argument("--library", default="user_library.db", help="Path to user_library.db")
    parser.add_argument("--ss-user", default="", help="ScreenScraper username")
    parser.add_argument("--ss-pass", default="", help="ScreenScraper password")
    parser.add_argument("--sgdb-key", default="", help="SteamGridDB API key")
    parser.add_argument("--artwork-dir", default="./artwork", help="Directory to save artwork files")
    args = parser.parse_args()
    run(args)


if __name__ == "__main__":
    main()
