"""HyperVane scraper daemon entry-point."""

import asyncio
import logging
import signal
import sys

from scraper.ytdlp_worker import YtdlpWorker
from previewer.ffmpeg_pipeline import FfmpegPipeline
from ipc.socket_client import IPCSocketServer

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
log = logging.getLogger("hypervane.scraper")

SOCKET_PATH = "/tmp/hypervane-scraper.sock"


async def main() -> None:
    pipeline = FfmpegPipeline()
    worker = YtdlpWorker(pipeline=pipeline)
    server = IPCSocketServer(socket_path=SOCKET_PATH, worker=worker)

    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, lambda: asyncio.create_task(shutdown(server)))

    log.info("Scraper daemon starting on %s", SOCKET_PATH)
    await server.serve_forever()


async def shutdown(server: "IPCSocketServer") -> None:
    log.info("Shutting down scraper daemon…")
    await server.close()
    asyncio.get_running_loop().stop()


if __name__ == "__main__":
    asyncio.run(main())
