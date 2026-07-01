"""FFmpeg pipeline — fast encode with audio, no unnecessary re-encode."""

from __future__ import annotations

import asyncio
import logging
from pathlib import Path

log = logging.getLogger(__name__)

# Fast path: copy video/audio streams, only remux + trim + add faststart.
# Falls back to re-encode if the source codec isn't supported by the target.
_COPY_ARGS = [
    "-t", "30",
    "-c:v", "copy",
    "-c:a", "copy",
    "-movflags", "+faststart",
]

# Slow path: re-encode with ultrafast H.264 + AAC.
_ENCODE_ARGS = [
    "-vf", "scale=-2:480",
    "-pix_fmt", "yuv420p",
    "-r", "30",
    "-c:v", "libx264",
    "-profile:v", "baseline",
    "-level", "3.0",
    "-preset", "ultrafast",
    "-crf", "30",
    "-c:a", "aac",
    "-ar", "44100",
    "-ac", "2",
    "-movflags", "+faststart",
    "-t", "30",
]

_TIMEOUT = 120.0

class FfmpegPipeline:
    async def encode_preview(self, source: Path, out_dir: Path, rom_id: str, max_duration: int = 30) -> Path:
        dest = out_dir / f"{rom_id}_preview.mp4"

        # Try fast copy-path first (instant remux + trim)
        ok = await self._try_copy(source, dest, max_duration)
        if ok:
            log.debug("Fast path (copy) for %s → %s", source, dest)
            return dest

        # Fall back to re-encode
        ok = await self._try_encode(source, dest, max_duration)
        if ok:
            log.debug("Re-encode path for %s → %s", source, dest)
            return dest

        raise RuntimeError(f"Both copy and encode paths failed for {source}")

    # ------------------------------------------------------------------
    # Fast copy path
    # ------------------------------------------------------------------
    async def _try_copy(self, source: Path, dest: Path, duration: int) -> bool:
        cmd = [
            "ffmpeg", "-y",
            "-i", str(source),
            "-t", str(duration),
            *_COPY_ARGS,
            str(dest),
        ]
        rc = await self._run(cmd)
        if rc != 0 or not dest.exists():
            dest.unlink(missing_ok=True)
            return False
        return True

    # ------------------------------------------------------------------
    # Re-encode path (ultrafast H.264 + AAC)
    # ------------------------------------------------------------------
    async def _try_encode(self, source: Path, dest: Path, duration: int) -> bool:
        cmd = [
            "ffmpeg", "-y",
            "-i", str(source),
            *_ENCODE_ARGS,
            str(dest),
        ]
        rc = await self._run(cmd)
        if rc != 0 or not dest.exists():
            dest.unlink(missing_ok=True)
            return False
        return True

    # ------------------------------------------------------------------
    # Pipe from yt-dlp stdout → ffmpeg (zero temp files)
    # ------------------------------------------------------------------
    async def pipe_from_ytdlp(
        self,
        ytdlp_args: list[str],
        dest: Path,
        duration: int = 30,
        format_spec: str = "best[ext=mp4][height<=720]",
    ) -> bool:
        """Launch yt-dlp piping stdout directly into ffmpeg.

        *ytdlp_args* should NOT include ``-o -``; this method adds it.
        *format_spec* is passed as ``-f`` to yt-dlp (single-stream formats
        required for reliable piping).
        Returns ``True`` if the pipeline succeeded.
        """
        dest.parent.mkdir(parents=True, exist_ok=True)

        ytdlp_proc = await asyncio.create_subprocess_exec(
            "yt-dlp", "-f", format_spec, "-o", "-", *ytdlp_args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
        )

        ffmpeg_cmd = [
            "ffmpeg", "-y",
            "-i", "pipe:0",
            *_ENCODE_ARGS,
            str(dest),
        ]
        ffmpeg_proc = await asyncio.create_subprocess_exec(
            *ffmpeg_cmd,
            stdin=ytdlp_proc.stdout,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE,
        )

        try:
            _, stderr = await asyncio.wait_for(
                ffmpeg_proc.communicate(), timeout=_TIMEOUT
            )
        except asyncio.TimeoutError:
            ytdlp_proc.kill()
            ffmpeg_proc.kill()
            await asyncio.gather(ytdlp_proc.wait(), ffmpeg_proc.wait())
            log.warning("pipe timed out after %ss", _TIMEOUT)
            dest.unlink(missing_ok=True)
            return False

        rc = ffmpeg_proc.returncode
        if rc != 0 or not dest.exists():
            dest.unlink(missing_ok=True)
            log.warning("pipe failed (rc=%d): %s", rc, stderr.decode(errors="replace")[:200])
            return False

        return True

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    @staticmethod
    async def _run(cmd: list[str]) -> int:
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.DEVNULL,
        )
        try:
            await asyncio.wait_for(proc.wait(), timeout=_TIMEOUT)
        except asyncio.TimeoutError:
            proc.kill()
            await proc.wait()
            return -1
        return proc.returncode
