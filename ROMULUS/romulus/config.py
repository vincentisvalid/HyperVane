from __future__ import annotations

import html
import re
from pathlib import Path

import yaml

from . import GAMELIST_FILES, PLATFORM_NAMES


def load_config(path: Path | None = None) -> dict:
    if path is None:
        path = Path(__file__).resolve().parent.parent / "config.yaml"
    if not path.exists():
        return {}
    with path.open() as f:
        return yaml.safe_load(f) or {}


def resolve_path(value: str, base: Path) -> Path:
    p = Path(value).expanduser()
    if not p.is_absolute():
        p = (base / p).resolve()
    return p


def config_paths(cfg: dict, root: Path) -> dict:
    return {
        "output_dir": resolve_path(cfg.get("output_dir", "../manual_roms"), root),
        "catalog_db": resolve_path(cfg.get("catalog_db", "romulus_catalog.db"), root),
        "gamelists_dir": resolve_path(
            cfg.get("gamelists_dir", "../scraper/gamelists"), root
        ),
        "user_data_dir": resolve_path(
            cfg.get("browser", {}).get("user_data_dir", "~/.cache/hypervane/browser"),
            root,
        ),
    }


def decode_title(title: str) -> str:
    return html.unescape(title).strip()


def slugify(title: str) -> str:
    text = decode_title(title).lower()
    text = re.sub(r"[^\w\s-]", "", text)
    text = re.sub(r"[\s_]+", "-", text).strip("-")
    return text


def platform_label(platform: str) -> str:
    return PLATFORM_NAMES.get(platform.lower(), platform.upper())


def gamelist_path(gamelists_dir: Path, platform: str) -> Path:
    name = GAMELIST_FILES[platform.lower()]
    return gamelists_dir / name
