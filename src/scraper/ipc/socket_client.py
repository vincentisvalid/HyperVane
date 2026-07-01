"""Unix domain socket server — receives scrape requests from the UI."""

from __future__ import annotations

import asyncio
import logging
import re
import struct
from pathlib import Path
from typing import TYPE_CHECKING

import hypervane_pb2 as pb

if TYPE_CHECKING:
    from scraper.ytdlp_worker import YtdlpWorker, ScrapeJob

log = logging.getLogger(__name__)

_FRAME_HEADER = struct.Struct(">I")
MAIN_SOCKET_PATH = "/tmp/hypervane.sock"
PREVIEW_DIR = Path("/tmp/hypervane-previews")


def _pack(envelope: pb.Envelope) -> bytes:
    data = envelope.SerializeToString()
    return _FRAME_HEADER.pack(len(data)) + data


class IPCSocketServer:
    def __init__(self, socket_path: str, worker: YtdlpWorker) -> None:
        self._socket_path = socket_path
        self._worker = worker
        self._server: asyncio.Server | None = None
        worker._on_preview_ready = self._on_preview_ready

    async def serve_forever(self) -> None:
        Path(self._socket_path).unlink(missing_ok=True)
        self._server = await asyncio.start_unix_server(
            self._handle_client, path=self._socket_path
        )
        async with self._server:
            await self._server.serve_forever()

    async def close(self) -> None:
        if self._server:
            self._server.close()
            await self._server.wait_closed()

    async def _handle_client(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        peer = writer.get_extra_info("peername", "<unknown>")
        log.info("Client connected: %s", peer)
        try:
            async for envelope in self._read_envelopes(reader):
                await self._dispatch(envelope, writer)
        except asyncio.IncompleteReadError:
            pass
        finally:
            writer.close()
            await writer.wait_closed()
            log.info("Client disconnected: %s", peer)

    async def _dispatch(
        self,
        envelope: pb.Envelope,
        writer: asyncio.StreamWriter,
    ) -> None:
        if envelope.HasField("launch_request"):
            req = envelope.launch_request
            title = _extract_title(req.rom_path, req.rom_id)
            log.info("Scraping preview for %s (%s)", req.rom_id, title)

            PREVIEW_DIR.mkdir(parents=True, exist_ok=True)
            from scraper.ytdlp_worker import ScrapeJob

            # Skip if cached preview already exists
            cached = PREVIEW_DIR / f"{req.rom_id}_preview.mp4"
            if cached.exists():
                log.info("Cache HIT — sending preview_ready immediately for %s", req.rom_id)
                await self._on_preview_ready(req.rom_id, str(cached))
                return

            job = ScrapeJob(
                rom_id=req.rom_id,
                query=f"{title} gameplay",
                out_dir=PREVIEW_DIR,
            )
            self._worker.enqueue(job)

    async def _on_preview_ready(self, rom_id: str, file_path: str) -> None:
        env = pb.Envelope()
        env.preview_ready.rom_id = rom_id
        env.preview_ready.file_path = file_path
        try:
            _, writer = await asyncio.open_unix_connection(MAIN_SOCKET_PATH)
            writer.write(_pack(env))
            await writer.drain()
            writer.close()
        except Exception:
            log.warning("Could not send preview_ready to main IPC", exc_info=True)

    @staticmethod
    async def _read_envelopes(reader: asyncio.StreamReader):
        while True:
            header = await reader.readexactly(_FRAME_HEADER.size)
            (length,) = _FRAME_HEADER.unpack(header)
            payload = await reader.readexactly(length)
            env = pb.Envelope()
            env.ParseFromString(payload)
            yield env


def _extract_title(rom_path: str, rom_id: str) -> str:
    m = re.search(r"/([^/]+)\.rom$", rom_path)
    if m:
        return m.group(1)
    return rom_id.replace("-", " ").title()
