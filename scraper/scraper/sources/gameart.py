from __future__ import annotations

import asyncio
from pathlib import Path
from urllib.parse import quote

import httpx
from rich.console import Console

from .base import ArtResult
from ..platforms import get as get_platform

console = Console()

_SCREENSCRAPER_BASE = "https://www.screenscraper.fr/api2"
_THEGAMESDB_BASE = "https://api.thegamesdb.net/v1"

_REGION_PRIORITY = {"us": ["us", "wor", "eu", "jp"], "eu": ["eu", "wor", "us", "jp"], "jp": ["jp", "wor", "us", "eu"]}

ART_TYPE_MAP = {
    "box_front": ["box-2D", "box-texture"],
    "box_back": ["box-2D-back"],
    "box_3d": ["box-3D"],
    "disc": ["wheel", "disc"],
    "screenshot": ["ss", "sstitle"],
    "banner": ["steamgrid", "fanart"],
    "title_screen": ["sstitle"],
    "video": ["video"],
}


class GameArtSource:
    def __init__(
        self,
        screenscraper_user: str = "",
        screenscraper_pass: str = "",
        thegamesdb_key: str = "",
        igdb_client_id: str = "",
        igdb_client_secret: str = "",
        preferred_region: str = "us",
        max_screenshots: int = 3,
    ) -> None:
        self._ss_user = screenscraper_user
        self._ss_pass = screenscraper_pass
        self._tgdb_key = thegamesdb_key
        self._igdb_id = igdb_client_id
        self._igdb_secret = igdb_client_secret
        self._region = preferred_region
        self._max_screenshots = max_screenshots
        self._client = httpx.AsyncClient(timeout=30, follow_redirects=True)

    async def __aenter__(self) -> "GameArtSource":
        return self

    async def __aexit__(self, *_) -> None:
        await self._client.aclose()

    # ------------------------------------------------------------------ search

    async def fetch_art(self, title: str, platform: str, dest: Path) -> ArtResult:
        dest.mkdir(parents=True, exist_ok=True)
        result = ArtResult(title=title, platform=platform)

        if self._ss_user:
            try:
                result = await self._screenscraper_fetch(title, platform, dest, result)
                return result
            except Exception as exc:
                console.print(f"[yellow]ScreenScraper failed ({exc}), trying TheGamesDB[/]")

        if self._tgdb_key:
            try:
                result = await self._thegamesdb_fetch(title, platform, dest, result)
                return result
            except Exception as exc:
                console.print(f"[yellow]TheGamesDB failed ({exc})[/]")

        return result

    # ------------------------------------------------------------- screenscraper

    async def _screenscraper_fetch(
        self, title: str, platform: str, dest: Path, result: ArtResult
    ) -> ArtResult:
        pinfo = get_platform(platform)
        params = {
            "devid": "hypervane",
            "devpassword": "",
            "softname": "hypervane-scraper",
            "output": "json",
            "ssid": self._ss_user,
            "sspassword": self._ss_pass,
            "romtype": "rom",
            "systemeid": str(pinfo.screenscraper_id),
            "romnom": quote(title),
        }
        url = f"{_SCREENSCRAPER_BASE}/jeuInfos.php"
        resp = await self._client.get(url, params=params)
        resp.raise_for_status()
        data = resp.json()

        game = data.get("response", {}).get("jeu", {})
        if not game:
            return result

        medias = game.get("medias", [])
        region_order = _REGION_PRIORITY.get(self._region, ["us", "wor", "eu"])

        async def best_media(types: list[str]) -> str | None:
            for rtype in types:
                for region in region_order:
                    for m in medias:
                        if m.get("type") == rtype and m.get("region", "wor") == region:
                            return m.get("url")
                for m in medias:
                    if m.get("type") == rtype:
                        return m.get("url")
            return None

        async def dl(url: str, name: str) -> str | None:
            if not url:
                return None
            path = dest / name
            if path.exists():
                return str(path)
            try:
                r = await self._client.get(url)
                r.raise_for_status()
                path.write_bytes(r.content)
                return str(path)
            except Exception:
                return None

        result.box_front = await dl(await best_media(ART_TYPE_MAP["box_front"]), "box_front.png")
        result.box_back = await dl(await best_media(ART_TYPE_MAP["box_back"]), "box_back.png")
        result.disc = await dl(await best_media(ART_TYPE_MAP["disc"]), "disc.png")
        result.banner = await dl(await best_media(ART_TYPE_MAP["banner"]), "banner.png")
        result.title_screen = await dl(await best_media(ART_TYPE_MAP["title_screen"]), "title_screen.png")

        ss_urls = [m.get("url") for m in medias if m.get("type") in ("ss",) and m.get("url")]
        for i, ss_url in enumerate(ss_urls[: self._max_screenshots]):
            path = await dl(ss_url, f"screenshot_{i + 1:02d}.png")
            if path:
                result.screenshots.append(path)

        return result

    # -------------------------------------------------------------- thegamesdb

    async def _thegamesdb_fetch(
        self, title: str, platform: str, dest: Path, result: ArtResult
    ) -> ArtResult:
        pinfo = get_platform(platform)
        search_resp = await self._client.get(
            f"{_THEGAMESDB_BASE}/Games/ByGameName",
            params={
                "apikey": self._tgdb_key,
                "name": title,
                "filter[platform]": pinfo.thegamesdb_id,
                "fields": "players,publishers,genres,overview,last_updated,rating,platform,coop,youtube,os,processor,ram,hdd,video,sound,alternates",
                "include": "boxart,platform",
            },
        )
        search_resp.raise_for_status()
        data = search_resp.json()

        games = data.get("data", {}).get("games", [])
        if not games:
            return result
        game = games[0]
        game_id = game["id"]

        base_url = data.get("include", {}).get("boxart", {}).get("base_url", {}).get("original", "")
        images = data.get("include", {}).get("boxart", {}).get("data", {}).get(str(game_id), [])

        async def dl(url: str, name: str) -> str | None:
            if not url:
                return None
            full = base_url + url if not url.startswith("http") else url
            path = dest / name
            if path.exists():
                return str(path)
            try:
                r = await self._client.get(full)
                r.raise_for_status()
                path.write_bytes(r.content)
                return str(path)
            except Exception:
                return None

        for img in images:
            side = img.get("side", "")
            img_url = img.get("filename", "")
            if side == "front" and not result.box_front:
                result.box_front = await dl(img_url, "box_front.png")
            elif side == "back" and not result.box_back:
                result.box_back = await dl(img_url, "box_back.png")

        return result
