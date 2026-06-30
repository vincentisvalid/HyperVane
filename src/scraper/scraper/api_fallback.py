"""Fallback scraping from community databases (e.g. ScreenScraper, IGDB)."""

from __future__ import annotations

import logging
from dataclasses import dataclass

import aiohttp

from scraper.rate_limiter import RateLimiter

log = logging.getLogger(__name__)

_SCREENSCRAPER_BASE = "https://www.screenscraper.fr/api2"


@dataclass
class GameAssets:
    rom_id: str
    video_url: str | None = None
    boxart_url: str | None = None
    title: str | None = None


class ApiFallbackScraper:
    def __init__(self, api_user: str, api_pass: str) -> None:
        self._user = api_user
        self._pass = api_pass
        self._limiter = RateLimiter(rate=5, period=1.0)

    async def fetch_assets(
        self,
        session: aiohttp.ClientSession,
        crc32: str,
        platform_id: int,
    ) -> GameAssets | None:
        await self._limiter.acquire()
        params = {
            "devid": "hypervane",
            "devpassword": "",
            "softname": "hypervane",
            "ssid": self._user,
            "sspassword": self._pass,
            "crc": crc32,
            "systemeid": platform_id,
            "output": "json",
        }
        try:
            async with session.get(
                f"{_SCREENSCRAPER_BASE}/jeuInfos.php",
                params=params,
                timeout=aiohttp.ClientTimeout(total=10),
            ) as resp:
                resp.raise_for_status()
                data = await resp.json()
                return self._parse(data)
        except Exception:
            log.warning("ScreenScraper lookup failed for crc=%s", crc32)
            return None

    @staticmethod
    def _parse(data: dict) -> GameAssets | None:
        game = data.get("response", {}).get("jeu")
        if not game:
            return None
        rom_id = str(game.get("id", ""))
        medias = game.get("medias", [])
        video_url = next(
            (m["url"] for m in medias if m.get("type") == "video"),
            None,
        )
        return GameAssets(rom_id=rom_id, video_url=video_url)
