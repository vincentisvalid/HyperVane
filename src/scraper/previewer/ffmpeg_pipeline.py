"""FFmpeg pipeline that encodes raw footage into silent, loopable preview clips."""

from __future__ import annotations

import asyncio
import logging
from pathlib import Path

log = logging.getLogger(__name__)

# yuv420p is required for H.264 baseline — without it, RGB/4:4:4 input
# causes "baseline profile doesn't support 4:4:4" and the encode fails.
_ENCODE_ARGS = [
    "-vf", "scale=-2:480",
    "-pix_fmt", "yuv420p",
    "-r", "30",
    "-c:v", "libx264",
    "-profile:v", "baseline",
    "-level", "3.0",
    "-preset", "fast",
    "-crf", "28",
    "-an",
    "-movflags", "+faststart",
    "-t", "30",
]

_ENCODE_TIMEOUT = 300.0  # seconds; kills a hung ffmpeg rather than blocking forever


class FfmpegPipeline:
    async def encode_preview(self, source: Path, out_dir: Path) -> Path:
        dest = out_dir / (source.stem + "_preview.mp4")
        cmd = [
            "ffmpeg", "-y",
            "-i", str(source),
            *_ENCODE_ARGS,
            str(dest),
        ]
        proc = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.DEVNULL,
            stderr=asyncio.subprocess.PIPE,
        )
        try:
            _, stderr = await asyncio.wait_for(
                proc.communicate(), timeout=_ENCODE_TIMEOUT
            )
        except asyncio.TimeoutError:
            proc.kill()
            await proc.wait()
            raise RuntimeError(
                f"ffmpeg timed out after {_ENCODE_TIMEOUT}s encoding {source}"
            )

        if proc.returncode != 0:
            raise RuntimeError(
                f"ffmpeg failed (rc={proc.returncode}): "
                f"{stderr.decode(errors='replace')}"
            )

        log.debug("Encoded preview: %s", dest)
        return dest
