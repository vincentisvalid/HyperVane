from __future__ import annotations

import asyncio
import re
from pathlib import Path
from urllib.parse import quote_plus, urljoin

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
from ..matchers import title as title_matcher
from .base import ROMResult
from .page_parser import PageMetadata, parse_detail_page, parse_listing_links, parse_search_results

ROM_EXTENSIONS = {".iso", ".pkg", ".bin", ".cue", ".zip", ".7z", ".chd", ".rar"}

BASE_URL = "https://romsfun.com"

PLATFORM_SLUGS: dict[str, str] = {
    "ps1": "playstation",
    "ps2": "playstation-2",
    "ps3": "playstation-3",
    "psp": "psp",
}


class RomsFunSource:
    def __init__(
        self,
        user_data_dir: Path = Path("~/.cache/hypervane/browser"),
        headless: bool = True,
        request_delay_ms: int = 1500,
        max_retries: int = 3,
    ) -> None:
        self._user_data_dir = user_data_dir
        self._headless = headless
        self._delay = request_delay_ms / 1000
        self._max_retries = max_retries
        self._browser: BrowserSession | None = None

    async def __aenter__(self) -> "RomsFunSource":
        self._browser = BrowserSession(
            user_data_dir=self._user_data_dir,
            headless=self._headless,
            request_delay_ms=int(self._delay * 1000),
        )
        await self._browser.__aenter__()
        return self

    async def __aexit__(self, *_) -> None:
        if self._browser:
            await self._browser.__aexit__(None, None, None)

    # ----------------------------------------------------------------- listing

    async def list_platform(
        self, platform: str, amount: int | None = None
    ) -> list[str]:
        slug = self._slug(platform)
        collected: list[str] = []
        page_num = 1

        while True:
            if amount and len(collected) >= amount:
                break

            url = (
                f"{BASE_URL}/roms/{slug}/"
                if page_num == 1
                else f"{BASE_URL}/roms/{slug}/page/{page_num}/"
            )
            html = await self._get_html(url)
            links = parse_listing_links(html, slug, BASE_URL)
            if not links:
                break

            for link in links:
                if link not in collected:
                    collected.append(link)
                if amount and len(collected) >= amount:
                    break

            if amount and len(collected) >= amount:
                break

            if f"/page/{page_num + 1}/" not in html and "rel=\"next\"" not in html:
                break
            page_num += 1
            await asyncio.sleep(self._delay)

        return collected[:amount] if amount else collected

    # ------------------------------------------------------------------ search

    async def search(self, title: str, platform: str) -> list[ROMResult]:
        slug = self._slug(platform)
        query = quote_plus(title)
        urls = [
            f"{BASE_URL}/?s={query}",
            f"{BASE_URL}/search/?q={query}",
        ]

        results: list[ROMResult] = []
        seen: set[str] = set()

        for url in urls:
            html = await self._get_html(url)
            for card_title, href in parse_search_results(html, slug, BASE_URL):
                if href in seen:
                    continue
                seen.add(href)
                sim = title_matcher.score(title, card_title) / 100
                results.append(
                    ROMResult(
                        title=card_title,
                        platform=platform,
                        detail_url=href,
                        formats=_detect_formats(card_title),
                        score=sim,
                    )
                )

        results.sort(key=lambda r: r.score, reverse=True)
        return results

    # ------------------------------------------------------------- metadata

    async def fetch_metadata(self, detail_url: str) -> PageMetadata:
        html = await self._get_html(detail_url)
        return parse_detail_page(html, detail_url)

    async def download_art(
        self, metadata: PageMetadata, art_dir: Path, max_screenshots: int = 3
    ) -> dict[str, str]:
        art_dir.mkdir(parents=True, exist_ok=True)
        saved: dict[str, str] = {}

        if metadata.cover_url:
            cover = art_dir / "cover.png"
            await self._save_image(metadata.cover_url, cover, referer=metadata.cover_url)
            saved["cover"] = cover.name
            saved["box_front"] = cover.name

        for i, url in enumerate(metadata.screenshot_urls[:max_screenshots], 1):
            path = art_dir / f"screenshot_{i:02d}.png"
            try:
                await self._save_image(url, path, referer=url)
                saved[f"screenshot_{i:02d}"] = path.name
            except Exception:
                continue

        return saved

    # ------------------------------------------------------------------ fetch

    async def fetch(self, result: ROMResult, dest: Path) -> Path:
        dest.mkdir(parents=True, exist_ok=True)

        for attempt in range(1, self._max_retries + 1):
            try:
                return await self._do_fetch(result, dest)
            except Exception:
                if attempt == self._max_retries:
                    raise
                await asyncio.sleep(self._delay * attempt)
        raise RuntimeError("unreachable")

    async def _do_fetch(self, result: ROMResult, dest: Path) -> Path:
        metadata = await self.fetch_metadata(result.detail_url)
        dl_page = metadata.download_page_url
        if not dl_page:
            raise RuntimeError(f"No download link found on: {result.detail_url}")

        html = await self._get_html(dl_page)
        file_url = _extract_file_url(html, dl_page)
        if not file_url:
            raise RuntimeError(f"Could not extract file URL from: {dl_page}")

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

            assert self._browser is not None
            await self._browser.stream_to_file(
                file_url,
                out_path,
                referer=dl_page,
                on_progress=on_progress,
            )

        return out_path

    # ----------------------------------------------------------------- helpers

    async def _get_html(self, url: str) -> str:
        assert self._browser is not None
        return await self._browser.get_html(url)

    async def _save_image(self, url: str, dest: Path, referer: str | None = None) -> None:
        assert self._browser is not None
        data = await self._browser.download_bytes(url, referer=referer)
        dest.write_bytes(data)

    def _slug(self, platform: str) -> str:
        slug = PLATFORM_SLUGS.get(platform.lower())
        if not slug:
            raise ValueError(f"Unsupported platform: {platform}")
        return slug


def _detect_formats(text: str) -> list[str]:
    lowered = text.lower()
    found: list[str] = []
    if "pkg" in lowered:
        found.append("pkg")
    if "iso" in lowered:
        found.append("iso")
    if "bin" in lowered or "cue" in lowered:
        found.append("bin_cue")
    if "chd" in lowered:
        found.append("chd")
    return found or ["iso"]


def _extract_file_url(html: str, page_url: str) -> str | None:
    for ext in ROM_EXTENSIONS:
        pattern = re.escape(ext)
        m = re.search(rf'href="([^"]*{pattern}[^"]*)"', html, re.I)
        if m:
            url = m.group(1)
            if url.startswith("//"):
                return "https:" + url
            if url.startswith(("http://", "https://")):
                return url
            return urljoin(page_url, url)

    for ext in ROM_EXTENSIONS:
        pattern = re.escape(ext)
        m = re.search(rf'(https?://[^\s"\'<>]+{pattern})', html, re.I)
        if m:
            return m.group(1)

    return None
