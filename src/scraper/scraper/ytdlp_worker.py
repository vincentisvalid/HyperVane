"""High-speed yt-dlp orchestrator — gameplay previews with sound, zero temp I/O."""

from __future__ import annotations

import asyncio
import logging
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import TYPE_CHECKING, Awaitable, Callable

import yt_dlp
from yt_gameplay_search import search as search_gameplay

if TYPE_CHECKING:
    from previewer.ffmpeg_pipeline import FfmpegPipeline

log = logging.getLogger(__name__)

_MAX_DURATION = 60
_PREVIEW_DIR = Path("/tmp/hypervane-previews")
# Single-stream format (no merge needed) — ideal for piping.
_FORMAT = "best[ext=mp4][height<=720]"
_YTDLP_OPTS: dict = {
    "quiet": True,
    "no_warnings": True,
    "format": _FORMAT,
    "merge_output_format": "mp4",
}


@dataclass
class ScrapeJob:
    rom_id: str
    query: str
    out_dir: Path = _PREVIEW_DIR
    max_duration: int = _MAX_DURATION


PreviewCallback = Callable[[str, str], Awaitable[None]]


class YtdlpWorker:
    def __init__(
        self,
        pipeline: FfmpegPipeline,
        concurrency: int = 3,
        on_preview_ready: PreviewCallback | None = None,
    ) -> None:
        self._pipeline = pipeline
        self._sem = asyncio.Semaphore(concurrency)
        self._queue: asyncio.Queue[ScrapeJob] = asyncio.Queue()
        self._on_preview_ready = on_preview_ready

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
                    preview_path = await self._process(job)
                    log.info("Preview ready: rom_id=%s path=%s", job.rom_id, preview_path)
                    if self._on_preview_ready is not None:
                        asyncio.create_task(
                            self._on_preview_ready(job.rom_id, str(preview_path))
                        )
                except Exception:
                    log.exception("Scrape failed for rom_id=%s", job.rom_id)
                finally:
                    self._queue.task_done()

    async def _process(self, job: ScrapeJob) -> Path:
        # ── Cache hit? Return instantly ────────────────────────────────
        job.out_dir.mkdir(parents=True, exist_ok=True)
        cached = job.out_dir / f"{job.rom_id}_preview.mp4"
        if cached.exists():
            log.info("Cache HIT for %s", job.rom_id)
            return cached

        # ── Find best gameplay video via yt_gameplay_search ───────────
        loop = asyncio.get_running_loop()
        results = await loop.run_in_executor(
            None, search_gameplay, job.query, 5, job.max_duration, 15,
        )

        if not results:
            log.warning("No gameplay results for %s, using fallback", job.rom_id)
            return self._fallback_video(job)

        best = results[0]
        log.info("Selected: %s (%ds, %s)", best.title, best.duration, best.url)

        # ── Fast path: pipe yt-dlp stdout → ffmpeg ────────────────────
        try:
            ok = await self._pipeline.pipe_from_ytdlp(
                ytdlp_args=[best.url],
                dest=cached,
                format_spec=_FORMAT,
            )
            if ok:
                return cached
        except Exception:
            log.warning("Pipe path failed for %s, falling back to temp path", job.rom_id, exc_info=True)

        # ── Fallback: download to temp, then copy/re-encode ────────────
        raw = await loop.run_in_executor(None, self._download_url, job, best.url)
        try:
            preview = await self._pipeline.encode_preview(raw, job.out_dir, job.rom_id)
        finally:
            raw.unlink(missing_ok=True)
        return preview

    def _download_url(self, job: ScrapeJob, url: str) -> Path:
        """Download *url* to a temp directory, return path to the MP4."""
        with tempfile.TemporaryDirectory() as tmpdir:
            opts: dict = {
                **_YTDLP_OPTS,
                "outtmpl": str(Path(tmpdir) / "%(id)s.%(ext)s"),
            }
            try:
                with yt_dlp.YoutubeDL(opts) as ydl:
                    ydl.extract_info(url, download=True)
            except Exception as e:
                log.warning("yt-dlp download failed for %s: %s", url, e)
                return self._fallback_video(job)

            mp4s = sorted(Path(tmpdir).glob("*.mp4"))
            candidates = mp4s or sorted(Path(tmpdir).iterdir())
            if not candidates:
                log.warning("yt-dlp gave no output for %s", url)
                return self._fallback_video(job)

            src = candidates[0]
            dest = job.out_dir / src.name
            shutil.move(str(src), str(dest))
            return dest

    # ------------------------------------------------------------------
    # Fallback: 30 s colored background with audible 440 Hz tone
    # ------------------------------------------------------------------
    @staticmethod
    def _fallback_video(job: ScrapeJob) -> Path:
        """Generate a 30 s 720p preview with audio beep as fallback."""
        job.out_dir.mkdir(parents=True, exist_ok=True)
        dest = job.out_dir / f"{job.rom_id}_fallback.mp4"
        if dest.exists():
            return dest

        # Color bars + audio tone — keeps the pipeline exercisable
        cmd = [
            "ffmpeg", "-y",
            "-f", "lavfi", "-i",
            "color=c=#0066cc:s=1280x720:d=30:r=30",
            "-f", "lavfi", "-i",
            "sine=frequency=440:duration=30",
            "-c:v", "libx264", "-preset", "ultrafast",
            "-profile:v", "baseline", "-level", "3.0",
            "-pix_fmt", "yuv420p",
            "-crf", "28",
            "-c:a", "aac",
            "-ar", "44100",
            "-shortest",
            str(dest),
        ]
        subprocess.run(cmd, capture_output=True)
        return dest
