from __future__ import annotations

from dataclasses import dataclass


@dataclass
class PlatformInfo:
    id: str
    name: str
    long_name: str
    formats: list[str]
    romsfun_slug: str
    screenscraper_id: int
    thegamesdb_id: int
