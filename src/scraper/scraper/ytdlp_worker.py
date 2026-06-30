"""Async yt-dlp orchestrator for fetching gameplay footage."""

from __future__ import annotations

import asyncio
import logging
import shutil
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING

import yt_dlp

if TYPE_CHECKING:
    from previewer.ffmpeg_pipeline import FfmpegPipeline

log = logging.getLogger(__name__)

_DEFAULT_MAX_DURATION = 60
_YTDLP_BASE_OPTS: dict = {
    "quiet": True,
    "no_warnings": True,
    "format": "bestvideo[ext=mp4][height<=720]+bestaudio/best[height<=720]",
    "merge_output_format": "mp4",
}


@dataclass
class ScrapeJob:
    rom_id: str
    query: str
    out_dir: Path
    max_duration: int = _DEFAULT_MAX_DURATION


class YtdlpWorker:
    def __init__(self, pipeline: FfmpegPipeline, concurrency: int = 3) -> None:
        self._pipeline = pipeline
        self._sem = asyncio.Semaphore(concurrency)
        self._queue: asyncio.Queue[ScrapeJob] = asyncio.Queue()

    def enqueue(self, job: ScrapeJob) -> None:
        self._queue.put_nowait(job)

    async def run(self) -> None:
        workers = [asyncio.create_task(self._worker()) for _ in range(3)]
        await asyncio.gather(*workers)

    async def _worker(self) -> None:
        while True:
            job = await self._queue.get()
            async with self._sem:
                try:
                    await self._process(job)
                except Exception:
                    log.exception("Scrape failed for rom_id=%s", job.rom_id)
                finally:
                    self._queue.task_done()

    async def _process(self, job: ScrapeJob) -> Path:
        loop = asyncio.get_running_loop()
        raw_path = await loop.run_in_executor(None, self._download, job)
        try:
            preview_path = await self._pipeline.encode_preview(raw_path, job.out_dir)
        finally:
            # Always remove the raw download — it's a temporary artifact and
            # can be hundreds of MB.  Do this whether encode succeeded or failed.
            raw_path.unlink(missing_ok=True)
        log.info("Preview ready: rom_id=%s path=%s", job.rom_id, preview_path)
        return preview_path

    def _download(self, job: ScrapeJob) -> Path:
        with tempfile.TemporaryDirectory() as tmpdir:
            tmpdir_path = Path(tmpdir)
            opts = {
                **_YTDLP_BASE_OPTS,
                "outtmpl": str(tmpdir_path / "%(id)s.%(ext)s"),
                "match_filter": yt_dlp.utils.match_filter_func(
                    f"duration < {job.max_duration}"
                ),
            }
            with yt_dlp.YoutubeDL(opts) as ydl:
                ydl.extract_info(f"ytsearch1:{job.query}", download=True)

            # yt-dlp merges streams and renames the file, so prepare_filename()
            # returns a stale pre-merge name.  Scan for the actual output instead.
            mp4_files = sorted(tmpdir_path.glob("*.mp4"))
            candidates = mp4_files or sorted(tmpdir_path.iterdir())
            if not candidates:
                raise RuntimeError(f"yt-dlp produced no output for query={job.query!r}")

            src = candidates[0]
            dest = job.out_dir / src.name
            # shutil.move handles cross-device moves (e.g. /tmp → /home).
            shutil.move(str(src), dest)
            return dest
