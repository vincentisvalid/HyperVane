from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class GameResult:
    title: str
    platform: str
    detail_url: str
    source: str
    score: float = 1.0
    size_hint: str | None = None
    catalog_id: str | None = None


@dataclass
class GameMetadata:
    title: str
    platform: str
    source: str
    description: str | None = None
    cover_url: str | None = None
    publisher: str | None = None
    developer: str | None = None
    release_date: str | None = None
    region: str | None = None
    genres: list[str] = field(default_factory=list)
    rating: float | None = None
    download_count: int | None = None
    size_hint: str | None = None
    screenshot_urls: list[str] = field(default_factory=list)
    download_page_url: str | None = None
    detail_url: str | None = None


class DownloadSource:
    source_name: str = "base"

    async def search(self, title: str, platform: str) -> list[GameResult]: ...

    async def list_platform(self, platform: str, amount: int | None = None) -> list[GameResult]: ...

    async def fetch_metadata(self, result: GameResult) -> GameMetadata: ...

    async def download_rom(self, result: GameResult, dest: Path) -> Path: ...

    async def download_art(
        self, metadata: GameMetadata, art_dir: Path, max_screenshots: int = 3
    ) -> dict[str, str]: ...
