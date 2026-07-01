from __future__ import annotations

import json
import shutil
from datetime import datetime, timezone
from pathlib import Path


def _safe_name(name: str) -> str:
    for ch in r'\/:*?"<>|':
        name = name.replace(ch, "_")
    return name.strip()


def title_dir(output_root: Path, platform_long_name: str, title: str) -> Path:
    d = output_root / _safe_name(platform_long_name) / _safe_name(title)
    d.mkdir(parents=True, exist_ok=True)
    return d


def art_dir(title_path: Path) -> Path:
    d = title_path / "art"
    d.mkdir(parents=True, exist_ok=True)
    return d


def place_rom(src: Path, title_path: Path) -> Path:
    title_path.mkdir(parents=True, exist_ok=True)
    dest = title_path / src.name
    if dest != src:
        shutil.move(str(src), dest)
    return dest


def write_metadata(
    title_path: Path,
    title: str,
    platform: str,
    region: str,
    rom_files: list[dict],
    art: dict[str, str],
    description: str | None = None,
    developer: str | None = None,
    publisher: str | None = None,
    release_date: str | None = None,
    genre: list[str] | None = None,
    extra: dict | None = None,
) -> Path:
    meta: dict = {
        "title": title,
        "platform": platform,
        "region": region,
        "description": description,
        "release_date": release_date,
        "developer": developer,
        "publisher": publisher,
        "genre": genre or [],
        "players": None,
        "rom_files": rom_files,
        "art": art,
        "scraped_at": datetime.now(timezone.utc).isoformat(),
    }
    if extra:
        meta.update(extra)
    path = title_path / "metadata.json"
    path.write_text(json.dumps(meta, indent=2, ensure_ascii=False))
    return path
