from __future__ import annotations

import asyncio
from pathlib import Path

import httpx


class BrowserSession:
    """Playwright browser with persistent profile for Cloudflare cookies."""

    def __init__(
        self,
        user_data_dir: Path,
        headless: bool = True,
        request_delay_ms: int = 1500,
    ) -> None:
        self._user_data_dir = user_data_dir.expanduser()
        self._headless = headless
        self._delay = request_delay_ms / 1000
        self._playwright = None
        self._context = None
        self._http: httpx.AsyncClient | None = None

    async def __aenter__(self) -> "BrowserSession":
        self._user_data_dir.mkdir(parents=True, exist_ok=True)
        self._http = httpx.AsyncClient(
            follow_redirects=True,
            timeout=httpx.Timeout(60.0),
            headers={
                "User-Agent": (
                    "Mozilla/5.0 (X11; Linux x86_64) "
                    "AppleWebKit/537.36 (KHTML, like Gecko) "
                    "Chrome/125.0.0.0 Safari/537.36"
                ),
            },
        )

        try:
            from playwright.async_api import async_playwright
            from playwright_stealth import Stealth

            self._playwright = await async_playwright().start()
            self._context = await self._playwright.chromium.launch_persistent_context(
                user_data_dir=str(self._user_data_dir),
                headless=self._headless,
                args=["--disable-blink-features=AutomationControlled"],
            )
            stealth = Stealth()
            await stealth.apply_stealth_async(self._context)
        except Exception:
            self._context = None

        return self

    async def __aexit__(self, *_) -> None:
        if self._http:
            await self._http.aclose()
        if self._context:
            await self._context.close()
        if self._playwright:
            await self._playwright.stop()

    async def get_html(self, url: str) -> str:
        if self._http:
            try:
                resp = await self._http.get(url)
                if resp.status_code == 200 and "Just a moment" not in resp.text:
                    return resp.text
            except httpx.HTTPError:
                pass

        if not self._context:
            raise RuntimeError(
                "romsfun.com blocked plain HTTP requests and Playwright is unavailable"
            )

        page = await self._context.new_page()
        try:
            await page.goto(url, wait_until="domcontentloaded", timeout=60_000)
            await asyncio.sleep(self._delay)
            return await page.content()
        finally:
            await page.close()

    async def download_bytes(self, url: str, referer: str | None = None) -> bytes:
        headers = {}
        if referer:
            headers["Referer"] = referer

        if self._http:
            try:
                resp = await self._http.get(url, headers=headers)
                resp.raise_for_status()
                return resp.content
            except httpx.HTTPError:
                pass

        if not self._context:
            raise RuntimeError("Unable to download asset: no browser session")

        page = await self._context.new_page()
        try:
            resp = await page.request.get(url, headers=headers)
            if not resp.ok:
                raise RuntimeError(f"Download failed ({resp.status}): {url}")
            return await resp.body()
        finally:
            await page.close()

    async def stream_to_file(
        self,
        url: str,
        dest: Path,
        referer: str | None = None,
        on_progress=None,
    ) -> None:
        headers = {"Referer": referer} if referer else {}
        if self._http:
            try:
                async with self._http.stream("GET", url, headers=headers) as resp:
                    resp.raise_for_status()
                    total = int(resp.headers.get("content-length", 0))
                    downloaded = 0
                    with dest.open("wb") as f:
                        async for chunk in resp.aiter_bytes(chunk_size=1 << 16):
                            f.write(chunk)
                            downloaded += len(chunk)
                            if on_progress:
                                on_progress(downloaded, total)
                return
            except httpx.HTTPError:
                pass

        data = await self.download_bytes(url, referer=referer)
        dest.write_bytes(data)
        if on_progress:
            on_progress(len(data), len(data))
