"""Search YouTube for gameplay videos.

Provides both a CLI (``yt-gameplay-search``) and a Python API
(``search()``) that returns structured results.

Usage:
    yt-gameplay-search "half life"
    yt-gameplay-search -n 10 --json "metroid prime"
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import sys
from dataclasses import asdict, dataclass
from typing import Any

import yt_dlp

log = logging.getLogger(__name__)


# ── Data ──────────────────────────────────────────────────────────────────────


@dataclass
class GameplayResult:
    title: str
    url: str
    video_id: str
    duration: int
    view_count: int | None = None
    channel: str | None = None
    upload_date: str | None = None


# ── Public API ────────────────────────────────────────────────────────────────


_SEARCH_OPTS: dict[str, Any] = {
    "quiet": True,
    "no_warnings": True,
    "extract_flat": "in_playlist",
}


def search(
    query: str,
    count: int = 5,
    max_duration: int = 600,
    min_duration: int = 10,
    prefer_longer: bool = False,
) -> list[GameplayResult]:
    """Search YouTube for gameplay videos matching *query*.

    Automatically appends "gameplay" to the search to filter for
    actual play-through footage.  Results are filtered by duration
    and sorted by relevance (or duration if *prefer_longer* is set).
    """
    search_query = _build_query(query)
    with yt_dlp.YoutubeDL(_SEARCH_OPTS) as ydl:
        info = ydl.extract_info(f"ytsearch{count}:{search_query}", download=False)

    entries: list[dict[str, Any]] = info.get("entries") or []
    results: list[GameplayResult] = []
    for entry in entries:
        duration = entry.get("duration") or 0
        if duration < min_duration or duration > max_duration:
            continue
        results.append(
            GameplayResult(
                title=entry.get("title", ""),
                url=f"https://youtube.com/watch?v={entry.get('id', '')}",
                video_id=entry.get("id", ""),
                duration=duration,
                view_count=entry.get("view_count"),
                channel=entry.get("channel") or entry.get("uploader"),
                upload_date=entry.get("upload_date"),
            )
        )

    if prefer_longer:
        results.sort(key=lambda r: r.duration, reverse=True)

    return results


async def search_async(
    query: str,
    count: int = 5,
    max_duration: int = 600,
    min_duration: int = 10,
    prefer_longer: bool = False,
) -> list[GameplayResult]:
    """Async wrapper around :func:`search` (runs in thread executor)."""
    from functools import partial

    loop = asyncio.get_running_loop()
    fn = partial(search, query, count, max_duration, min_duration, prefer_longer)
    return await loop.run_in_executor(None, fn)


# ── CLI ───────────────────────────────────────────────────────────────────────


def _build_query(query: str) -> str:
    """Append *gameplay* unless the query already includes it."""
    ql = query.lower()
    if "gameplay" in ql or "playthrough" in ql or "longplay" in ql:
        return query
    return f"{query} gameplay"


def _duration_str(seconds: int) -> str:
    m, s = divmod(seconds, 60)
    h, m = divmod(m, 60)
    if h:
        return f"{h}h{m:02d}m{s:02d}s"
    return f"{m}m{s:02d}s"


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Search YouTube for gameplay videos",
    )
    parser.add_argument("query", help="Game name to search for")
    parser.add_argument(
        "-n", "--count",
        type=int,
        default=5,
        help="Number of results to fetch (default: 5)",
    )
    parser.add_argument(
        "--max-duration",
        type=int,
        default=600,
        help="Max video duration in seconds (default: 600)",
    )
    parser.add_argument(
        "--min-duration",
        type=int,
        default=10,
        help="Min video duration in seconds (default: 10)",
    )
    parser.add_argument(
        "--json", action="store_true",
        help="Output as JSON",
    )
    parser.add_argument(
        "--raw", action="store_true",
        help="Dump raw yt-dlp response (debug)",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()

    if args.raw:
        raw_opts: dict[str, Any] = {
            "quiet": True,
            "no_warnings": True,
            "extract_flat": "in_playlist",
        }
        query = _build_query(args.query)
        with yt_dlp.YoutubeDL(raw_opts) as ydl:
            info = ydl.extract_info(
                f"ytsearch{args.count}:{query}", download=False,
            )
        json.dump(info, sys.stdout, indent=2, default=str)
        return

    results = search(
        query=args.query,
        count=args.count,
        max_duration=args.max_duration,
        min_duration=args.min_duration,
    )

    if args.json:
        json.dump([asdict(r) for r in results], sys.stdout, indent=2)
        return

    if not results:
        print(f"⚠  No gameplay videos found for '{args.query}'")
        sys.exit(1)

    for i, r in enumerate(results, 1):
        views = f"{r.view_count:,}" if r.view_count else "?"
        print(f"{i:>2}. {r.title}")
        print(f"     {r.url}")
        print(
            f"     {_duration_str(r.duration)}  "
            f"│  {views} views  "
            f"│  {r.channel or '?'}"
        )
        print()


if __name__ == "__main__":
    main()
