"""Stream a YouTube video on loop via yt-dlp → mpv pipe.

Usage:
    yt-load "https://youtube.com/watch?v=..."
    yt-load --format "best[height<=480]" --no-loop "https://youtu.be/..."
"""

from __future__ import annotations

import argparse
import logging
import shlex
import signal
import subprocess
import sys

log = logging.getLogger(__name__)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Play a YouTube video on loop via yt-dlp → mpv pipe"
    )
    parser.add_argument("url", help="YouTube video URL")
    parser.add_argument(
        "-f", "--format",
        default="best[ext=mp4][height<=720]",
        help="yt-dlp format string (default: best[ext=mp4][height<=720])",
    )
    parser.add_argument(
        "--no-loop", action="store_true",
        help="Play once instead of looping",
    )
    parser.add_argument(
        "--mpv-args", default="",
        help="Extra arguments to pass to mpv (quoted string)",
    )
    return parser


def main() -> None:
    args = build_parser().parse_args()

    loop_flag: list[str] = [] if args.no_loop else ["--loop=inf"]
    extra_mpv = shlex.split(args.mpv_args) if args.mpv_args else []

    ytdlp_cmd = ["yt-dlp", "-f", args.format, "-o", "-", args.url]
    mpv_cmd = ["mpv", "--no-terminal", *loop_flag, "-", *extra_mpv]

    log.debug("yt-dlp: %s", " ".join(ytdlp_cmd))
    log.debug("mpv: %s", " ".join(mpv_cmd))

    ytdlp_proc = subprocess.Popen(
        ytdlp_cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    mpv_proc = subprocess.Popen(
        mpv_cmd,
        stdin=ytdlp_proc.stdout,
    )
    ytdlp_proc.stdout.close()

    def _kill(*_: object) -> None:
        mpv_proc.terminate()
        ytdlp_proc.terminate()

    signal.signal(signal.SIGINT, _kill)
    signal.signal(signal.SIGTERM, _kill)

    mpv_proc.wait()
    ytdlp_proc.kill()


if __name__ == "__main__":
    main()
