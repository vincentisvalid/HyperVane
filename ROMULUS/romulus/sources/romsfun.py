from __future__ import annotations

import asyncio
import tempfile
from pathlib import Path
from urllib.parse import quote_plus, urljoin

from rapidfuzz import fuzz
from rich.progress import (
    BarColumn,
    DownloadColumn,
    Progress,
    SpinnerColumn,
    TextColumn,
    TimeRemainingColumn,
    TransferSpeedColumn,
)

from ..browser import BrowserSession
from .base import GameMetadata, GameResult
from .page_parser import (
    extract_file_url,
    parse_romsfun_detail,
    parse_romsfun_links,
    parse_romsfun_search,
)

BASE_URL = "https://romsfun.com"
ROM_EXTENSIONS = {".iso", ".pkg", ".bin", ".cue", ".zip", ".7z", ".chd", ".rar"}

PLATFORM_SLUGS = {
    "ps1": "playstation",
    "ps2": "playstation-2",
    "ps3": "playstation-3",
    "psp": "psp",
}


class RomsFunSource:
    source_name = "romsfun"

    def __init__(self, browser: BrowserSession, delay_ms: int = 1500, max_retries: int = 3):
        self._browser = browser
        self._delay = delay_ms / 1000
        self._max_retries = max_retries

    async def search(self, title: str, platform: str) -> list[GameResult]:
        slug = self._slug(platform)
        results: list[GameResult] = []
        seen: set[str] = set()

        for url in (
            f"{BASE_URL}/?s={quote_plus(title)}",
            f"{BASE_URL}/search/?q={quote_plus(title)}",
        ):
            html = await self._browser.get_html(url)
            for card_title, href in parse_romsfun_search(html, slug, BASE_URL):
                if href in seen:
                    continue
                seen.add(href)
                score = fuzz.token_sort_ratio(title.lower(), card_title.lower()) / 100
                results.append(
                    GameResult(
                        title=card_title,
                        platform=platform,
                        detail_url=href,
                        source=self.source_name,
                        score=score,
                    )
                )

        results.sort(key=lambda r: r.score, reverse=True)
        return results

    async def list_platform(self, platform: str, amount: int | None = None) -> list[GameResult]:
        slug = self._slug(platform)
        collected: list[GameResult] = []
        page = 1

        while True:
            if amount and len(collected) >= amount:
                break
            url = (
                f"{BASE_URL}/roms/{slug}/"
                if page == 1
                else f"{BASE_URL}/roms/{slug}/page/{page}/"
            )
            html = await self._browser.get_html(url)
            links = parse_romsfun_links(html, slug, BASE_URL)
            if not links:
                break
            for href in links:
                title = Path(href).stem.replace("-", " ").title()
                collected.append(
                    GameResult(
                        title=title,
                        platform=platform,
                        detail_url=href,
                        source=self.source_name,
                    )
                )
                if amount and len(collected) >= amount:
                    break
            if amount and len(collected) >= amount:
                break
            if f"/page/{page + 1}/" not in html:
                break
            page += 1
            await asyncio.sleep(self._delay)

        return collected[:amount] if amount else collected

    async def fetch_metadata(self, result: GameResult) -> GameMetadata:
        html = await self._browser.get_html(result.detail_url)
        page = parse_romsfun_detail(html, result.detail_url)
        return GameMetadata(
            title=page.title,
            platform=result.platform,
            source=self.source_name,
            description=page.description,
            cover_url=page.cover_url,
            publisher=page.publisher,
            developer=page.developer,
            release_date=page.release_date,
            region=page.region,
            genres=page.genres,
            rating=page.rating,
            download_count=page.download_count,
            size_hint=page.size_hint,
            screenshot_urls=page.screenshot_urls,
            download_page_url=page.download_page_url,
            detail_url=result.detail_url,
        )

    async def download_art(
        self, metadata: GameMetadata, art_dir: Path, max_screenshots: int = 3
    ) -> dict[str, str]:
        art_dir.mkdir(parents=True, exist_ok=True)
        saved: dict[str, str] = {}
        if metadata.cover_url:
            cover = art_dir / "cover.png"
            data = await self._browser.download_bytes(
                metadata.cover_url, referer=metadata.detail_url
            )
            cover.write_bytes(data)
            saved["cover"] = f"art/{cover.name}"
            saved["box_front"] = saved["cover"]
        for i, url in enumerate(metadata.screenshot_urls[:max_screenshots], 1):
            path = art_dir / f"screenshot_{i:02d}.png"
            try:
                data = await self._browser.download_bytes(url, referer=metadata.detail_url)
                path.write_bytes(data)
                saved[f"screenshot_{i:02d}"] = f"art/{path.name}"
            except Exception:
                continue
        return saved

    async def download_rom(self, result: GameResult, dest: Path) -> Path:
        dest.mkdir(parents=True, exist_ok=True)
        for attempt in range(1, self._max_retries + 1):
            try:
                return await self._download(result, dest)
            except Exception:
                if attempt == self._max_retries:
                    raise
                await asyncio.sleep(self._delay * attempt)
        raise RuntimeError("unreachable")

    async def _download(self, result: GameResult, dest: Path) -> Path:
        meta = await self.fetch_metadata(result)
        if not meta.download_page_url:
            raise RuntimeError(f"No download page for {result.detail_url}")

        dl_html = await self._browser.get_html(meta.download_page_url)
        file_url = extract_file_url(dl_html, meta.download_page_url, ROM_EXTENSIONS)
        if not file_url:
            for mirror in _mirror_links(dl_html, meta.download_page_url):
                await asyncio.sleep(self._delay)
                mirror_html = await self._browser.get_html(mirror)
                file_url = extract_file_url(mirror_html, mirror, ROM_EXTENSIONS)
                if file_url:
                    break

        if not file_url:
            raise RuntimeError(f"Could not resolve ROM file URL for {result.title}")

        filename = Path(file_url.split("?")[0]).name or f"{result.title}.bin"
        out_path = dest / filename

        with Progress(
            SpinnerColumn(),
            TextColumn("[bold blue]{task.description}"),
            BarColumn(),
            DownloadColumn(),
            TransferSpeedColumn(),
            TimeRemainingColumn(),
        ) as progress:
            task = progress.add_task(f"[cyan]{result.title}", total=None)

            def on_progress(done: int, total: int) -> None:
                progress.update(task, completed=done, total=total or None)

            await self._browser.stream_to_file(
                file_url,
                out_path,
                referer=meta.download_page_url,
                on_progress=on_progress,
            )
        return out_path

    def _slug(self, platform: str) -> str:
        slug = PLATFORM_SLUGS.get(platform.lower())
        if not slug:
            raise ValueError(f"romsfun does not support platform: {platform}")
        return slug


def _mirror_links(html: str, page_url: str) -> list[str]:
    from bs4 import BeautifulSoup

    soup = BeautifulSoup(html, "html.parser")
    mirrors: list[str] = []
    for a in soup.select("a[href*='/download/']"):
        href = urljoin(page_url, a.get("href", ""))
        if href != page_url and href not in mirrors:
            mirrors.append(href)
    return mirrors
