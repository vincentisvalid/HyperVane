from __future__ import annotations

import binascii
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path


def safe_name(name: str) -> str:
    for ch in r'\/:*?"<>|':
        name = name.replace(ch, "_")
    return name.strip()


def title_dir(output_root: Path, platform: str, title: str) -> Path:
    d = output_root / platform.lower() / safe_name(title)
    d.mkdir(parents=True, exist_ok=True)
    return d


def art_dir(title_path: Path) -> Path:
    d = title_path / "art"
    d.mkdir(parents=True, exist_ok=True)
    return d


def place_rom(src: Path, title_path: Path) -> Path:
    dest = title_path / src.name
    if dest != src:
        shutil.move(str(src), dest)
    return dest


def compute_hashes(path: Path) -> dict[str, str]:
    crc = 0
    h_md5 = hashlib.md5()
    h_sha1 = hashlib.sha1()
    with path.open("rb") as f:
        while chunk := f.read(1 << 20):
            crc = binascii.crc32(chunk, crc)
            h_md5.update(chunk)
            h_sha1.update(chunk)
    return {
        "crc32": format(crc & 0xFFFFFFFF, "08x"),
        "md5": h_md5.hexdigest(),
        "sha1": h_sha1.hexdigest(),
    }


def write_metadata(title_path: Path, payload: dict) -> Path:
    payload.setdefault("scraped_at", datetime.now(timezone.utc).isoformat())
    path = title_path / "metadata.json"
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False))
    return path
