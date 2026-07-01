from __future__ import annotations

import asyncio
import re
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
from ..config import slugify
from .base import GameMetadata, GameResult
from .page_parser import extract_file_url, parse_pspkg_detail, parse_pspkg_links

BASE_URL = "https://pspkg.com"
PKG_EXTENSIONS = {".pkg", ".zip", ".rar", ".7z"}


class PspkgSource:
    source_name = "pspkg"

    def __init__(self, browser: BrowserSession, delay_ms: int = 1500, max_retries: int = 3):
        self._browser = browser
        self._delay = delay_ms / 1000
        self._max_retries = max_retries

    async def search(self, title: str, platform: str) -> list[GameResult]:
        slug = platform.lower()
        results: list[GameResult] = []
        seen: set[str] = set()

        for url in (
            f"{BASE_URL}/{slug}/?search={quote_plus(title)}",
            f"{BASE_URL}/{slug}/?q={quote_plus(title)}",
            f"{BASE_URL}/search?q={quote_plus(title)}",
        ):
            try:
                html = await self._browser.get_html(url, wait_selector="a[href]")
            except Exception:
                continue
            for card_title, href in parse_pspkg_links(html, slug, BASE_URL):
                if slug not in href or href in seen:
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

        if not results:
            guess = await self._guess_detail_url(title, platform)
            if guess:
                results.append(guess)

        results.sort(key=lambda r: r.score, reverse=True)
        return results

    async def list_platform(self, platform: str, amount: int | None = None) -> list[GameResult]:
        slug = platform.lower()
        collected: list[GameResult] = []
        page = 1

        while True:
            if amount and len(collected) >= amount:
                break
            url = f"{BASE_URL}/{slug}/" if page == 1 else f"{BASE_URL}/{slug}/?page={page}"
            html = await self._browser.get_html(url, wait_selector="a[href]")
            pairs = parse_pspkg_links(html, slug, BASE_URL)
            if not pairs:
                break
            for card_title, href in pairs:
                collected.append(
                    GameResult(
                        title=card_title,
                        platform=platform,
                        detail_url=href,
                        source=self.source_name,
                    )
                )
                if amount and len(collected) >= amount:
                    break
            if amount and len(collected) >= amount:
                break
            if f"page={page + 1}" not in html and f"/{page + 1}" not in html:
                break
            page += 1
            await asyncio.sleep(self._delay)

        return collected[:amount] if amount else collected

    async def fetch_metadata(self, result: GameResult) -> GameMetadata:
        html = await self._browser.get_html(result.detail_url, wait_selector="h1")
        page = parse_pspkg_detail(html, result.detail_url)
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
            size_hint=page.size_hint or result.size_hint,
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
        html = await self._browser.get_html(meta.download_page_url or result.detail_url)
        file_url = extract_file_url(html, result.detail_url, PKG_EXTENSIONS)

        if not file_url:
            page = parse_pspkg_detail(html, result.detail_url)
            for candidate in page.file_urls:
                if candidate.endswith(".pkg") or "/download" in candidate.lower():
                    probe = await self._browser.get_html(candidate)
                    file_url = extract_file_url(probe, candidate, PKG_EXTENSIONS)
                    if file_url:
                        break

        if not file_url:
            raise RuntimeError(f"Could not resolve PKG download for {result.title}")

        filename = Path(file_url.split("?")[0]).name
        if not filename.lower().endswith(".pkg"):
            filename = f"{slugify(result.title)}.pkg"
        out_path = dest / filename

        with Progress(
            SpinnerColumn(),
            TextColumn("[bold blue]{task.description}"),
            BarColumn(),
            DownloadColumn(),
            TransferSpeedColumn(),
            TimeRemainingColumn(),
        ) as progress:
            task = progress.add_task(f"[magenta]{result.title}", total=None)

            def on_progress(done: int, total: int) -> None:
                progress.update(task, completed=done, total=total or None)

            await self._browser.stream_to_file(
                file_url,
                out_path,
                referer=result.detail_url,
                on_progress=on_progress,
            )
        return out_path

    async def _guess_detail_url(self, title: str, platform: str) -> GameResult | None:
        slug = slugify(title)
        patterns = [
            f"{BASE_URL}/{platform}/{slug}.html",
            f"{BASE_URL}/{platform}/{slug}-1.html",
        ]
        for url in patterns:
            try:
                html = await self._browser.get_html(url)
                if "404" in html.lower() and "not found" in html.lower():
                    continue
                page = parse_pspkg_detail(html, url)
                if page.title and page.title.lower() != "unknown":
                    return GameResult(
                        title=page.title,
                        platform=platform,
                        detail_url=url,
                        source=self.source_name,
                        score=0.8,
                    )
            except Exception:
                continue
        return None
